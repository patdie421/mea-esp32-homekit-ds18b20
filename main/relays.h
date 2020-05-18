#ifndef __relay_h
#define __relay_h

void relay1_write(bool on);
void relay1_init();
int  relay1_on_get();
void relay1_on_set(int value);

#define RELAY_OPENED 1
#define RELAY_CLOSED 0

struct relay_s {
   int8_t gpio_pin;
   int8_t state;
   char *name;
   void *relay;
   int8_t status;
};

void relays_init(struct relay_s relays[], int nb_relays);

int  relays_get(int r);
void relays_set(int r, bool value);

#endif
