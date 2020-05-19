#ifndef __config_h
#define __config_h


struct mea_config_s {
   char *accessory_password;
   char *accessory_name;

   uint8_t wifi_configured_flag;
   char *wifi_ssid;
   char *wifi_password;

   char *token;

   char *xpl_deviceid;
   char *xpl_vendorid;
   char *xpl_instanceid;
   char *xpl_addr;
};

struct mea_config_s *config_init();
struct mea_config_s *config_get();
int config_set_wifi(char *wifi_ssid, char *wifi_password);
int config_reset_wifi();

#endif
