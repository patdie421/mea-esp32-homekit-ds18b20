#ifndef __xpl_server_h
#define __xpl_server_h

#define XPL_MAX_MESSAGE_ITEMS 10
#define XPL_MAX_VALUE_SIZE 41

typedef struct xpl_msg_s {
   char section[36];
   char name[18];
   char value[XPL_MAX_VALUE_SIZE];
} xpl_msg_struc_t ;

void xpl_server_init(char *source);

void xpl_send_current_float(char *msg_type, char *device, float value);
void xpl_send_current_hl(char *msg_type, char *device, int8_t value);

#endif
