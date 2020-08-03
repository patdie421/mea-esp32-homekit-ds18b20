#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "tcp_server.h"
#include "tcp_process.h"

#include "config.h"
#include "contacts.h"
#include "relays.h"
#include "temperature_dht.h"
#include "temperature_ds18b20.h"

#define PORT 8081

static const char *TAG = "tcp_server";
static int8_t _mode = 0;
struct mea_config_s *_mea_config = NULL; 
TaskHandle_t tcpServer_taskHandle = NULL;
int listen_sock = -1;

static tcp_process_callback_t __callback = NULL;
static void *__userdata = NULL;


void tcp_send_data(const int sock, char *data)
{
   int len = strlen(data);
   int to_write = len;

   while (to_write > 0) {
       int written = send(sock, data + (len - to_write), to_write, 0);
       if (written < 0) {
           ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
       }
       to_write -= written;
   }
}


static void _do_request(const int sock)
{
   int len;
   char rx_buffer[128];

   len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
   if (len < 0) {
      ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
   }
   else if (len == 0) {
      ESP_LOGW(TAG, "Connection closed");
   }
   else {
      char token[17]="", parameters[81]="";
      char cmd=0;
      int r1=0,r2=0;

      rx_buffer[len]=0;
//      int l=strlen(rx_buffer);
      int n=sscanf(rx_buffer,"%20[^:]:%c%n:%80s%n",token,&cmd,&r1,parameters,&r2);
      if(n<2 || n>3) {
         ESP_LOGE(TAG,"Data error");
         tcp_send_data(sock, BAD_REQUEST_STATUS);
         return;
      }

      if(_mode==TCP_SERVER_RESTRICTED) {
         if(strcmp(token,_mea_config->token)!=0) {
            ESP_LOGE(TAG,"Not authorized");
            tcp_send_data(sock, BAD_CREDENCIAL); // bad credencial
            return;
         }
      }
      
      if(n==3 && r2==len) {
         if(__callback) {
            if(__callback(sock, _mea_config, _mode, cmd, parameters, __userdata)==0) {
               tcp_send_data(sock, BAD_COMMAND_STATUS);
               ESP_LOGW(TAG, "unknown command");
               return;
            }
         }
      }
      else if(n==2 && r1==len) {
          if(cmd=='R') {
            tcp_send_data(sock, OK_STATUS);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ESP_LOGW(TAG, "Restarting...");
            esp_restart();
            for(;;);
         }
         else if(__callback) {
            if(__callback(sock, _mea_config, _mode, cmd, NULL, __userdata)==0) {
               tcp_send_data(sock, BAD_COMMAND_STATUS);
               ESP_LOGW(TAG, "unknown command");
               return;
            }
         }
      }
      else {
         tcp_send_data(sock, BAD_REQUEST_STATUS);
         ESP_LOGW(TAG, "bad request");
         return;
      }
      return; 
   }
}


static void tcp_server_task(void *pvParameters)
{
   int ip_protocol = 0;
   struct sockaddr_in dest_addr;

   struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
   dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
   dest_addr_ip4->sin_family = AF_INET;
   dest_addr_ip4->sin_port = htons(PORT);
   ip_protocol = IPPROTO_IP;

   listen_sock = socket(AF_INET, SOCK_STREAM, ip_protocol);
   if (listen_sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      vTaskDelete(NULL);
      return;
   }

   ESP_LOGI(TAG, "Socket created");
   int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
   if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      goto CLEAN_UP;
   }

   err = listen(listen_sock, 1);
   if (err != 0) {
      ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
      goto CLEAN_UP;
   }

   while (1) {
      struct sockaddr_in source_addr;

      uint addr_len = sizeof(source_addr);
      int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
      if (sock < 0) {
         ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
         break;
      }

      _do_request(sock);

      shutdown(sock, 0);
      close(sock);
   }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}


void tcp_server_restart()
{
   if(tcpServer_taskHandle) {
      ESP_LOGI(TAG, "Restarting");
      vTaskDelete(tcpServer_taskHandle);
      tcpServer_taskHandle=NULL;
      if(listen_sock>=0) {
         close(listen_sock);
         listen_sock = -1;
      }
   }
   else {
      ESP_LOGI(TAG, "Starting");
   }
   xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, &tcpServer_taskHandle);
}

//void tcp_server_init(uint8_t mode)
void tcp_server_init(uint8_t mode, tcp_process_callback_t _callback, void *_userdata)
{
    __callback = _callback;
    __userdata = _userdata;
   _mode = mode;
   _mea_config=config_get();
   tcp_server_restart();
}
