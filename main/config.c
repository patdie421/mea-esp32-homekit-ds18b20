#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"

static char *TAG = "config";

// config
char *nvs_namespace = "meaconfig";
static struct mea_config_s mea_config = {
   .accessory_password = NULL,
   .accessory_name = NULL,
   .wifi_configured_flag = 0,
   .wifi_ssid = NULL,
   .wifi_password = NULL,
   .token = NULL
};


struct mea_config_s *get_mea_config()
{
   return &mea_config;
}


#define TOKENSIZE 20
char *generate_token_alloc()
{
   char *t = malloc(TOKENSIZE+1);
   for(int i=0;i<TOKENSIZE;i++) {
      for(;;) {
         uint8_t r=(uint8_t)esp_random();
         if( (r>='0' && r<='9') ||
             (r>='a' && r<='z') ||
             (r>='A' && r<='Z') ) {
            t[i]=(char)r;
            break;
         }
      }
   }
   t[TOKENSIZE]=0;
   return t;
}


char *generate_accessory_name_alloc()
{
   uint8_t macaddr[6];

   esp_wifi_get_mac(ESP_IF_WIFI_STA, macaddr);
   uint8_t rnd = (uint8_t)(esp_random() & 0xFF);

   int l = snprintf(NULL, 0, "mea-%02X%02X%02X-%02X", macaddr[3], macaddr[4], macaddr[5], rnd);
   char *_accessory_name = malloc(l+1);
   if(_accessory_name) {
      snprintf(_accessory_name, l+1, "mea-%02X%02X%02X-%02X", macaddr[3], macaddr[4], macaddr[5], rnd);
   }
   return _accessory_name;
}


char *generate_accessory_password_alloc()
{
   char *_accessory_password = malloc(11);

   for(int i=0;i<11;i++) {
      if(i==3 || i==6) {
         _accessory_password[i]='-';
      }
      else {
         _accessory_password[i] = (int)((double)esp_random()/(double)UINT32_MAX * 9.0)+'0';
      }
   }
   _accessory_password[10]=0;

   return _accessory_password;
}


static char *_get_str(nvs_handle_t *my_handle, char *key, char **var)
{
   size_t required_size = 0;

   nvs_get_str(*my_handle, key, NULL, &required_size);
   if(*var) {
      free(*var);
   }
   *var=malloc(required_size);
   nvs_get_str(*my_handle, key, *var, &required_size);

   return *var;
}


static int _set_mea_config_wifi(nvs_handle_t *my_handle, char *wifi_ssid, char *wifi_password)
{
   if(mea_config.wifi_ssid) {
      free(mea_config.wifi_ssid);
   }
   mea_config.wifi_ssid=malloc(strlen(wifi_ssid)+1);
   strcpy(mea_config.wifi_ssid, wifi_ssid);
   nvs_set_str(*my_handle, "wifi_ssid", wifi_ssid);

   if(mea_config.wifi_password) {
      free(mea_config.wifi_password);
   }
   mea_config.wifi_password=malloc(strlen(wifi_password)+1);
   strcpy(mea_config.wifi_password, wifi_password);
   nvs_set_str(*my_handle, "wifi_pass", wifi_password);

   return 0;
}


int __set_mea_config_wifi(char *wifi_ssid, char *wifi_password, int flag)
{
   nvs_handle_t my_handle;

   nvs_open("meanamespace", NVS_READWRITE, &my_handle);

   _set_mea_config_wifi(&my_handle, wifi_ssid, wifi_password);

   mea_config.wifi_configured_flag=flag;
   nvs_set_u8(my_handle, "wifi_flag", flag);

   nvs_commit(my_handle);
   nvs_close(my_handle);

   return 0;
}


inline int set_mea_config_wifi(char *wifi_ssid, char *wifi_password)
{
   return __set_mea_config_wifi(wifi_ssid, wifi_password, 1);
}


