#ifndef __tcp_server_h
#define __tcp_server_h

#include "config.h"

#define TCP_SERVER_RESTRICTED 0
#define TCP_SERVER_CONFIG 1

#define OK_STATUS "OK"
#define KO_STATUS "KO"
#define BAD_COMMAND_STATUS "???"
#define BAD_REQUEST_STATUS "!!!"
#define BAD_CREDENCIAL "###"

typedef int (*tcp_process_callback_t)(int sock, struct mea_config_s *mea_config, int8_t mode, char cmd, char *parameters, void *userdata);

void tcp_send_data(const int sock, char *data);
void tcp_server_init(uint8_t mode, tcp_process_callback_t _callback, void *_userdata);

void tcp_server_restart();

#endif
