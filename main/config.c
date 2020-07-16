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
static char xpl_addr[35]="";

typedef char * (*generate_value_t)(void *userdata);

// config
char *nvs_namespace = "meaconfig";

static struct mea_config_s mea_config = {
   .accessory_password = NULL,
   .accessory_name = NULL,
   .wifi_configured_flag = 0,
   .wifi_ssid = NULL,
   .wifi_password = NULL,
   .xpl_vendorid = "mea",
   .xpl_deviceid = NULL,
   .xpl_instanceid = NULL,
   .xpl_addr = xpl_addr,
   .token = NULL
};


struct mea_config_s *config_get()
{
   return &mea_config;
}


#define TOKENSIZE 20
static char *generate_token_alloc(void *data)
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


static char *generate_xpl_device_id_alloc(void *data)
{
   uint8_t macaddr[6];

   esp_wifi_get_mac(ESP_IF_WIFI_STA, macaddr);

   int l = snprintf(NULL, 0, "%2s%02x%02x%02x", (char *)data, macaddr[3], macaddr[4], macaddr[5]);
   char *_device_id = malloc(l+1);
   if(_device_id) {
      snprintf(_device_id, l+1, "%2s%02x%02x%02x", (char *)data, macaddr[3], macaddr[4], macaddr[5]);
   }
   return _device_id;
}


static char *generate_accessory_name_alloc(void *data)
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


static char *generate_accessory_password_alloc(void *data)
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



static int set_item_str_value(nvs_handle_t *my_handle, char *item, char **variable, char *value)
{
   if(*variable) {
      free(*variable);
      *variable=NULL;
   }
   *variable=malloc(strlen(value)+1);
   strcpy(*variable, value);
   return nvs_set_str(*my_handle, item, value);
}


static int retrieve_item_str_value(nvs_handle_t *my_handle, char *item, char **variable, generate_value_t generate_value, void *userdata)
{
   size_t required_size = 0;

   if(*variable) {
      free(*variable);
      *variable=NULL;
   }
   int ret = nvs_get_str(*my_handle, item, NULL, &required_size);
   if(ret == ESP_ERR_NVS_NOT_FOUND) {
      if(generate_value) {
         *variable = generate_value(userdata);
      }
      else {
         *variable = userdata;
      }
      if(*variable) {
         ret = nvs_set_str(*my_handle, item, *variable);
         if(ret!=ESP_OK) {
            ESP_LOGE(TAG, "Error (%s) nvs_set_str", esp_err_to_name(ret));
            return -1;
         }
      }
   }
   else {
      *variable = malloc(required_size);
      nvs_get_str(*my_handle, item, *variable, &required_size);
   }

   return 0;
}


static inline int __set_mea_config_wifi(nvs_handle_t *my_handle, char *wifi_ssid, char *wifi_password)
{
   set_item_str_value(my_handle, "wifi_ssid", &(mea_config.wifi_ssid), wifi_ssid);
   set_item_str_value(my_handle, "wifi_pass", &(mea_config.wifi_password), wifi_password);

   return 0;
}


static int _set_mea_config_wifi(char *wifi_ssid, char *wifi_password, int flag)
{
   nvs_handle_t my_handle;

   nvs_open("meanamespace", NVS_READWRITE, &my_handle);

   __set_mea_config_wifi(&my_handle, wifi_ssid, wifi_password);

   mea_config.wifi_configured_flag=flag;
   nvs_set_u8(my_handle, "wifi_flag", flag);

   nvs_commit(my_handle);
   nvs_close(my_handle);

   return 0;
}


inline int config_set_wifi(char *wifi_ssid, char *wifi_password)
{
   return _set_mea_config_wifi(wifi_ssid, wifi_password, 1);
}


inline int config_reset_wifi()
{
   return _set_mea_config_wifi("", "", 0);
}


struct mea_config_s *config_init(char *ext)
{  
   nvs_handle_t my_handle;

   int ret = nvs_open("meanamespace", NVS_READWRITE, &my_handle);

   if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
      return NULL;
   }

   retrieve_item_str_value(&my_handle, "xpl_deviceid", &(mea_config.xpl_deviceid), generate_xpl_device_id_alloc, ext);
   retrieve_item_str_value(&my_handle, "xpl_instanceid", &(mea_config.xpl_instanceid), NULL, "edomus");
   snprintf(xpl_addr, sizeof(xpl_addr)-1, "%s-%s.%s", mea_config.xpl_vendorid, mea_config.xpl_deviceid, mea_config.xpl_instanceid);
   retrieve_item_str_value(&my_handle, "accessory_name", &(mea_config.accessory_name), generate_accessory_name_alloc, NULL);
   retrieve_item_str_value(&my_handle, "accessory_pass", &(mea_config.accessory_password), generate_accessory_password_alloc, NULL);
   retrieve_item_str_value(&my_handle, "token", &(mea_config.token), generate_token_alloc, NULL);

   ret = nvs_get_u8(my_handle, "wifi_flag", &(mea_config.wifi_configured_flag));
   if(ret == ESP_ERR_NVS_NOT_FOUND) {
      nvs_set_u8(my_handle, "wifi_flag", 0);
      __set_mea_config_wifi(&my_handle, "", "");
   }
   else {
      retrieve_item_str_value(&my_handle, "wifi_ssid", &(mea_config.wifi_ssid), NULL, "");
      retrieve_item_str_value(&my_handle, "wifi_pass", &(mea_config.wifi_password), NULL, "");
   }

   ESP_LOGI(TAG, "WIFI_SSID=%s", mea_config.wifi_ssid);
//   ESP_LOGI(TAG, "WIFI_PASSWORD=%s", mea_config.wifi_password);
   ESP_LOGI(TAG, "ACCESSORY_NAME=%s", mea_config.accessory_name);
   ESP_LOGI(TAG, "ACCESSORY_PASSWORD=%s", mea_config.accessory_password);
   ESP_LOGI(TAG, "XPL_VENDORID=%s", mea_config.xpl_vendorid);
   ESP_LOGI(TAG, "XPL_DEVICEID=%s", mea_config.xpl_deviceid);
   ESP_LOGI(TAG, "XPL_INSTANCEID=%s", mea_config.xpl_instanceid);
   ESP_LOGI(TAG, "XPL_ADDR=%s", mea_config.xpl_addr);
   ESP_LOGI(TAG, "TOKEN=%s", mea_config.token);

   ret = nvs_commit(my_handle);
   nvs_close(my_handle);

   return &mea_config;
}
