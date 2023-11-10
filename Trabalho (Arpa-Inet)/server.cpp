#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <pthread.h>

// g++ server.cpp -pthread -o server && ./server

using namespace std;

void *runClient(void *arg);

#define MAX_THREADS_NUM 50

struct thread_arg{
    int id;
    int server_socket; 
    int client_socket; 
    sockaddr_in client; 
    socklen_t client_size;
};

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
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
    // inet_pton(AF_INET, "0.0.0.0", &server.sin_addr);

    if(bind(server_socket, (sockaddr*) &server, sizeof(server)) == -1){
        perror("[-] Could not bind IP/port :( \n");
    }

    // (3) Faz com que o socket possa aceitar um determinado número de conexões:
    if(listen(server_socket, SOMAXCONN) == -1){
        perror("[-] Could not start listening :( \n");
    }

    sockaddr_in client;
    socklen_t client_size = sizeof(client);

    // (4) Criando as threads para lidar com as conexões:
    pthread_t threads[MAX_THREADS_NUM];
    int actual_thread_id = 0;
    int rc;

    while(true){

        if(actual_thread_id >= MAX_THREADS_NUM){
            cout << "[-] Número máximo de threads atingido, não é possível abrir mais conexões!" << endl;
            continue;
        }

        // (5) Aceitar uma conexação:
        // (5.1) Criando um endereço de socket para o cliente:
        struct thread_arg *info = (struct thread_arg *)malloc(sizeof(thread_arg));
        int client_socket = accept(server_socket, (sockaddr *)&info->client, &info->client_size);

        info->id = actual_thread_id;
        info->client_socket = client_socket;

        if(client_socket == -1){
            perror("[-] Problem on accepting a connection");
        }
        else{
            rc = pthread_create(&threads[actual_thread_id], NULL, runClient, (void *) info);
            actual_thread_id++;
        }

    }

    close(server_socket);
    return 0;
}

void *runClient(void *arg){

    struct thread_arg *info;
    info = (struct thread_arg *) arg; 

    int id = info->id;
    int server_socket = info->server_socket;
    int client_socket = info->client_socket;
    sockaddr_in client = info->client;
    socklen_t client_size = info->client_size;
    free(info);

    int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    while(true){
        // (5.2) Limpando o buffer para recebimento de mensagem:
        memset(buffer, 0, BUFFER_SIZE);
        // (5.3) Esperando uma mensagem:
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if(bytes_received == -1){
            perror("[-] Erro na conexão :(");
            close(server_socket);
            pthread_exit(NULL);
        }
        else if(bytes_received == 0){
            cout << "[+] Cliente desconectado (thread = " << id << ")" << endl;
            break;
        }
        else if(strcmp(buffer, "bye") == 0){
            send(client_socket, "bye bro!", sizeof("bye bro"), 0);
            break;
        }
        else{
            // (5.3) Reenviar mensagem para o cliente:
            send(client_socket, buffer, bytes_received + 1, 0);
        }
    }

    cout << "Bye client (thread " <<  id << ")" << endl;

    close(client_socket);
    pthread_exit(NULL);
}