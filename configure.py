import socket
import sys

# HOST = '192.168.0.45'
HOST = '192.168.4.1'
# HOST ="led-33066C-22.local"
PORT = 8081

TOKEN = "dKkMOCB1HOyklnmAQNS0"

def do_request(host, port, command):
   print command
   client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
   client.connect((host, port))
   n = client.send(message)
   if (n == len(message)):
      donnees=""
      while(1):
         d = client.recv(4096)
         if len(d) > 0:
            donnees=donnees+d
         else:
            break;
   else:
      donnees = False;
   print "Recu:",donnees

   client.close()

   return donnees

   
message = TOKEN+':W:france2/wificdpii1000'
do_request(HOST,PORT,message) 
message = TOKEN+':R'
do_request(HOST,PORT,message) 
sys.exit(0)

message = TOKEN+':O:3'
s=do_request(HOST,PORT,message) 
if(s=='1'):
   v=0
else:
   v=1

message = TOKEN+':O:3/'+str(v)
do_request(HOST,PORT,message) 

message = TOKEN+':O:3'
s=do_request(HOST,PORT,message) 

message = TOKEN+':I:3'
do_request(HOST,PORT,message) 

message = TOKEN+':T:0'
do_request(HOST,PORT,message) 

message = TOKEN+':T:1'
do_request(HOST,PORT,message) 

message = TOKEN+':H:0'
do_request(HOST,PORT,message) 
