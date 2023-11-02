import socket
import threading

PORT = 5050
# (1) gethostname returns de localhost name and gethostbyname returns...
# ... the IP address of this host:
SERVER = socket.gethostbyname(socket.gethostname())

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ADDR = (SERVER, PORT)

print(socket.gethostname())
print(SERVER)