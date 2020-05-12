#ifndef __contacts_h
#define __contacts_h

typedef void (*contact_callback_t)(int value, void *userdata);

struct contact_s {
   int8_t last_state;
   int8_t gpio_pin;
   char *name;

   void *contact;
   contact_callback_t callback;

   int8_t status;
};

void gpio_in_init(struct contact_s contacts[], int nb_contacts);

int8_t contact_get(int8_t id);

#endif
