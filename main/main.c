#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include "network.h"
#include "config.h"
#include "status_led.h"
#include "temperature_ds18b20.h"
#include "temperature_dht.h"
#include "tcp_server.h"
#include "xpl_server.h"
#include "relays.h"
#include "contacts.h"

#define LED_PIN 2

static char *TAG = "main";

static struct mea_config_s *mea_config = NULL;


/*
 * Contacts data and callbacks
 */
void update_contact_callback(int8_t v, int8_t prev, int8_t id, void *data);

#define NB_CONTACTS 4

struct contact_s my_contacts[NB_CONTACTS] = {
   { .last_state=-1, .gpio_pin=16, .name="Contact 1", .callback=update_contact_callback, .status=1 },
   { .last_state=-1, .gpio_pin=17, .name="Contact 2", .callback=update_contact_callback, .status=1 },
   { .last_state=-1, .gpio_pin=18, .name="Contact 3", .callback=update_contact_callback, .status=1 },
   { .last_state=-1, .gpio_pin=19, .name="Contact 4", .callback=update_contact_callback, .status=1 }
};


void update_contact_callback(int8_t v, int8_t prev, int8_t id, void *data)
{
   homekit_characteristic_t *c=(homekit_characteristic_t *)data;
   c->value.uint8_value = (uint8_t)v;

   if(v!=prev) {
      char device[3]="iX";
      device[1]='0'+id;
      xpl_send_current_hl("trig", device, v);
      homekit_characteristic_notify(c, HOMEKIT_UINT8(v));
   }
}


homekit_value_t contact_state_getter(homekit_characteristic_t *c) {
   for(int8_t i=0;i<NB_CONTACTS; i++) {
      homekit_characteristic_t *_c = (homekit_characteristic_t *)(my_contacts[i].contact);
      if(_c->id == c->id) {
         if(my_contacts[i].last_state==-1) {
            my_contacts[i].last_state=gpio_get_level(my_contacts[i].gpio_pin);
         }
         return HOMEKIT_UINT8(my_contacts[i].last_state == 0 ? 0 : 1);
      }
   }
   return HOMEKIT_UINT8(0);
}


/*
 * dht temperature/humidity data and callbacks
 */
void update_temperature_dht_callback(float t, float prev_t, void *data)
{
   homekit_characteristic_t *c = (homekit_characteristic_t *)data;
   c->value.float_value = t;

   if(t!=prev_t) {
      homekit_characteristic_notify(c, HOMEKIT_FLOAT(t));
      xpl_send_current_float("trig", "t0", t);
   }
}


void update_humidity_dht_callback(float h, float prev_h, void *data)
{
   homekit_characteristic_t *c = (homekit_characteristic_t *)data;
   c->value.float_value = h;

   if(h!=prev_h) {
      homekit_characteristic_notify(c, HOMEKIT_FLOAT(h));
      xpl_send_current_float("trig", "h0", h);
   }
}


/*
 * temperature data and callbacks
 */
void update_temperature_callback(float t, float l, void *data)
{
   homekit_characteristic_t *c = (homekit_characteristic_t *)data;
   c->value.float_value = t;

   if(t != l) {
      homekit_characteristic_notify(c, HOMEKIT_FLOAT(t));
      xpl_send_current_float("trig", "t1", t);
   }
}


/*
 * relay data and callbacks
 */
void update_relay_callback(int8_t v, int8_t prev, int8_t id, void *data);
#define NB_RELAYS 4
struct relay_s my_relays[NB_RELAYS] = {
   { .gpio_pin=4,  .name="Relay 1", .callback=update_relay_callback, .status=1 },
   { .gpio_pin=21, .name="Relay 2", .callback=update_relay_callback, .status=1 },
   { .gpio_pin=22, .name="Relay 3", .callback=update_relay_callback, .status=1 },
   { .gpio_pin=25, .name="Relay 4", .callback=update_relay_callback, .status=1 }
};


void update_relay_callback(int8_t v, int8_t prev, int8_t id, void *data)
{
   homekit_characteristic_t *_c = (homekit_characteristic_t *)data;
   _c->value.bool_value=v;

   if(v!=prev) {
      char device[3]="oX";
      device[1]='0'+id;
      homekit_characteristic_notify(_c, HOMEKIT_BOOL(_c->value.bool_value));
      xpl_send_current_hl("trig", device, v);
   }
}

/*
int8_t update_relay(uint8_t r)
{
   if(r<NB_RELAYS) {
      homekit_characteristic_t *_c = (homekit_characteristic_t *)(my_relays[r].relay);
      _c->value.bool_value=relays_get(r);
      homekit_characteristic_notify(_c, HOMEKIT_BOOL(_c->value.bool_value));
      return 0;
   }
   return -1;
}
*/

homekit_value_t relay_state_getter(homekit_characteristic_t *c)
{
   for(int i=0;i<NB_RELAYS; i++) {
      homekit_characteristic_t *_c = (homekit_characteristic_t *)(my_relays[i].relay);
      if(_c->id == c->id) {
         return HOMEKIT_BOOL(relays_get(i) == 0 ? 0 : 1);
      }
   }
   return HOMEKIT_BOOL(0);
}


void relay_state_setter(homekit_characteristic_t *c, const homekit_value_t value) {
   for(int8_t i=0;i<NB_RELAYS; i++) {
      homekit_characteristic_t *_c = (homekit_characteristic_t *)(my_relays[i].relay);
      if(_c->id == c->id) {
         relays_set(i,value.bool_value);
         return;
      }
   }
}


/*
 * Homekit
 */
#define MAX_SERVICES 20
homekit_accessory_t *accessories[2];
homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t temperature_dht = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity_dht = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);


