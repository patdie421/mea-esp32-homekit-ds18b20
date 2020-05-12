import socket

#HOST = '192.168.0.45'
HOST = '192.168.4.1'
# HOST ="led-33066C-22.local"
PORT = 8081

TOKEN = "x"

def do_request(host, port, command):
   client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
   client.connect((host, port))
   n = client.send(message)
   if (n == len(message)):
      print 'Recu :'
      donnees=""
      while(1):
         d = client.recv(4096)
         if len(d) > 0:
            donnees=donnees+d
         else:
            break;
   else:
      donnees = False;
   print donnees
   client.close()
   if donnees=="OK":
      return True;
   return False

   
message = TOKEN+':W:france2/wificdpii1000'
#message = TOKEN+':O:1/1'
#message = TOKEN+':I:3'
#message = TOKEN+':R'

do_request(HOST,PORT,message) 
