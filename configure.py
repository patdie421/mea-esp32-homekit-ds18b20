import socket

# HOST = '192.168.0.45'
HOST = '192.168.4.1'
# HOST ="led-33066C-22.local"

PORT = 8081
TOKEN = "sfsrezrfsdg"
message = TOKEN+':W:france2/wificdpii1000'
#message = TOKEN+':C'
#message = TOKEN+':R'
client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect((HOST, PORT))
print 'Connexion vers ' + HOST + ':' + str(PORT) + ' reussie.'

print 'Envoi de :' + message

n = client.send(message)
if (n != len(message)):
   print 'Erreur envoi.'
else:
   print 'Envoi ok.'

print 'Reception...'
donnees = client.recv(1024)
print 'Recu :', donnees

print 'Deconnexion.'
client.close()
