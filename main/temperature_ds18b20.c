#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#include "temperature_ds18b20.h"

#include <owb.h>
#include <owb_gpio.h>
#include <ds18b20.h>


static char *TAG = "ds18b20";

// DS18B20
#define GPIO_DS18B20_0       (5)
#define MAX_DEVICES          (8)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)


struct temperature_ds18b20_data {
   temperature_ds18b20_callback_t cb;
   void *userdata;
} _temperature_ds18b20_data = { .cb=NULL, .userdata=NULL };


OneWireBus *owb;
owb_gpio_driver_info ds18b20_driver_info;
DS18B20_Info *ds18b20_devices[MAX_DEVICES] = {0};
int ds18b20_nb_devices = 0;


int temperature_ds18b20_init(temperature_ds18b20_callback_t cb, void *userdata)
{
   _temperature_ds18b20_data.cb=cb;
   _temperature_ds18b20_data.userdata=userdata;

    // Stable readings require a brief period before communication
   vTaskDelay(2000.0 / portTICK_PERIOD_MS);

   owb = owb_gpio_initialize(&ds18b20_driver_info, GPIO_DS18B20_0);
   owb_use_crc(owb, true); // enable CRC check for ROM code

   // Find all connected devices
   OneWireBus_ROMCode device_rom_codes[MAX_DEVICES] = {0};
   int num_devices = 0;
   OneWireBus_SearchState search_state = {0};
   bool found = false;

   owb_search_first(owb, &search_state, &found);
   while (found) {
      char rom_code_s[17];
      owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
      ESP_LOGI(TAG, "found %d : %s", num_devices, rom_code_s);
      device_rom_codes[num_devices] = search_state.rom_code;
      ++num_devices;
      owb_search_next(owb, &search_state, &found);
   }
   ESP_LOGI(TAG, "Found %d device%s", num_devices, num_devices == 1 ? "" : "s");
   ds18b20_nb_devices = num_devices; 
   if(ds18b20_nb_devices < 1) {
      return 0;
   }

   // Create DS18B20 devices on the 1-Wire bus
   for (int i = 0; i < num_devices; ++i) {
      DS18B20_Info *ds18b20_info = ds18b20_malloc(); // heap allocation
      ds18b20_devices[i] = ds18b20_info;

      if (num_devices == 1) {
         ds18b20_init_solo(ds18b20_info, owb); // only one device on bus
      }
      else {
         ds18b20_init(ds18b20_info, owb, device_rom_codes[i]); // associate with bus and device
      }
      ds18b20_use_crc(ds18b20_info, true); // enable CRC check on all reads
      ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
   }
   return ds18b20_nb_devices;
}


void temperature_ds18b20_task(void *_args) {

    float temperature_value=0;
    DS18B20_ERROR err=0;

    while (1) {
       err = ds18b20_convert_and_read_temp(ds18b20_devices[0], &temperature_value);
       if(!err) {
          if(_temperature_ds18b20_data.cb) {
             _temperature_ds18b20_data.cb(temperature_value, _temperature_ds18b20_data.userdata);
          }
       }
       vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}


void temperature_ds18b20_start()
{
   xTaskCreate(temperature_ds18b20_task, "ds18b20", 2048, &_temperature_ds18b20_data, 2, NULL);
}
