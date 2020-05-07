#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#include <driver/gpio.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include <owb.h>
#include <owb_gpio.h>
#include <ds18b20.h>

#include "wifi.h"

#include "network.h"
#include "config.h"
#include "status_led.h"
#include "temperature_ds18b20.h"
#include "relay.h"
#include "tcp_server.h"
#include "contacts.h"

static char *TAG = "main";

static struct mea_config_s *mea_config = NULL;


/*
 * Contacts data and callbacks
 */
void update_contact_callback(int v, void *data);

#define NB_CONTACTS 4

struct contact_s my_contacts[NB_CONTACTS] = {
   { .last_state=-1, .gpio_pin=16, .name="Contact 1", .callback=update_contact_callback  },
   { .last_state=-1, .gpio_pin=17, .name="Contact 2", .callback=update_contact_callback  },
   { .last_state=-1, .gpio_pin=18, .name="Contact 3", .callback=update_contact_callback  },
   { .last_state=-1, .gpio_pin=19, .name="Contact 4", .callback=update_contact_callback  }
};


void update_contact_callback(int v, void *data)
{
   homekit_characteristic_t *c=(homekit_characteristic_t *)data;
   c->value.uint8_value = (uint8_t)v;

   homekit_characteristic_notify(c, HOMEKIT_UINT8(v));
}


homekit_value_t contact_state_getter(homekit_characteristic_t *c) {
   for(int i=0;i<NB_CONTACTS; i++) {
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
 * temperature data and callbacks
 */
void update_temperature_callback(float v, void *data)
{
   homekit_characteristic_t *t = (homekit_characteristic_t *)data;
   t->value.float_value = v;

   homekit_characteristic_notify(t, HOMEKIT_FLOAT(v));
}


/*
 * relay data and callbacks
 */
#define NB_RELAYS 2
struct relay_s my_relays[NB_RELAYS] = {
   { .gpio_pin=4,  .name="Relay 1" },
   { .gpio_pin=21, .name="Relay 2" }
};


homekit_value_t relay_state_getter(homekit_characteristic_t *c)
{
   for(int i=0;i<NB_RELAYS; i++) {
      homekit_characteristic_t *_c = (homekit_characteristic_t *)(my_relays[i].relay);
      if(_c->id == c->id) {
         return HOMEKIT_BOOL(relay_get(i) == 0 ? 0 : 1);
      }
   }
   return HOMEKIT_BOOL(0);
}


void relay_state_setter(homekit_characteristic_t *c, const homekit_value_t value) {
   for(int i=0;i<NB_RELAYS; i++) {
      homekit_characteristic_t *_c = (homekit_characteristic_t *)(my_relays[i].relay);
      if(_c->id == c->id) {
         relay_set(i,value.bool_value);
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


homekit_server_config_t config = {
   .accessories = accessories,
   .password = NULL
};


void identify_device(homekit_value_t _value) {
   ESP_LOGI(TAG, "identify");
   int interval=status_led_get_interval();
   status_led_set_interval(75);
   vTaskDelay(2000 / portTICK_PERIOD_MS);
   status_led_set_interval(interval);
}


homekit_server_config_t *init_accessory() {
   homekit_service_t* services[MAX_SERVICES + 1];
   homekit_service_t** s = services;

   struct mea_config_s *mea_config = get_mea_config();

   config.password = mea_config->accessory_password;

   *(s++) = NEW_HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
      NEW_HOMEKIT_CHARACTERISTIC(NAME, mea_config->accessory_name),
      NEW_HOMEKIT_CHARACTERISTIC(MANUFACTURER, "MEA"),
      NEW_HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0"),
      NEW_HOMEKIT_CHARACTERISTIC(MODEL, "OneLed"),
      NEW_HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.0"),
      NEW_HOMEKIT_CHARACTERISTIC(IDENTIFY, identify_device),
      NULL
   });

   *(s++) = NEW_HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
      NEW_HOMEKIT_CHARACTERISTIC(NAME, "TEMP1"),
      &temperature,
      NULL
   });


   for(int i=0;i<NB_RELAYS;i++) {
      my_relays[i].relay=(void *)NEW_HOMEKIT_CHARACTERISTIC(ON, false, .getter_ex=relay_state_getter, .setter_ex=relay_state_setter, NULL);
      *(s++) = NEW_HOMEKIT_SERVICE(LIGHTBULB, .characteristics=(homekit_characteristic_t*[]) {
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

   accessories[0] = NEW_HOMEKIT_ACCESSORY(.category=homekit_accessory_category_lightbulb, .services=services);
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

   gpio_out_init(my_relays, NB_RELAYS);
   gpio_in_init(my_contacts, NB_CONTACTS);
   temperature_ds18b20_init(update_temperature_callback,(void *)&temperature);
   temperature_ds18b20_start();

   tcp_server_init();
}


void ap_network_ready() {
   status_led_set_interval(50);

   tcp_server_init();
}


struct contact_s startup_contact[1] = {
   { .last_state=-1, .gpio_pin=23, .name="init", .callback=NULL  }
};


void app_main(void) {
   status_led_init(125);

   esp_err_t ret = nvs_flash_init();
   if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
   }
   ESP_ERROR_CHECK(ret);

   mea_config = mea_config_init();

   int startup_mode=0;
   gpio_in_init(startup_contact, 1);
   while(startup_contact[0].last_state==-1) {
      vTaskDelay(1);
   }
   if(startup_contact[0].last_state==0) {
      status_led_set_interval(10);
      int i=0;
      for(;startup_contact[0].last_state==0 && i<5000/portTICK_PERIOD_MS;i++) {
         vTaskDelay(1);
      }
      if(i>=5000/portTICK_PERIOD_MS && startup_contact[0].last_state==0) {
         startup_mode=1;
      }
   }

   
   int mode=network_init(mea_config,startup_mode);
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
