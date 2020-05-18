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

#include "xpl_server.h"


void xpl_send_hbeat(int fd, char *source, char *type, int interval, char *version);

#define PORT 3865

static const char *TAG = "xpl_server";

static char *xpl_source = NULL;
static char *xpl_version = "0.0.1";
static int xpl_interval = 60; // seconds


struct xpl_msg_s xpl_msg[XPL_MAX_MESSAGE_ITEMS+1], *pxpl_msg=&xpl_msg[0];


int8_t set_xpl_source(char *s)
{
   if(xpl_source) {
      free(xpl_source);
   }
   xpl_source=malloc(strlen(s)+1);
   if(!xpl_source) {
      return -1;
   }
   strcpy(xpl_source,s);

   return 0;
}


int8_t xpl_parser(const char *xplmsg, struct xpl_msg_s *pxpl_msg)
{
#define XPL_IN_SECTION 0
#define XPL_IN_NAME 1
#define XPL_IN_VALUE 2

   int y, z, c;
   int w;
   char xpl_section[35];
   char xpl_name[17];
   char xpl_value[XPL_MAX_VALUE_SIZE];
   unsigned int xpl_msg_index = 0;
   
   w = XPL_IN_SECTION;
   z = 0;
   y = (int)strlen(xplmsg);
   for (c=0;c<y;c++) {
      
      switch(w) {
         case XPL_IN_SECTION:
            if((xplmsg[c] != 10) && (z != 34)) {
               xpl_section[z]=xplmsg[c];
               z++;
            }
            else {
               c++;
               c++;
               w++;
               xpl_section[z]='\0';
               z=0;
            }
            break;
         case XPL_IN_NAME:
            if((xplmsg[c] != '=') && (xplmsg[c] != '!') && (z != 16)) {
               if(z<16) {
                  xpl_name[z]=xplmsg[c];
                  z++;
               }
            }
            else {
               w++;
               xpl_name[z]='\0';
               z=0;
            }
            break;
         case XPL_IN_VALUE:
            if((xplmsg[c] != 10) && (z != 128)) {
               if(z<128) {
                  xpl_value[z]=xplmsg[c];
                  z++;
               }
            }
            else {
               w++;
               xpl_value[z]='\0';
               z=0;
               if(xpl_msg_index > XPL_MAX_MESSAGE_ITEMS) {
                  return(0);
               }
               strncpy(pxpl_msg[xpl_msg_index].section, xpl_section, sizeof(pxpl_msg[xpl_msg_index].section)-1);
               strncpy(pxpl_msg[xpl_msg_index].name, xpl_name, sizeof(pxpl_msg[xpl_msg_index].name)-1);
               strncpy(pxpl_msg[xpl_msg_index].value, xpl_value, XPL_MAX_VALUE_SIZE-1);
               xpl_msg_index++;
               if(xplmsg[c+1] != '}') {
                  w=XPL_IN_NAME;
               }
               else {
                  w=XPL_IN_SECTION;
                  c++;
                  c++;
               }
            }
            break;
      }
   }
   if(xpl_msg_index<3) {
      xpl_msg_index = 0;
   }
   return xpl_msg_index;
}


char *xpl_value_p(const char *xpl_name, struct xpl_msg_s *pxpl_msg, unsigned int xpl_msg_index)
{
   unsigned int c=0;

   while (c<xpl_msg_index) {
      if(strcasecmp(pxpl_msg[c].name,xpl_name)==0) {
         return pxpl_msg[c].value;
      }
      c++;
   }
   return NULL;
}


int8_t xpl_msg_has_section_name(char *section, struct xpl_msg_s *pxpl_msg, int8_t nb_values)
{
   for(int8_t i=0;i<nb_values;i++) {
      if(strcasecmp(section,pxpl_msg[i].name)==0) {
         return 0;
      }
   }
   return -1;
}


int8_t xpl_process_msg(int sock, struct xpl_msg_s *xpl_msg, int nb_values)
{
   char *s = xpl_value_p("target", pxpl_msg, nb_values);

   if(!s || (strcasecmp(s,"*")!=0 && strcasecmp(s,xpl_source)!=0) ) {
      return -1;
   } 

   if(strcasecmp(pxpl_msg[0].section,"XPL-CMND")==0) {
      if(xpl_msg_has_section_name("HBEAT.REQUEST", pxpl_msg, nb_values)==0) {
         s=xpl_value_p("COMMAND", pxpl_msg, nb_values);
         if(s && strcasecmp(s,"REQUEST")==0) { /* xpl-cmnd { hop=1 source=xpl-xxx target=* } hbeat.request { command=request } */
            xpl_send_hbeat(sock, xpl_source, "basic", xpl_interval, xpl_version); 
            return 0;
         }
      }
   }
   return -1;
}


