from socket import *

serverName = '192.168.0.176'
serverPort = 12000
clientSocket = socket(AF_INET, SOCK_STREAM)
clientSocket.connect((serverName, serverPort))

sentence = input('Input lowercase sentence: ')
clientSocket.sendall(sentence.encode('utf-8'))

modifiedSentence = clientSocket.recv(1024)
# 1024 é o tamanho do buffer da camada de aplicação
print ('From server: ', modifiedSentence)
clientSocket.close()
