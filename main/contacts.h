#ifndef __contacts_h
#define __contacts_h

typedef void (*contact_callback_t)(int value, void *userdata);

struct contact_s {
   int last_state;
   int gpio_pin;
   char *name;

   void *contact;
   contact_callback_t callback;
};

void gpio_in_init(struct contact_s contacts[], int nb_contacts);

#endif
