#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>

#include <ds18x20.h>
#include "ds18b20_add.h"

#define ds18x20_WRITE_SCRATCHPAD 0x4E
#define DS18B20_FAMILY_ID 0x28

esp_err_t ds18b20_set_resolution(gpio_num_t pin, ds18x20_addr_t addr, uint8_t resolution)
{
    uint8_t family_id = (uint8_t)addr;

    if(addr != ds18x20_ANY && family_id != DS18B20_FAMILY_ID) {
       return ESP_FAIL;
    }

    if (!onewire_reset(pin))
        return ESP_ERR_INVALID_RESPONSE;

    if (addr == ds18x20_ANY) {
        onewire_skip_rom(pin);
    }
    else {
        onewire_select(pin, addr);
    }

    onewire_write(pin, ds18x20_WRITE_SCRATCHPAD);
    onewire_write(pin, 0);
    onewire_write(pin, 0);
    onewire_write(pin, resolution);

    return ESP_OK;
}
