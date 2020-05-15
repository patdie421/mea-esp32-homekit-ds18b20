#ifndef __config_h
#define __config_h


struct mea_config_s {
   char *accessory_password;
   char *accessory_name;

   uint8_t wifi_configured_flag;
   char *wifi_ssid;
   char *wifi_password;

   char *token;
};

struct mea_config_s *mea_config_init();
struct mea_config_s *get_mea_config();
int set_mea_config_wifi(char *wifi_ssid, char *wifi_password);
int reset_mea_config_wifi();

#endif
