/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
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

#include "config.h"

#define PORT 8081

static const char *TOKEN = "sfsrezrfsdg";
static const char *TAG = "tcp_server";


void send_data(const int sock, char *data)
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


static void do_request(const int sock)
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
      rx_buffer[len]=0;
      ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

      char token[17], parameters[81];
      char cmd;
      int r=0; 
      int n=sscanf(rx_buffer,"%16[^:]:%c:%80s%n",token,&cmd,parameters,&r);
      if(n==3) {
         switch(cmd) {
            case 'W': {
               ESP_LOGI(TAG, "WIFI set ...");
               char ssid[41], password[41];
               n=sscanf(parameters,"%40[^/]/%40s%n",ssid,password,&r);
               if(n==2 && r==strlen(parameters)) { 
                  set_mea_config_wifi(ssid, password);
                  send_data(sock,"OK");
                  ESP_LOGI(TAG, "WIFI set OK");
               }
               else {
                  send_data(sock,"KO");
                  ESP_LOGI(TAG, "WIFI set KO");
               }
               break;
            };
            default:
               send_data(sock,"???");
               ESP_LOGW(TAG, "bad command");
               break;
      
         }
      }
      else if(n==2) {
         switch(cmd) {
            case 'C':
               reset_mea_config_wifi();
               send_data(sock,"OK");
               break;
            case 'R':
               ESP_LOGW(TAG, "Restart ...");
               send_data(sock,"OK");
               ESP_LOGW(TAG, "Restart OK");
               vTaskDelay(1000 / portTICK_PERIOD_MS);
               esp_restart();
               break;
            default:
               send_data(sock,"???");
               ESP_LOGW(TAG, "bad command");
               break;
         }
      }
      else {
         send_data(sock,"!!!");
         ESP_LOGW(TAG, "bad request");
      }
   }
}


static void tcp_server_task(void *pvParameters)
{
   int addr_family = (int)pvParameters;
   int ip_protocol = 0;
   struct sockaddr_in dest_addr;

   struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
   dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
   dest_addr_ip4->sin_family = AF_INET;
   dest_addr_ip4->sin_port = htons(PORT);
   ip_protocol = IPPROTO_IP;

   int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
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
      struct sockaddr_in source_addr; // Large enough for both IPv4 or IPv6
      uint addr_len = sizeof(source_addr);
      int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
      if (sock < 0) {
         ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
         break;
      }


      do_request(sock);

      shutdown(sock, 0);
      close(sock);
   }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}


void tcp_server_init()
{
   xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);
}
