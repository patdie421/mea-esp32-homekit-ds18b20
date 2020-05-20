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

#include "contacts.h"


static char *TAG = "contacts";

static int _nb_contacts = 0;
static struct contact_s *_contacts;


int8_t contacts_get(int8_t id)
{
   if(id<_nb_contacts) {
      return _contacts[id].last_state;
   }
   else {
      return -1;
   }
}


static void contacts_task(void* arg)
{
   for(;;) {
      for(int i=0;i<_nb_contacts;i++) {
         int v=0;
         for(int j=0;j<24;j++) {
            vTaskDelay(1);
            v=v+gpio_get_level(_contacts[i].gpio_pin);
         }
         v=(v > 12) ? 1 : 0;
         if(v!=_contacts[i].last_state) {
            ESP_LOGI(TAG, "GPIO[%d] intr, val: %d", i, v);
            if(_contacts[i].callback) {
               _contacts[i].callback(v, _contacts[i].last_state, i, _contacts[i].contact);
            }
            _contacts[i].last_state = v;
         }
      }
   }
}


void contacts_init(struct contact_s my_contacts[], int nb_contacts) {
   gpio_config_t io_conf;

   _contacts = my_contacts;
   _nb_contacts = nb_contacts;

   uint32_t gpio_input_pin_sel = 0;

   //bit mask of the pins, use GPIO18
   for(int i=0;i<_nb_contacts;i++) {
      gpio_input_pin_sel = gpio_input_pin_sel | (1ULL<<_contacts[i].gpio_pin);
   }
   io_conf.pin_bit_mask = gpio_input_pin_sel;

   //interrupt disabled
   io_conf.intr_type = GPIO_INTR_DISABLE;
   //set as input mode
   io_conf.mode = GPIO_MODE_INPUT;
   //enable pull-up mode
   io_conf.pull_up_en = 1;
   io_conf.pull_down_en = 0;

   gpio_config(&io_conf);

   xTaskCreate(contacts_task, "contacts_task", 2560, NULL, 10, NULL);
}

