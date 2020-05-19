#ifndef __temperature_ds18b20_h
#define __temperature_ds18b20_h

typedef void (*temperature_ds18b20_callback_t)(float value, float prev, void *userdata);

int  temperature_ds18b20_init(temperature_ds18b20_callback_t cb, void *userdata);
void temperature_ds18b20_task(void *_args);
void temperature_ds18b20_start();

float temperature_ds18b20_get();

#endif
