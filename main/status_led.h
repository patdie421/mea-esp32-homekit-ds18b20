#ifndef __status_led
#define __status_led

void status_led_init(uint16_t interval, uint8_t pin);
void status_led_set_interval(uint16_t interval);
uint16_t status_led_get_interval();

#endif
