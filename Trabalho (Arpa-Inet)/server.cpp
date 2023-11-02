#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <pthread.h>

using namespace std;

int main(){

    // (1) Criar um socket:
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1){
        perror("[-] Could not create socket :( \n");
    }

    // (2) Vincular (Bind) o socket a um IP e uma porta:
    sockaddr_in server;
    // (2.1) A familia AF_INET indica uso de IPV4: 
    server.sin_family = AF_INET;
    // (2.2) htons() converte um inteiro para formato u_int16 (portas são representadas por valores deste tipo):
    server.sin_port = htons(60000);
    // (2.3) inet_pton() converte um endereço de formato em texto ("localhost" ou "127.0.0.1") para seu formato em...
    // ... binário e armazena no buffer passado como parâmetro (server.sin_addr):
    inet_pton(AF_INET, "127.0.1.1", &server.sin_addr);
    // inet_pton(AF_INET, "0.0.0.0", &server.sin_addr);

    if(bind(server_socket, (sockaddr*) &server, sizeof(server)) == -1){
        perror("[-] Could not bind IP/port :( \n");
    }

    // (3) Faz com que o socket possa aceitar um determinado número de conexões:
    if(listen(server_socket, SOMAXCONN) == -1){
        perror("[-] Could not start listening :(");
    }

    // (4) Aceitar uma conexação:
    // (4.1) Criando um endereço de socket para o cliente
    sockaddr_in client;
    socklen_t client_size = sizeof(client);
    // (4.2) Buffers para o host name e server name (qual iremos obter mais adiante):
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];

    int client_socket = accept(server_socket, (sockaddr *)&client, &client_size);
    if(client_socket == -1){
        perror("[-] Problem on accepting a connection");
    }

    // (4.3) Limpando os buffers e a variável de file descriptor (desassocia o arquivo do file descriptor):
    close(server_socket);
    memset(host, 0, NI_MAXHOST);
    memset(serv, 0, NI_MAXSERV);

    // (5) Obtendo o nome do cliente:
    // (5.1) A função getnameinfo() traduz um socket address para uma localização (nome) e um nome de serviço:
    int result = getnameinfo((sockaddr *)&client_socket, client_size, host, NI_MAXHOST, serv, NI_MAXSERV, 0);

    if(result){
        cout << "[+] " << host << " connected on " << serv << endl;
    }

    int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    while(true){
        // (5.2) Limpando o buffer para recebimento de mensagem:
        memset(buffer, 0, BUFFER_SIZE);
        // (5.3) Esperando uma mensagem:
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if(bytes_received == -1){
            perror("[-] Erro na conexão :(");
        }
        else if(bytes_received == 0){
            cout << "[+] Cliente desconectado :(" << endl;
            break;
        }
        else if(strcmp(buffer, "bye") == 0){
            send(client_socket, "bye bro!", sizeof("bye bro"), 0);
        }
        else{
            // (5.3) Reenviar mensagem para o cliente:
            send(client_socket, buffer, bytes_received + 1, 0);
        }
    }

    cout << "Bye!" << endl;
    return 0;
}