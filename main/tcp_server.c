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

#include "tcp_server.h"
#include "config.h"
#include "contacts.h"
#include "relays.h"
#include "temperature_dht.h"
#include "temperature_ds18b20.h"


int8_t update_relay(uint8_t r);

#define PORT 8081

static const char *TAG = "tcp_server";
static int8_t _mode = 0;
struct mea_config_s *_mea_config = NULL; 

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
      char token[17]="", parameters[81]="";
      char cmd=0;
      int r1=0,r2=0;

      rx_buffer[len]=0;

      int l=strlen(rx_buffer);
      int n=sscanf(rx_buffer,"%20[^:]:%c%n:%80s%n",token,&cmd,&r1,parameters,&r2);
      if(n<2 || n>3) {
         ESP_LOGE(TAG,"Data error");
         send_data(sock,"???");
         return;
      }

      if(_mode==TCP_SERVER_RESTRICTED) {
         if(strcmp(token,_mea_config->token)!=0) {
            ESP_LOGE(TAG,"Not authorized");
            send_data(sock,"BC"); // bad credencial
            return;
         }
      }

      if(n==3 && r2==l) {
         switch(cmd) {
            case 'O': {
               int id,v;
               n=sscanf(parameters, "%d%n/%d%n",&id,&r1,&v,&r2);
               if(n==2 && r2==strlen(parameters)) {
                  relay_set(id, ((v == 0) ? 0 : 1));
                  update_relay(id);
                  ESP_LOGI(TAG,"relay %d set to: %d",id,v);
                  send_data(sock,"OK");
               }
               else if(n==1 && r1==strlen(parameters)) {
                  char str[2];
                  ESP_LOGI(TAG,"relay %d get",id);
                  sprintf(str,"%d",relay_get(id));
                  send_data(sock,str);
               }
               else {
                  send_data(sock,"???");
               } 
               break;
            };
            case 'I': {
               int id,r;
               n=sscanf(parameters, "%d%n",&id,&r); 
               if(n==1 && r==strlen(parameters)) {
                  ESP_LOGI(TAG,"input %d get",id);
                  int8_t v=contact_get(id);
                  if(v<0) {
                     ESP_LOGI(TAG,"KO");
                     send_data(sock,"KO");
                  }
                  else {
                     char s[2]="";
                     s[0] = '0' + ((v == 0) ? 0 : 1);
                     s[1] = 0;
                     send_data(sock,s);
                  }
               }
               else {
                  send_data(sock,"???");
               } 
               break;
            };
            case 'H': {
               int id,r,h;
               n=sscanf(parameters, "%d%n",&id,&r); 
               if(n==1 && r==strlen(parameters) && id==0) {
                  char str[5];
                  ESP_LOGI(TAG,"humidity %d get",id);
                  h=(int)temperature_dht_get_h();
                  sprintf(str,"%d",h);
                  send_data(sock,str);
               }
               else {
                  send_data(sock,"???");
               }
               break;
            }
            case 'T': {
               int id,r;
               char str[10];
               n=sscanf(parameters, "%d%n",&id,&r); 
               if(n==1 && r==strlen(parameters) && (id==0 || id==1)) {
                  ESP_LOGI(TAG,"temperature %d get",id);
                  if(id==0) {
                     sprintf(str,"%d",(int)temperature_dht_get_t());
                  }
                  else {
                     sprintf(str,"%.1f",temperature_ds18b20_get());
                  }
                  send_data(sock,str);
               }
               else {
                  send_data(sock,"???");
               }
               break;
            }
            case 'W': {
               ESP_LOGI(TAG, "WIFI setting ...");
               int r=0;
               char ssid[41], password[41];
               n=sscanf(parameters,"%40[^/]/%40s%n",ssid,password,&r);
               if(n==2 && r==strlen(parameters)) { 
                  set_mea_config_wifi(ssid, password);
                  send_data(sock,"OK");
                  ESP_LOGI(TAG, "WIFI setting done");
               }
               else {
                  send_data(sock,"KO");
                  ESP_LOGI(TAG, "WIFI setting KO");
               }
               break;
            };
            default:
               send_data(sock,"???");
               ESP_LOGW(TAG, "bad command");
               break;
      
         }
      }
      else if(n==2 && r1==l) {
         switch(cmd) {
            case 'w':
               {
                  char s[81]="";

                  sprintf(s,"WIFI_SSID=%s\n",_mea_config->wifi_ssid);
                  send_data(sock,s);
                  sprintf(s,"HOMEKIT_NAME=%s\n",_mea_config->accessory_name);
                  send_data(sock,s);
                  sprintf(s,"HOMEKIT_PASSWORD=%s\n",_mea_config->accessory_password);
                  send_data(sock,s);
               }
               break;
            case 't': // get token
               {
                  char s[81]="";
                  if(_mode==TCP_SERVER_CONFIG) {
                     sprintf(s,"TOKEN=%s\n",_mea_config->token);
                     send_data(sock,s);
                  }
                  else {
                     send_data(sock,"NA");
                  }
               }
               break;
            case 'C': // reset wifi configuration
               reset_mea_config_wifi();
               send_data(sock,"OK");
               if(_mode == TCP_SERVER_RESTRICTED) {
                  vTaskDelay(1000 / portTICK_PERIOD_MS);
                  esp_restart();
               }
               break;
            case 'R': // restart (reboot)
               ESP_LOGW(TAG, "Restart...");
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
   int ip_protocol = 0;
   struct sockaddr_in dest_addr;

   struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
   dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
   dest_addr_ip4->sin_family = AF_INET;
   dest_addr_ip4->sin_port = htons(PORT);
   ip_protocol = IPPROTO_IP;

   int listen_sock = socket(AF_INET, SOCK_STREAM, ip_protocol);
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

      do_request(sock);

      shutdown(sock, 0);
      close(sock);
   }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}


void tcp_server_init(uint8_t mode)
{
   _mode = mode;
   _mea_config=get_mea_config();
   xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);
}
