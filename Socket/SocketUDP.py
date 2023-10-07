from socket import *
# serverName = 'localHost'
serverName = '192.168.0.176'
serverPort = 12000

# IP do professor: 192.168.0.176/21 (21 é conteúdo do próximo capítulo)
# Note que para identificação é necessário ip (interface de rede) e porta (processo)

clientSocket = socket(AF_INET, SOCK_DGRAM)
# Neste exemplo vamos acessar o servidor via protocolo de DATAGRAMA (um protocolo da camada de transporte)
# ... por isto o atributo "SOCK_DGRAM" enquanto "AF_INET" é usado para declarar que iremos usar protocolo IP
message = input('Input lowercase sentence:')
clientSocket.sendto(message.encode('utf-8'), (serverName, serverPort))
modifiedMessage, serverAddress = clientSocket.recvfrom(2048)
print(modifiedMessage)
clientSocket.close()
