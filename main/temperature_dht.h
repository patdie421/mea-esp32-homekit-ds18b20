#ifndef __temperature_dht_h
#define __temperature_dht_h

typedef void (*temperature_dht_callback_t)(float temperature, float prev, void *userdata);
typedef void (*humidity_dht_callback_t)(float humidity, float prev, void *userdata);

float temperature_dht_get_h();
float temperature_dht_get_t();

int  temperature_dht_init(temperature_dht_callback_t cb_t, void *userdata_t, humidity_dht_callback_t cb_h, void *userdata_h);
void temperature_dht_task(void *_args);
void temperature_dht_start();

#endif

