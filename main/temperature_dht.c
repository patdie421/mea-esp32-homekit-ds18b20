#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include <dht.h>

#include "temperature_dht.h"

#define SENSOR_TYPE DHT_TYPE_DHT11
#define DHT_GPIO 26

static char *TAG = "dht11";

static int16_t temperature = 0;
static int16_t humidity = 0;

struct dht_data {
   temperature_dht_callback_t cb_t;
   void *userdata_t;
   float prev_t;
   humidity_dht_callback_t cb_h;
   void *userdata_h;
   float prev_h;

} _dht_data = { .cb_t=NULL, .userdata_t=NULL, .cb_h=NULL, .userdata_h=NULL, .prev_t=-9999.0, .prev_h=-1.0 };


int temperature_dht_init(temperature_dht_callback_t cb_t, void *userdata_t, temperature_dht_callback_t cb_h, void *userdata_h)
{
   _dht_data.cb_t=cb_t;
   _dht_data.userdata_t=userdata_t;
   _dht_data.cb_h=cb_h;
   _dht_data.userdata_h=userdata_h;

   return 0;
}


float temperature_dht_get_t()
{
   return temperature / 10.0;
}


float temperature_dht_get_h()
{
   return humidity / 10.0;
}


void temperature_dht_task(void *_args)
{
   // DHT sensors that come mounted on a PCB generally have
   // pull-up resistors on the data pin.  It is recommended
   // to provide an external pull-up resistor otherwise...
   gpio_set_pull_mode(DHT_GPIO, GPIO_PULLUP_ONLY);

   while (1) {
      if (dht_read_data(SENSOR_TYPE, DHT_GPIO, &humidity, &temperature) == ESP_OK) {
         ESP_LOGI(TAG, "Humidity: %.0f%% Temp: %.1f°C", humidity / 10.0, temperature / 10.0);
         if(_dht_data.cb_t) {
            _dht_data.cb_t(temperature / 10.0, _dht_data.prev_t / 10.0, _dht_data.userdata_t);
         }
         if(_dht_data.cb_h) {
            _dht_data.cb_h(humidity / 10.0, _dht_data.prev_h / 10.0, _dht_data.userdata_h);
         }
         _dht_data.prev_t=temperature;
         _dht_data.prev_h=humidity;
      }
      else {
      }
      vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

}


void temperature_dht_start()
{
   xTaskCreate(temperature_dht_task, "dht", 2560, &_dht_data, 2, NULL);
}