int16_t xpl_send_msg(int16_t fd, char *data, int l_data)
{
   struct sockaddr_in _send_addr;
   _send_addr.sin_family = AF_INET,
   _send_addr.sin_port = htons(PORT);
   _send_addr.sin_addr.s_addr  = INADDR_BROADCAST;
   _send_addr.sin_len = sizeof(_send_addr);

   int r=sendto(fd, data, l_data, 0, (const struct sockaddr*)&_send_addr, sizeof(_send_addr));

   return r;
}


int16_t xpl_read_msg(int16_t fd, int32_t timeoutms, char *data, int l_data)
{
   fd_set input_set;
   struct timeval timeout = {0,0};
   
   FD_ZERO(&input_set);
   FD_SET(fd, &input_set);
   
   timeout.tv_sec = 0;
   timeout.tv_usec = timeoutms * 1000;
   
   int ret = select(fd+1, &input_set, NULL, NULL, &timeout);
   if(ret<=0) {
      if(ret != 0) {
         ESP_LOGI(TAG, "Select error");
         ret=-1;
      }
      goto on_error_xpl_read_msg_exit;
   }
   
   ret = (int)read(fd, data, l_data);
//   ESP_LOGI(TAG, "read %d byte(s)", ret);

on_error_xpl_read_msg_exit:
   return ret;
}


void xpl_send_hbeat(int fd, char *source, char *type, int interval, char *version)
{
   char msg[256];

   char *hbeatMsg = "xpl-stat\n{\nhop=1\nsource=%s\ntarget=*\n}\nhbeat.%s\n{\ninterval=%d\nversion=%s\n}\n";
   sprintf(msg, hbeatMsg, source, type, interval, version);
   
   xpl_send_msg(fd, msg, strlen(msg));
}


static void xplhb_timer_callback(void* arg)
{
//   ESP_LOGI(TAG, "HB");
   xpl_send_hbeat((int)arg, xpl_source, "basic", xpl_interval, xpl_version);
}


static void xpl_server_task(void *pvParameters)
{
   struct sockaddr_in dest_addr;
   int flag = 1;
   int addr_family = AF_INET;
   esp_timer_handle_t xplhb_timer = 0;


   bzero(&dest_addr,sizeof(dest_addr));
   dest_addr.sin_family = AF_INET;
   dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   dest_addr.sin_port = htons(PORT);

   int sock = socket(addr_family, SOCK_DGRAM, IPPROTO_IP);
   if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      goto CLEAN_UP;
   }

   if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &flag, (socklen_t)sizeof(flag)) < 0) {
      ESP_LOGI(TAG, "unable to set SO_BROADCAST on socket: errno %d\n", errno);
      goto CLEAN_UP;
   }

   int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
   if (err < 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      goto CLEAN_UP;
   }

   const esp_timer_create_args_t xplhb_timer_args = {
      .callback = &xplhb_timer_callback,
      .name = "xplhb",
      .arg = (void *)sock
   };
   ESP_ERROR_CHECK(esp_timer_create(&xplhb_timer_args, &xplhb_timer));
   ESP_ERROR_CHECK(esp_timer_start_periodic(xplhb_timer, xpl_interval * 1000000));

   xpl_send_hbeat(sock, xpl_source, "basic", xpl_interval, xpl_version);

   char data[1024];
   int xpl_msg_index = 0;
   while (1) {
      int r=xpl_read_msg(sock, 250, data, sizeof(data));
      if(r>0) {
         data[r]=0;
         xpl_msg_index=xpl_parser(data, pxpl_msg);
         if(xpl_msg_index > 0) {
            xpl_process_msg(sock, pxpl_msg, xpl_msg_index);
         }
      }
      else if(r==0) {
         // timeout
      }
      else if(r<0) {
         ESP_LOGI(TAG, "ERROR");
      }
   }


CLEAN_UP:
   ESP_ERROR_CHECK(esp_timer_stop(xplhb_timer));
   ESP_ERROR_CHECK(esp_timer_delete(xplhb_timer));
   close(sock);
   vTaskDelete(NULL);
}


void xpl_server_init(char *source)
{
   if(set_xpl_source(source)==0) {
      xTaskCreate(xpl_server_task, "xpl_server", 4096, (void*)NULL, 3, NULL);
   }
}
