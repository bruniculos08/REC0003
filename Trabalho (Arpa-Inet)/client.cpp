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
    server_address.sin_port = htons(60001);
    // (2.3) inet_pton() converte um endereço de formato em texto ("localhost" ou "127.0.0.1") para seu formato em...
    // ... binário e armazena no buffer passado como parâmetro (server.sin_addr):
    inet_pton(AF_INET, "127.0.1.1", &server_address.sin_addr);

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