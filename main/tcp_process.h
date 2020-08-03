#ifndef __tcp_process_h
#define __tcp_process_h

#include "config.h"

int tcp_process(int sock, struct mea_config_s *mea_config, int8_t mode, char cmd, char *parameters, void *userdata);

#endif
