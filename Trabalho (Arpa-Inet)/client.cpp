#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <bits/stdc++.h>
#include <regex>
#include <dirent.h>
#include <errno.h>

using namespace std;
#define LEN 4096
#define PACKET 256

int receiveListOfStrings(vector<string> &listResponse, int my_socket);
int sendUpdate(int my_socket, FILE *fptr);
int getFileSize(FILE *fptr);
int getDownload(int my_socket, FILE *fptr);
int checkPath(char path[]);

FILE *existsFile(char path[],char file_name[]);
FILE *openFile(char path[], char file_name[]);

regex point_pattern{"[. ]+"};
regex list_pattern{"list[ ]*"};
regex exit_pattern{"exit[ ]*"};
regex upload_pattern{"upload [^/]+"};
regex delete_pattern{"delete [^/]+"};
regex shutdown_pattern{"shutdown[ ]*"};
regex download_pattern{"download [^/]+[.][a-zA-Z0-9]+ [^]+"};

char client_dir[LEN];

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

    cout << "[+] Choose the client dir (ex.: my_folder/): " << flush;
    string temp;
    getline(cin, temp);
    strcpy(client_dir, temp.c_str());
    cin.clear();

    label_restart_connection:

    // (1) Criar um socket:
    int my_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(my_socket == -1){
        cout << "[-] Could not create socket" << endl;
        return 1;
    }

    // (2) Aqui criamos a struct com o endereço do servidor:
    sockaddr_in server_address;
    // (2.1) A familia AF_INET indica uso de IPV4: 
    server_address.sin_family = AF_INET;
    // (2.2) htons() converte um inteiro para formato u_int16 (portas são representadas por valores deste tipo):
    server_address.sin_port = htons(60000);

    // (2.3) Encontra o endereço ipv4 por meio de requisição DNS:
    // char url[] = "0.tcp.sa.ngrok.io:14832";
    // char *IPv4 = discoverIPv4(url, "14832");
    // cout << "IPv4 address found: " << IPv4 << endl;

    // (2.4) inet_pton() converte um endereço de formato em texto ("localhost" ou "127.0.0.1") para seu formato em...
    // ... binário e armazena no buffer passado como parâmetro (server.sin_addr):
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    // (3) Conectar com o servidor:
    if(connect(my_socket, (sockaddr *)&server_address, sizeof(server_address)) == -1){
        cout << "[-] Cant connect to the server" << endl; 
        return 1;
    }

    // (4) Comunicação com o servidor:
    char receive_buffer[LEN];
    char send_buffer[LEN];
    int msg_len;

    while (true)
    {
        memset(receive_buffer, 0, LEN);
        memset(send_buffer, 0, LEN);

        // (5) Enviando uma mensagem para o servidor:
        cout << "[+] Type something to the server: " << flush;

        // (6) Converte a entrada de string para um array de char:
        string temp;
        getline(cin, temp);
        strcpy(send_buffer, temp.c_str());
        cin.clear();

        // (7) Verificar se a entrada é um comando de upload:
        if(regex_match(send_buffer, upload_pattern)){
            // (7.1) Coloca os argumentos do comando upload em variáveis:
            char command[sizeof("update")];
            char file_name[256];
            sscanf(send_buffer, "%s %[^\n]", command, file_name);

            FILE *fptr = existsFile(client_dir, file_name);

            if(fptr != NULL){
                // (7.2) Envia o comando ao servidor:
                send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
                // (7.3) Chama a função para enviar arquivo:
                int flag = sendUpdate(my_socket, fptr);
                // (7.4) Volta ao incio e tenta reiniciar a conexão:
                if(flag == 1){
                    cout << "Erro de conexão, upload cancelado." << endl;
                    cout << "Tentando reconectar ao servidor..." << endl;
                    goto label_restart_connection;
                } 
            }
            else cout << "Tried to upload non existing file." << endl;
            continue;
        }
        // (8) Verificar se a entrada é um comando de delete:
        else if(regex_match(send_buffer, delete_pattern)){
            send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
            msg_len = recv(my_socket, receive_buffer, LEN, 0);

            if(msg_len == 0){
                    cout << "Connection error." << endl;
                    cout << "Trying to reconnect to the server..." << endl;
                    goto label_restart_connection;
            }

            cout << receive_buffer << endl;
            continue;
        }
        // (9) Verificar se a entrada é um comando de download:
        else if(regex_match(send_buffer, download_pattern)){
            // (9.1) Coloca os argumentos do comando upload em variáveis:
            char command[sizeof("download")];
            char file_init[256];
            char file_ext[256];
            char path[256];
            sscanf(send_buffer, "%s %[^.]%[^ ] %[^\n]", command, file_init, file_ext, path);

            // (9.2) Remonta o nome do arquivo a ser baixado:
            char file_name[strlen(file_init) + strlen(file_ext) + 1];
            // Obs.: o tamanho do char array deve-ser o número de caracteres + 1 (provavelmente pois termina...
            // ... com char "\0").
            strcpy(file_name, file_init);
            strcat(file_name, file_ext);

            // (9.3) Se o caminho é inválido não envia o comando ao servidor:
            if(checkPath(path) == 1){
                cout << "[-] Invalid path." << endl;
                continue;
            }
            
            // (9.4) Verifica se o arquivo existe no servidor:
            send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
            msg_len = recv(my_socket, receive_buffer, LEN, 0);

            // (9.5) Tenta fazer download do arquivo:
            if(strcmp(receive_buffer, "Yes") == 0){
                FILE *fptr = openFile(path, file_name);
                int flag = getDownload(my_socket, fptr);
                fclose(fptr);
                if(flag == 1){
                    cout << "[-] Conection error, download canceled." << endl;
                    cout << "[-] Trying to reconnect to the server..." << endl;
                    goto label_restart_connection;
                }
            }
            else cout << "[-] Tried to download non-existing file." << endl;
            continue;
        }
        // (10) Verifica se a entrada é um comando de list:
        else if(regex_match(send_buffer, list_pattern)){
            send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
            cout << "[+] List: " << endl;

            vector<string> listResponse;
            int flag = receiveListOfStrings(listResponse, my_socket);

            if(flag == 1){
                    cout << "[-] Conection error, list failed." << endl;
                    cout << "[-] Trying to reconnect to the server..." << endl;
                    goto label_restart_connection;
                }

            for(string x : listResponse){
                cout << x << endl;
            }
            continue;
        }
        // (11) Verifica se a entrada é um comando de exit:
        else if(regex_match(send_buffer, exit_pattern)){
            send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
            msg_len = recv(my_socket, receive_buffer, LEN, 0);
            cout << "[+] Server answer: " << receive_buffer << endl;
            break;
        }
        // (12) Envia algum texto e recebe o mesmo do servidor:
        send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
        msg_len = recv(my_socket, receive_buffer, LEN, 0);
        
        if(strcmp(receive_buffer, "") == 0){
            cout << "[+] Server has been shutdown... " << endl;
            break;
        }
        cout << "[+] Server answer: " << receive_buffer << " [invalid sintax or not a command]" << endl;
    }

    close(my_socket);
    cout << "[+] Connection closed." << endl;
    return 0;    
}

