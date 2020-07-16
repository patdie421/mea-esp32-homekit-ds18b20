#ifndef __relay_h
#define __relay_h

void relay1_write(bool on);
void relay1_init();
int  relay1_on_get();
void relay1_on_set(int value);

#define RELAY_OPENED 1
#define RELAY_CLOSED 0

#define RELAY_WITH_SAVING 1
#define RELAY_WITHOUT_SAVING 0

typedef void (*relay_callback_t)(int8_t value, int8_t prev, int8_t id, void *userdata);

struct relay_s {
   int8_t gpio_pin;
   int8_t state;
   char *name;
   void *relay;
   int8_t status;
   relay_callback_t callback;

};

void relays_init(struct relay_s relays[], int nb_relays, int8_t save_state);

int  relays_get(int r);
void relays_set(int r, bool value);

#endif