homekit_server_config_t config = {
   .accessories = accessories,
   .password = NULL
};


void identify_device(homekit_value_t _value) {
   ESP_LOGI(TAG, "identify");
   uint16_t interval=status_led_get_interval();
   status_led_set_interval(75);
   vTaskDelay(2000 / portTICK_PERIOD_MS);
   status_led_set_interval(interval);
}


homekit_server_config_t *init_accessory() {
   homekit_service_t* services[MAX_SERVICES + 1];
   homekit_service_t** s = services;

   struct mea_config_s *mea_config = config_get();

   config.password = mea_config->accessory_password;

   *(s++) = NEW_HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
      NEW_HOMEKIT_CHARACTERISTIC(NAME, mea_config->accessory_name),
      NEW_HOMEKIT_CHARACTERISTIC(MANUFACTURER, "MEA"),
      NEW_HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0"),
      NEW_HOMEKIT_CHARACTERISTIC(MODEL, "combo_4I_4O_1T"),
      NEW_HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
      NEW_HOMEKIT_CHARACTERISTIC(IDENTIFY, identify_device),
      NULL
   });

   *(s++) = NEW_HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
      NEW_HOMEKIT_CHARACTERISTIC(NAME, "ambient temperature"),
      &temperature_dht,
      NULL
   });

   *(s++) = NEW_HOMEKIT_SERVICE(HUMIDITY_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
      NEW_HOMEKIT_CHARACTERISTIC(NAME, "ambient humidity"),
      &humidity_dht,
      NULL
   });

   *(s++) = NEW_HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
      NEW_HOMEKIT_CHARACTERISTIC(NAME, "temperature probe"),
      &temperature,
      NULL
   });

   for(int i=0;i<NB_RELAYS;i++) {
      my_relays[i].relay=(void *)NEW_HOMEKIT_CHARACTERISTIC(ON, false, .getter_ex=relay_state_getter, .setter_ex=relay_state_setter, NULL);
//      *(s++) = NEW_HOMEKIT_SERVICE(LIGHTBULB, .characteristics=(homekit_characteristic_t*[]) {
      *(s++) = NEW_HOMEKIT_SERVICE(OUTLET, .characteristics=(homekit_characteristic_t*[]) {
         NEW_HOMEKIT_CHARACTERISTIC(NAME, my_relays[i].name),
         my_relays[i].relay,
         NULL
      });
   }

   for(int i=0;i<NB_CONTACTS;i++) {
      my_contacts[i].contact=(void *)NEW_HOMEKIT_CHARACTERISTIC(CONTACT_SENSOR_STATE, 0, .getter_ex=contact_state_getter, .setter_ex=NULL, NULL);
      *(s++) = NEW_HOMEKIT_SERVICE(CONTACT_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
         NEW_HOMEKIT_CHARACTERISTIC(NAME, my_contacts[i].name),
         my_contacts[i].contact,
         NULL
      });
   }

   *(s++) = NULL;

//   accessories[0] = NEW_HOMEKIT_ACCESSORY(.category=homekit_accessory_category_lightbulb, .services=services);
   accessories[0] = NEW_HOMEKIT_ACCESSORY(.category=homekit_accessory_category_outlet, .services=services);
   accessories[1] = NULL;

   return &config;
}


void sta_network_ready() {
   status_led_set_interval(1000);

   homekit_server_config_t *_config = init_accessory();
   if(_config) {
      homekit_server_init(_config);
   }

   vTaskDelay(2000 / portTICK_PERIOD_MS);

   contacts_init(my_contacts, NB_CONTACTS);

   temperature_dht_init(update_temperature_dht_callback,(void *)&temperature_dht, update_humidity_dht_callback, (void *)&humidity_dht);
   temperature_dht_start();

   temperature_ds18b20_init(update_temperature_callback,(void *)&temperature);
   temperature_ds18b20_start();

   tcp_server_init(TCP_SERVER_RESTRICTED);
   xpl_server_init(mea_config->xpl_addr);
}


void ap_network_ready() {
   status_led_set_interval(50);

   tcp_server_init(TCP_SERVER_CONFIG);
}


struct contact_s startup_contact[1] = {
   { .last_state=-1, .gpio_pin=23, .name="init", .callback=NULL, .status=1  }
};

#define BUTTON_PUSHED 1
int select_startup_mode()
{
   int8_t startup_mode=0;
   contacts_init(startup_contact, 1);
   while(startup_contact[0].last_state==-1) { // wait gpio_in_init done
      vTaskDelay(1);
   }
   if(startup_contact[0].last_state==BUTTON_PUSHED) {
      int16_t i=0;
      for(;startup_contact[0].last_state==BUTTON_PUSHED && i<5000/portTICK_PERIOD_MS;i++) { // wait 5s or button released
         vTaskDelay(1);
      }
      if(i>=5000/portTICK_PERIOD_MS && startup_contact[0].last_state==BUTTON_PUSHED) { // button pressed during 5 seconds ?
         startup_mode=1;
      }
   }
   status_led_set_interval(125);
   return startup_mode;
}


void app_main(void) {
   status_led_init(10, LED_PIN);

   esp_err_t ret = nvs_flash_init();
   if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
   }
   ESP_ERROR_CHECK(ret);

   relays_init(my_relays, NB_RELAYS);

   mea_config = config_init();
   
   int8_t mode=network_init(mea_config,select_startup_mode());
   switch(mode) {
      case 0: esp_restart();
              break;
      case 1: // sta
              sta_network_ready();
              break;
      case 2: // AP
              ap_network_ready();
              break;
      default:
              break;
   }
}