string convertToString(char *array, int size){
    string s = "";
    for(int i = 0; i < size; i++){
        s = s + array[i];
    }
    return s;
}

int receiveListOfStrings(vector<string> &words, int my_socket){

    // (0) Cria buffer de recebimento de mensagem:
    char receive_buffer[LEN];
    memset(receive_buffer, 0, LEN);
    int msg_len;

    // (1) Recebe o número aleatório e reenvia:
    int random_check_len = recv(my_socket, receive_buffer, LEN, 0);
    if(random_check_len == 0){
        return 1;
    }

    // (2) Reenvia o número aleatório:
    send(my_socket, receive_buffer, random_check_len + 1, 0);

    // (3) Copia o número aleatório para um buffer especial:
    char random_check[LEN];
    memset(random_check, 0, LEN);
    strcpy(random_check, receive_buffer);

    while(true){

        // (4) Recebe a primeira mensagem (char array):
        msg_len = recv(my_socket, receive_buffer, LEN, 0);

        // (5) Verifica se a mensagem (char array) é o número aleatório ou se é vazia:
        if(strcmp(receive_buffer, random_check) == 0 || msg_len == 0){
            break;
        }

        string x = convertToString(receive_buffer, msg_len);
        words.push_back(x);

        send(my_socket, "waiting", sizeof("waiting"), 0);
        // Obs.: sizeof() retorna o número de caracteres do char array + 1 pois conta com o '\0', enquanto...
        // ... strlen() retorna exatamente o número de caracteres do char array (não conta o '\0'). 
        memset(receive_buffer, 0, LEN);

    }
    return 0;
}

FILE *existsFile(char path[],char file_name[]){
    // (1) Cria um char array contendo o path concatenado com o nome do arquivo:
    char file_path[strlen(file_name) + strlen(path) + 1];
    // Obs.: o tamanho do char array deve-ser o número de caracteres + 1 (provavelmente pois termina...
    // ... com char "\0").
    strcpy(file_path, path);
    strcat(file_path, file_name);

    FILE *fptr;
    fptr = fopen(file_path, "rb");
    return fptr;
}

