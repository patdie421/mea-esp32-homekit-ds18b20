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

#include "relays.h"


static char *TAG = "relays";

int _nb_relays = 0;
struct relay_s *_relays = NULL;
static nvs_handle_t _relays_handle = 0;
static int8_t _nvs_flag = 0;


static void _relay_save_state(int8_t id)
{
   if(_nvs_flag) {
      nvs_set_i16(_relays_handle, _relays[id].name, _relays[id].state);       
   }
}


static int8_t _relay_load_state(int8_t id)
{
   int16_t v = 0;

   if(_nvs_flag) {
      nvs_get_i16(_relays_handle, _relays[id].name, &v);
   }
   return v;
}


int relay_get(int i)
{
   if(_relays && i<_nb_relays) {
      return _relays[i].state;
   }

   return -1;
}


void relay_set(int i, bool v)
{
   if(_relays && i<_nb_relays) {
      gpio_set_level(_relays[i].gpio_pin, v ? RELAY_CLOSED : RELAY_OPENED);
      _relays[i].state=v ? 1 : 0;
      _relay_save_state(i);
   }
}


void gpio_out_init(struct relay_s relays[], int nb_relays) {
   _relays = relays;
   _nb_relays = nb_relays;

   esp_err_t ret = nvs_open("relays", NVS_READWRITE, &_relays_handle);
   if (ret == ESP_OK) {
      _nvs_flag=1;
   }
    
   for(int i=0;i<_nb_relays;i++) {
      gpio_set_direction(relays[i].gpio_pin, GPIO_MODE_OUTPUT);
      relay_set(i, _relay_load_state(i) ? 1 : 0);
   }

   ESP_LOGI(TAG, "relays initialized");
}
