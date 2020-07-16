#ifndef __network_h
#define __network_h

#include "config.h"

typedef void (*restart_callback_t)(void *userdata);

int8_t network_init(struct mea_config_s *mea_config, int8_t ap_mode, restart_callback_t restart_callback, void *restart_userdata);

#endif
