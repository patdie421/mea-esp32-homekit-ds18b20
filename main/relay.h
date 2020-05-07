#ifndef __relay_h
#define __relay_h

void relay1_write(bool on);
void relay1_init();
int  relay1_on_get();
void relay1_on_set(int value);


struct relay_s {
   int gpio_pin;
   int state;
   char *name;
   void *relay;
};

void gpio_out_init(struct relay_s relays[], int nb_relays);

int relay_get(int r);
void relay_set(int r, bool value);

#endif
