#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "relays.h"
#include "contacts.h"
#include "temperature_ds18b20.h"
#include "temperature_dht.h"

#include "xpl_server.h"
#include "xpl_process.h"

static const char *TAG = "xpl_process";

void process_control_basic(char *p, char *v)
{
   if(strlen(p)!=2 || toupper(p[0])!='O' || p[1]<'0' || p[1]>'9') {
      return;
   }

   int8_t _v=-1;
   if(strcasecmp(v, "HIGH")==0
      || strcasecmp(v, "ON")==0
      || strcasecmp(v,"TRUE")==0
      || (is_number(v) && atoi(v)>0)) {
      _v=1;
   }
   else if( strcasecmp(v, "LOW")==0
      || strcasecmp(v, "OFF")==0
      || strcasecmp(v,"FALSE")==0
      || (is_number(v) && atoi(v)==0)) {
      _v=0;
   }

   if(_v>=0) {
      ESP_LOGI(TAG,"xpl: device %s(%d) state set to %d", p, p[1]-'0',  _v);
      relays_set(p[1]-'0', _v);
   }
}


void process_sensor_basic(int16_t sock, char *p)
{
   if(p[0]==0 || p[1]<'0' || p[1]>'9' || p[2]!=0) {
      return;
   }

   int8_t id=p[1]-'0';
   char device[3];
   device[0]=tolower(p[0]);
   device[1]=p[1];
   device[2]=0;

   float fvalue=-9999.9;
   int ivalue=-1;

   switch(device[0]) {
      case 'o':
         ivalue=relays_get(id);
         if(ivalue>=0) {
            xpl_send_current_hl(sock, "stat", device, ivalue);
         }
         break;
      case 'i':
         ivalue=contacts_get(id);
         if(ivalue>=0) {
            xpl_send_current_hl(sock, "stat", device, ivalue);
         }
         break;
      case 't':
         if(id==0) {
            fvalue=temperature_dht_get_t();
         }
         else if(id==1) {
            fvalue=temperature_ds18b20_get();
         }
         if(fvalue!=9999.9) {
            xpl_send_current_float(sock, "stat", device, fvalue);
         }
         break;
      case 'h':
         if(id==0) {
            fvalue=temperature_dht_get_h();
         }
         if(fvalue!=9999.9) {
            xpl_send_current_float(sock, "stat", device, fvalue);
         }
         break;
      default:
         return;
   }
   if(ivalue>=0 || fvalue!=-9999.99) {
      ESP_LOGI(TAG,"xpl: request value for %s", p);
   }
}


int8_t xpl_process_msg(int sock, struct xpl_msg_s *xpl_msg, int nb_values, void *userdata)
{
   if(strcasecmp(xpl_msg[0].section,"XPL-CMND")==0) {
      char *p=xpl_value_p("DEVICE", xpl_msg, nb_values);
      if(p) {
         if(xpl_msg_has_section_name("CONTROL.BASIC", xpl_msg, nb_values)==0) {
            char *c=xpl_value_p("CURRENT", xpl_msg, nb_values);
            if(c) {
               process_control_basic(p,c);
               return 0;
            }
         }
         else if(xpl_msg_has_section_name("SENSOR.REQUEST", xpl_msg, nb_values)==0) {
            char *r=xpl_value_p("REQUEST", xpl_msg, nb_values);
            if(r && strcasecmp(r, "CURRENT")==0) {
               process_sensor_basic(sock, p);
               return 0;
            }
         }
      }
   }
   return -1;
}