inline int reset_mea_config_wifi()
{
   return __set_mea_config_wifi("", "", 0);
}


struct mea_config_s *mea_config_init()
{  
   nvs_handle_t my_handle;

   int ret = nvs_open("meanamespace", NVS_READWRITE, &my_handle);

   if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
      return NULL;
   }

   size_t required_size = 0;
      
   if(mea_config.accessory_name) {
      free(mea_config.accessory_name);
      mea_config.accessory_name=NULL;
   }
   ret = nvs_get_str(my_handle, "accessory_name", NULL, &required_size);
   if(ret == ESP_ERR_NVS_NOT_FOUND) {
      mea_config.accessory_name = generate_accessory_name_alloc();
      if(mea_config.accessory_name) {
         ret = nvs_set_str(my_handle, "accessory_name", mea_config.accessory_name);
         if(ret!=ESP_OK) {
            ESP_LOGE(TAG, "Error (%s) nvs_set_str", esp_err_to_name(ret));
         }
      }
   }
   else {
      mea_config.accessory_name = malloc(required_size);
      nvs_get_str(my_handle, "accessory_name", mea_config.accessory_name, &required_size);
   }
  
 
   if(mea_config.accessory_password) {
      free(mea_config.accessory_password);
      mea_config.accessory_password=NULL;
   }
   ret = nvs_get_str(my_handle, "accessory_pass", NULL, &required_size);
   if(ret == ESP_ERR_NVS_NOT_FOUND) {
      mea_config.accessory_password=generate_accessory_password_alloc();
      if(mea_config.accessory_password) {
         ret = nvs_set_str(my_handle, "accessory_pass", mea_config.accessory_password);
         if(ret!=ESP_OK) {
            ESP_LOGE(TAG, "Error (%s) nvs_set_str", esp_err_to_name(ret));
         }
      }
   }
   else {
      mea_config.accessory_password = malloc(required_size);
      nvs_get_str(my_handle, "accessory_pass", mea_config.accessory_password, &required_size);
   }


   if(mea_config.token) {
      free(mea_config.token);
      mea_config.token=NULL;
   }
   ret = nvs_get_str(my_handle, "token", NULL, &required_size);
   if(ret == ESP_ERR_NVS_NOT_FOUND) {
      mea_config.token=generate_token_alloc();
      if(mea_config.token) {
         ret = nvs_set_str(my_handle, "token", mea_config.token);
         if(ret!=ESP_OK) {
            ESP_LOGE(TAG, "Error (%s) nvs_set_str", esp_err_to_name(ret));
         }
      }
   }
   else {
      mea_config.token = malloc(required_size);
      nvs_get_str(my_handle, "token", mea_config.token, &required_size);
   }


//   uint8_t wifi_configured_flag=0;
   ret = nvs_get_u8(my_handle, "wifi_flag", &(mea_config.wifi_configured_flag));
   if(ret == ESP_ERR_NVS_NOT_FOUND) {
      nvs_set_u8(my_handle, "wifi_flag", 0);
      _set_mea_config_wifi(&my_handle, "", "");
   }
   else {
      _get_str(&my_handle, "wifi_ssid", &(mea_config.wifi_ssid));
      _get_str(&my_handle, "wifi_pass", &(mea_config.wifi_password));
   }


   ESP_LOGI(TAG, "ACCESSORY_NAME=%s", mea_config.accessory_name);
   ESP_LOGI(TAG, "ACCESSORY_PASSWORD=%s", mea_config.accessory_password);
   ESP_LOGI(TAG, "WIFI_SSID=%s", mea_config.wifi_ssid);
   ESP_LOGI(TAG, "TOKEN=%s", mea_config.token);
//   ESP_LOGI(TAG, "WIFI_PASSWORD=%s", mea_config.wifi_password);

   ret = nvs_commit(my_handle);
   nvs_close(my_handle);


   return &mea_config;
}