FILE *openFile(char path[], char file_name[]){
    // (1) Cria um char array contendo o path concatenado com o nome do arquivo:
    char file_path[strlen(file_name) + strlen(path) + 1];
    // Obs.: o tamanho do char array deve-ser o número de caracteres + 1 (provavelmente pois termina...
    // ... com char "\0").
    strcpy(file_path, path);
    strcat(file_path, file_name);

    FILE *fptr;
    fptr = fopen(file_path, "wb");
    return fptr;
}

int sendUpdate(int my_socket, FILE *fptr){

    // (0) Cria buffer de recebimento de mensagem:
    char receive_buffer[LEN];
    memset(receive_buffer, 0, LEN);
    int msg_len;

    // (2) Recebe e envia número aleatório:
    char random_check[11];
    int random_check_len = recv(my_socket, random_check, 11, 0);
    if(random_check_len == 0){
        return 1;
    }
    send(my_socket, random_check, 11, 0);

    // (3) Cria buffer para o arquivo:
    char send_buffer[PACKET];
    int read_size;

    // (4) Espera mensagem "waiting" para continuar a enviar:
    msg_len = recv(my_socket, receive_buffer, LEN, 0);
    if(strcmp(receive_buffer, "waiting") != 0 || msg_len == 0){
            return 1;
    }

    while(true){

        // (5) Limpa os buffers:
        memset(receive_buffer, 0, LEN);
        memset(send_buffer, 0, LEN);

        read_size = fread(send_buffer, sizeof(char), PACKET, fptr);
        send(my_socket, send_buffer, read_size, 0);
        
        // (6) Se o número de bytes lido é menor que o tamanho do buffer significa que se chegou ao final do arquivo:
        if(read_size < PACKET){
            break;
        }

        // (7) Espera mensagem "waiting" para continuar a enviar:
        msg_len = recv(my_socket, receive_buffer, LEN, 0);
        if(strcmp(receive_buffer, "waiting") != 0 || msg_len == 0){
            return 1;
        }
    }

    // (8) Limpa os buffers:
    memset(receive_buffer, 0, LEN);
    memset(send_buffer, 0, LEN);

    // (9) Espera mensagem "waiting" para terminar de enviar:
    msg_len = recv(my_socket, receive_buffer, LEN, 0);
    if(strcmp(receive_buffer, "waiting") != 0 || msg_len == 0){
        return 1;
    }

    // (10) Envia o número aleatório gerado inicialmente indicando fim do envio:
    send(my_socket, random_check, random_check_len + 1, 0);
    return 0;
}

int getFileSize(FILE *fptr){
    fseek(fptr, 0L, SEEK_END);
    int size =  ftell(fptr);
    rewind(fptr);
    return size;
}

int getDownload(int my_socket, FILE *fptr){

    // (0) Cria buffer de recebimento de mensagem:
    char receive_buffer[LEN];
    memset(receive_buffer, 0, LEN);
    int msg_len;

    // (1) Enviar "waiting" após receber "Yes":
    send(my_socket, "waiting", sizeof("waiting"), 0);

    // (2) Recebe e envia número aleatório:
    char random_check[11];
    int random_check_len = recv(my_socket, random_check, 11, 0);
    if(random_check_len == 0){
        cout << "[+] Connection problem, stop download" << endl;
        cout.clear();
        return 1;
    }
    // (3) Reenvia o número aleatório:
    send(my_socket, random_check, 11, 0);

    while(true){
        // (4) Limpa o buffer:
        memset(receive_buffer, LEN, 0);
        // (5) Recebe um pedaço do arquivo:
        msg_len = recv(my_socket, receive_buffer, LEN, 0);
        // (6) Verifica se o pedaço do arquivo é na verdade o número aleatório...
        // ... ou se é vazio:
        if(strcmp(receive_buffer, random_check) == 0) break;
        else if(msg_len == 0) return 1;
        // (7) Escreve o pedaço do arquivo:
        fwrite(receive_buffer, sizeof(char), msg_len, fptr);
        // (8) Envia "waiting" para o servidor:
        send(my_socket, "waiting", sizeof("waiting"), 0);
    }

    return 0;
}

int checkPath(char path[]){
    DIR *dir = opendir(path);
    if(dir == NULL){
        closedir(dir);
        return 1;
    } 
    closedir(dir);
    return 0;
}