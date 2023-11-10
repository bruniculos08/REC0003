#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>

using namespace std;
#define LEN 4096

char *discoverIPv4(char *url, const char *port){
    // (1) Estrutura para salvar os endereços de IP 
    addrinfo *addresses_found = NULL;
    // (2) getaddrinfo() faz uma consulta DNS recebendo uma URL e uma porta e coloca....
    // ... as informações em uma variável do tipo addrinfo:
    getaddrinfo("0.tcp.sa.ngrok.io", "14832", 0, &addresses_found);

    // OBS.: a variável do tipo addrinfo() é uma lista encadeada em que cada nó contém...
    // ... um endereço e informações sobre este endereço.

    // (3) Variável para andar pela lista encadeada:
    addrinfo *actual_address;
    actual_address = addresses_found;

    // (4) String para guarda o IPv4;
    // char *IPv4 = (char *)malloc(sizeof(char)*INET6_ADDRSTRLEN);
    char IPv4[INET6_ADDRSTRLEN];

    // (5) Enquanto o nó atual não for nulo (não se chegou no fim da lista encadeada):
    while(actual_address != NULL){
        // (5.1) Se o socket contido no nó da lista é de ipv4:
        if(actual_address->ai_addr->sa_family == AF_INET){
            // (5.1.2) Cria uma variável de socket de internet:
            sockaddr_in *internet_socket_address = (struct sockaddr_in *) actual_address->ai_addr;
            // (5.1.3) Converte o endereço ipv4 em binário no socket para uma string e coloca...
            // ... na variável IPv4:
            inet_ntop(AF_INET, &internet_socket_address->sin_addr, IPv4, sizeof(IPv4));
            // (5.1.4) Coloca a string de ipv4 em um endereço na memória a ser retornado pela função:
            char *str = (char *)malloc(sizeof(char) * INET6_ADDRSTRLEN);
            strcpy(str, IPv4);
            cout << str << endl;
            // (5.1.4) Retorna o endereço da string com o endereço ipv4:
            return str;
        }
        // (5.2) Anda para o próximo nó da lista:
        actual_address = actual_address->ai_next;
    }
    // (6) Se não for encontrado nenhum endereço ipv4 retorna NULL:
    return NULL;
}

int main(){

    // (1) Criar um socket:
    int my_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(my_socket == -1){
        perror("[-] Could not create socket :(");
        return EXIT_FAILURE;
    }

    // (2) Aqui criamos a struct com o endereço do servidor:
    sockaddr_in server_address;
    // (2.1) A familia AF_INET indica uso de IPV4: 
    server_address.sin_family = AF_INET;
    // (2.2) htons() converte um inteiro para formato u_int16 (portas são representadas por valores deste tipo):
    server_address.sin_port = htons(14832);
    // server_address.sin_port = htons(19664);

    // (2.3) Encontra o endereço ipv4 por meio de requisição DNS:
    char url[100] = "0.tcp.sa.ngrok.io:14832";
    char *IPv4 = discoverIPv4(url, "14832");
    cout << "IPv4 address found: " << IPv4 << endl;

    // (2.4) inet_pton() converte um endereço de formato em texto ("localhost" ou "127.0.0.1") para seu formato em...
    // ... binário e armazena no buffer passado como parâmetro (server.sin_addr):
    // inet_pton(AF_INET, "127.0.1.1", &server_address.sin_addr);
    // inet_pton(AF_INET, "18.231.93.153", &server_address.sin_addr);
    inet_pton(AF_INET, IPv4, &server_address.sin_addr);

    // (3) Conectar com o servidor:
    if(connect(my_socket, (sockaddr *)&server_address, sizeof(server_address)) == -1){
        perror("[-] Cant connect to the server");
        return EXIT_FAILURE;
    }

    // (4) Comunicação com o servidor:
    char receive_buffer[LEN];
    char send_buffer[LEN];
    int msg_len;

    while (true)
    {
        memset(receive_buffer, 0, LEN);
        memset(send_buffer, 0, LEN);

        // (4.1) Enviando uma mensagem para o servidor:
        cout << "[+] Type something to the server: ";
        cin >> send_buffer;

        send(my_socket, send_buffer, strlen(send_buffer), 0);

        msg_len = recv(my_socket, receive_buffer, LEN, 0);
        cout << "[+] Server answer: " << receive_buffer << endl;

        if(strcmp(receive_buffer, "bye") == 0 or strcmp(send_buffer, "bye") == 0){
            break;
        }
    }

    close(my_socket);
    cout << "[+] Connection closed." << endl;
    return EXIT_SUCCESS;    
}