#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#include "status_led.h"


static uint8_t status_led_gpio = 2;
static uint16_t status_led_interval = 50; // ms
static uint8_t  status_led_is_init = 0;


void status_led_write(bool on) {
   gpio_set_level(status_led_gpio, on ? 1 : 0);
}


void status_led_task(void *_args) {
   while(1) {
      status_led_write(true);
      vTaskDelay(status_led_interval / portTICK_PERIOD_MS);
      status_led_write(false);
      vTaskDelay(status_led_interval / portTICK_PERIOD_MS);
   }
}


void status_led_init(uint16_t interval, uint8_t pin) {
   status_led_interval=interval;
   if(!status_led_is_init) {
      status_led_is_init=1;
      status_led_gpio=pin;
      gpio_set_direction(status_led_gpio, GPIO_MODE_OUTPUT);
      xTaskCreate(status_led_task, "status led", 1024, NULL, 2, NULL);
   }
}


void status_led_set_interval(uint16_t interval)
{
   status_led_interval=interval;
}


uint16_t status_led_get_interval()
{
   return status_led_interval;
}
