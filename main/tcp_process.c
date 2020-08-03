#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "tcp_server.h"
#include "tcp_process.h"

#include "config.h"

#include "contacts.h"
#include "relays.h"
#include "temperature_dht.h"
#include "temperature_ds18b20.h"

#include <homekit/homekit.h>


static const char *TAG = "tcp_process";


int tcp_process(int sock, struct mea_config_s *mea_config, int8_t mode, char cmd, char *parameters, void *userdata)
{
   if(tcp_network_config(sock, mea_config, mode, cmd, parameters)) {
      return 1;
   }
   
   if(parameters) {
      switch(cmd) {
         case 'O': {
            int id,v,r1,r2;
            int n=sscanf(parameters, "%d%n/%d%n",&id,&r1,&v,&r2);
            if(n==2 && r2==strlen(parameters)) {
               relays_set(id, ((v == 0) ? 0 : 1));
               ESP_LOGI(TAG,"relay %d set to: %d",id,v);
               tcp_send_data(sock,"OK");
            }
            else if(n==1 && r1==strlen(parameters)) {
               char str[2];
               ESP_LOGI(TAG,"relay %d get",id);
               sprintf(str,"%d",relays_get(id));
               tcp_send_data(sock,str);
            }
            else {
               tcp_send_data(sock,"???");
            } 
            return 1;
         };
         case 'I': {
            int id,r;
            int n=sscanf(parameters, "%d%n",&id,&r); 
            if(n==1 && r==strlen(parameters)) {
               ESP_LOGI(TAG,"input %d get",id);
               int8_t v=contacts_get(id);
               if(v<0) {
                  ESP_LOGI(TAG,"KO");
                  tcp_send_data(sock,"KO");
               }
               else {
                  char s[2]="";
                  s[0] = '0' + ((v == 0) ? 0 : 1);
                  s[1] = 0;
                  tcp_send_data(sock,s);
               }
            }
            else {
               tcp_send_data(sock,"???");
            } 
            return 1;
         };
         case 'H': {
            int id,r,h;
            int n=sscanf(parameters, "%d%n",&id,&r); 
            if(n==1 && r==strlen(parameters) && id==0) {
               char str[5];
               ESP_LOGI(TAG,"humidity %d get",id);
               h=(int)temperature_dht_get_h();
               sprintf(str,"%d",h);
               tcp_send_data(sock,str);
            }
            else {
               tcp_send_data(sock,"???");
            }
            return 1;
         }
         case 'T': {
            int id,r;
            char str[10];
            int n=sscanf(parameters, "%d%n",&id,&r); 
            if(n==1 && r==strlen(parameters) && (id==0 || id==1)) {
               ESP_LOGI(TAG,"temperature %d get",id);
               if(id==0) {
                  sprintf(str,"%d",(int)temperature_dht_get_t());
               }
               else {
                  sprintf(str,"%.1f",temperature_ds18b20_get());
               }
               tcp_send_data(sock,str);
            }
            else {
               tcp_send_data(sock,"???");
            }
            return 1;
         }
      }
   }
   else {
      switch(cmd) {
         case 'r': { // reset to factory
            ESP_LOGW(TAG, "reset to factory defaults ...");
            tcp_send_data(sock, OK_STATUS);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            homekit_server_reset();
            nvs_flash_erase();
            esp_restart();
            for(::);
         }
      }
   }
   return 0;
}
