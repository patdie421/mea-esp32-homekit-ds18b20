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

#include "relay.h"


static char *TAG = "relays";

int _nb_relays = 0;
struct relay_s *_relays = NULL;

/*
const int relay1_gpio = 4;
bool relay1_on = false;

void relay1_write(bool on) {
   gpio_set_level(relay1_gpio, on ? 1 : 0);
}

void relay1_init() {
    gpio_set_direction(relay1_gpio, GPIO_MODE_OUTPUT);
    relay1_write(relay1_on);
}

int relay1_on_get() {
   return relay1_on;
}

void relay1_on_set(int value) {
   relay1_on = value;
   relay1_write(relay1_on);
}
*/


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
      gpio_set_level(_relays[i].gpio_pin, v ? 1 : 0);
      _relays[i].state=v ? 1 : 0;
   }
}


void gpio_out_init(struct relay_s relays[], int nb_relays) {
   _relays = relays;
   _nb_relays = nb_relays;
   
   for(int i=0;i<_nb_relays;i++) {
      gpio_set_direction(relays[i].gpio_pin, GPIO_MODE_OUTPUT);
   }
}
