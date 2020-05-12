#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "config.h"


/*
 *
 * WIFI STA MODE
 *
 */
#define WIFI_ESP_MAXIMUM_RETRY 5

static const char *TAG_STA = "wifi station";

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static int s_retry_num = 0;


static void event_handler_sta(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
   if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();
   }
   else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
      if (s_retry_num < WIFI_ESP_MAXIMUM_RETRY) {
         esp_wifi_connect();
         s_retry_num++;
         ESP_LOGI(TAG_STA, "retry to connect to the AP");
      }
      else {
         xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      }
      ESP_LOGI(TAG_STA,"connect to the AP fail");
   }
   else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      ESP_LOGI(TAG_STA, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      s_retry_num = 0;
      xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
   }
}


int wifi_init_sta(char *wifi_ssid, char *wifi_password)
{
   s_wifi_event_group = xEventGroupCreate();

   ESP_ERROR_CHECK(esp_netif_init());

   ESP_ERROR_CHECK(esp_event_loop_create_default());
   esp_netif_create_default_wifi_sta();

   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   ESP_ERROR_CHECK(esp_wifi_init(&cfg));

   esp_event_handler_instance_t instance_any_id;
   esp_event_handler_instance_t instance_got_ip;
   ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                       ESP_EVENT_ANY_ID,
                                                       &event_handler_sta,
                                                       NULL,
                                                       &instance_any_id));
   ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                       IP_EVENT_STA_GOT_IP,
                                                       &event_handler_sta,
                                                       NULL,
                                                       &instance_got_ip));

   wifi_config_t wifi_config = {
      .sta = {
         .ssid = "",
         .password = ""
      },
   };
   strcpy((char *)wifi_config.sta.ssid, wifi_ssid);
   strcpy((char *)wifi_config.sta.password, wifi_password);

   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
   ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
   ESP_ERROR_CHECK(esp_wifi_start() );

   ESP_LOGI(TAG_STA, "wifi_init_sta finished.");

   /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
   EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
      WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
      pdFALSE,
      pdFALSE,
      portMAX_DELAY);

   /* The event will not be processed after unregister */
   ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
   ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
   vEventGroupDelete(s_wifi_event_group);

   if (bits & WIFI_CONNECTED_BIT) {
      ESP_LOGI(TAG_STA, "connected to ap SSID:%s", wifi_ssid);
      return 1;
   }
   else if (bits & WIFI_FAIL_BIT) {
      ESP_LOGI(TAG_STA, "Failed to connect to SSID:%s", wifi_password);
      return 0;
   }
   else {
      ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
      return -1;
   }
}


/*
 *
 * WIFI AP
 *
 */
char *TAG_AP = "wifi AP";

#define ESP_WIFI_SSID      "esp32"
#define ESP_WIFI_PASS      "password"
#define ESP_WIFI_CHANNEL   (11)
#define MAX_STA_CONN       (5)


static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
   if (event_id == WIFI_EVENT_AP_STACONNECTED) {
      wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
      ESP_LOGI(TAG_AP, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
   } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
      wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
      ESP_LOGI(TAG_AP, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
   }
}


int wifi_init_softap(char *ssid, char *password)
{
   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

   ESP_ERROR_CHECK(esp_netif_init());
   ESP_ERROR_CHECK(esp_event_loop_create_default());
   esp_netif_create_default_wifi_ap();
   ESP_ERROR_CHECK(esp_wifi_init(&cfg));
   ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

   wifi_config_t wifi_config = {
      .ap = {
          .ssid_len = strlen(ssid),
          .channel = ESP_WIFI_CHANNEL,
          .max_connection = MAX_STA_CONN,
          .authmode = WIFI_AUTH_WPA_WPA2_PSK
      },
   };
   strcpy((char *)wifi_config.ap.ssid, ssid);
   strcpy((char *)wifi_config.ap.password, password);

   if (strlen(password) == 0) {
      wifi_config.ap.authmode = WIFI_AUTH_OPEN;
   }

   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
   ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
   ESP_ERROR_CHECK(esp_wifi_start());

   ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s channel:%d", ssid, ESP_WIFI_CHANNEL);

   return 1;
}


int8_t network_init(struct mea_config_s *mea_config, int8_t ap_mode)
{
   int8_t ret=0;
   int8_t mode=0;

   if(mea_config->wifi_configured_flag==0 || ap_mode == 1) {
     mode=2;
     ret=wifi_init_softap(mea_config->accessory_name,"0123456789");
   }
   else {
      mode=1;
      ret=wifi_init_sta(mea_config->wifi_ssid, mea_config->wifi_password);
   }

   if(ret==1) {
      return mode;
   }
   else {
      return 0;
   }
}
