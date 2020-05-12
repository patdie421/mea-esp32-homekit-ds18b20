#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include <ds18x20.h>

#include "temperature_ds18b20.h"


static char *TAG = "ds18x20";

struct temperature_ds18b20_data {
   temperature_ds18b20_callback_t cb;
   void *userdata;
} _temperature_ds18b20_data = { .cb=NULL, .userdata=NULL };


#define MAX_SENSORS 8
#define RESCAN_INTERVAL 8
static const gpio_num_t SENSOR_GPIO = 5;
static const uint32_t LOOP_DELAY_MS = 5000;

static ds18x20_addr_t addrs[MAX_SENSORS];
static float temps[MAX_SENSORS];
static int sensor_count;


int temperature_ds18b20_init(temperature_ds18b20_callback_t cb, void *userdata)
{
   _temperature_ds18b20_data.cb=cb;
   _temperature_ds18b20_data.userdata=userdata;

   sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);

   return sensor_count;
}


void temperature_ds18b20_task(void *_args)
{
   while (1) {
      // Every RESCAN_INTERVAL samples, check to see if the sensors connected
      // to our bus have changed.
      sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);

      if (sensor_count < 1) {
         ESP_LOGI(TAG, "No sensors detected!");
      }
      else {
         ESP_LOGI(TAG, "%d sensors detected:", sensor_count);
         // If there were more sensors found than we have space to handle,
         // just report the first MAX_SENSORS..
         if (sensor_count > MAX_SENSORS) {
            sensor_count = MAX_SENSORS;
         }

         // Do a number of temperature samples, and print the results.
         for (int i = 0; i < RESCAN_INTERVAL; i++) {
            ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, temps);
/*
            for (int j = 0; j < sensor_count; j++) {
               uint32_t addr0 = addrs[j] >> 32;
               uint32_t addr1 = addrs[j];
               float temp_c = temps[j];
               ESP_LOGI(TAG, "Sensor %08x%08x reports %f deg C", addr0, addr1, temp_c);
               if(_temperature_ds18b20_data.cb) {
                  _temperature_ds18b20_data.cb(temp_c, _temperature_ds18b20_data.userdata);
               }
            }
*/
            uint32_t addr0 = addrs[0] >> 32;
            uint32_t addr1 = addrs[0];
            float temp_c = temps[0];
            ESP_LOGI(TAG, "Sensor %08x%08x reports %f deg C", addr0, addr1, temp_c);
            if(_temperature_ds18b20_data.cb) {
               _temperature_ds18b20_data.cb(temp_c, _temperature_ds18b20_data.userdata);
            }

            vTaskDelay(LOOP_DELAY_MS / portTICK_PERIOD_MS);
         }
      }
   }
}


void temperature_ds18b20_start()
{
   xTaskCreate(temperature_ds18b20_task, "ds18b20", 2048, &_temperature_ds18b20_data, 2, NULL);
}
