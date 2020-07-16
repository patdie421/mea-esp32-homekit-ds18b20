#ifndef __xpl_process_h
#define __xpl_process_h

#include "xpl_server.h"

int8_t xpl_process_msg(int sock, struct xpl_msg_s *xpl_msg, int nb_values, void *userdata);

#endif

