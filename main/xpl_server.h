#ifndef __xpl_server_h
#define __xpl_server_h

#define XPL_PORT 3865

#define XPL_MAX_MESSAGE_ITEMS 10
#define XPL_MAX_VALUE_SIZE 41

typedef struct xpl_msg_s {
   char section[36];
   char name[18];
   char value[XPL_MAX_VALUE_SIZE];
} xpl_msg_struc_t ;


typedef int8_t (*xpl_process_msg_callback_t)(int sock, struct xpl_msg_s *xpl_msg, int nb_values, void *userdata);

int8_t is_number(char *s);

int8_t xpl_msg_has_section_name(char *section, struct xpl_msg_s *pxpl_msg, int8_t nb_values);
char *xpl_value_p(const char *xpl_name, struct xpl_msg_s *pxpl_msg, unsigned int xpl_msg_index);

void xpl_server_init(char *source, xpl_process_msg_callback_t _callback, void *_userdata);
void xpl_server_restart();
void xpl_server_stop();
void xpl_server_start();

int8_t  xpl_set_source(char *s);
char   *xpl_get_source();

int16_t xpl_get_sock();
int16_t xpl_send_msg(int16_t fd, char *data, int l_data);
int16_t xpl_read_msg(int16_t fd, int32_t timeoutms, char *data, int l_data);
void    xpl_send_current_float(int16_t fd, char *msg_type, char *device, float value);
void    xpl_send_current_hl(int16_t fd, char *msg_type, char *device, int8_t value);

#endif
