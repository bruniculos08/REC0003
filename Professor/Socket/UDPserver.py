from socket import *

serverPort = 12000
serverSocket = socket(AF_INET, SOCK_DGRAM)
serverSocket.bind(('', serverPort))
print("REC0003 - servidor UDP pronto")

while 1:
    message, clientAddress = serverSocket.recvfrom(2048)
    modifiedMessage = "%s ... %s" %(str(message), str(message.upper()))
    serverSocket.sendto(modifiedMessage.encode('utf-8'), clientAddress)
