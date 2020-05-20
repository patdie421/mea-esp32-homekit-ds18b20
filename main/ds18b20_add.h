#ifndef __ds18b20_add_h
#define __ds18b20_add_h

#include <freertos/FreeRTOS.h>

#define DS18B20_RESOLUTION_9_BIT   0x1F
#define DS18B20_RESOLUTION_10_BIT  0x3F
#define DS18B20_RESOLUTION_11_BIT  0x5F
#define DS18B20_RESOLUTION_12_BIT  0x7F

esp_err_t ds18b20_set_resolution(gpio_num_t pin, ds18x20_addr_t addr, uint8_t resolution);

#endif
