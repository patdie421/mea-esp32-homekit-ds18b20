#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#include "status_led.h"


const int status_led_gpio = 2;
int status_led_interval = 50; // ms


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


void status_led_init(int interval) {
   status_led_interval=interval;
   gpio_set_direction(status_led_gpio, GPIO_MODE_OUTPUT);
   status_led_write(false);
   xTaskCreate(status_led_task, "status led", 1024, NULL, 2, NULL);
}


void status_led_set_interval(int interval)
{
   status_led_interval=interval;
}


int status_led_get_interval()
{
   return status_led_interval;
}
