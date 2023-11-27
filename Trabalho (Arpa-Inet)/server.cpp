#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <regex>
#include <bits/stdc++.h>

// g++ server.cpp -pthread -lstdc++fs -o server && ./server

using namespace std;

void *runClient(void *arg);
void *initServer(void *arg);
void *listenCommands(void *arg);

int getThreadId(pthread_t *threads, int client_socket);
int shutdownThreads(pthread_t *threads);
int getElementIndex(vector<int> &A, int n);
int sendListOfString(vector<string> &list, int client_socket);
int getUpload(int client_socket, FILE *fptr);
int sendDownload(int client_socket, FILE *fptr);

FILE *existsFile(char path[], char file_name[]);
FILE *openFile(char path[], char file_name[]);

regex upload_pattern{"upload [^/]+"};
regex delete_pattern{"delete [^/]+"};
regex download_pattern{"download [^/]+[.][a-zA-Z0-9]+ [^]+"};
regex list_pattern{"list[ ]*"};
regex exit_pattern{"exit[ ]*"};
regex invalid_file{"[. ]+"};
char server_dir[] = "/home/bruno/REC/Trabalho (Arpa-Inet)/ServerFiles/";

string convertToString(char *array, int size);
vector<string> listDir(char dir[]);

pthread_mutex_t mtx_thread_info; 

#define LEN 4096
#define MAX_THREADS_NUM 20
// threads_flag serve para comunicação entre as threads (ainda não utilizada):
int threads_flag = 0;
int connection_flag = 0;

vector<int> in_use_thread_ids;
vector<int> in_use_client_sockets;
vector<int> free_thread_ids;

struct thread_arg{
    int id;
    int server_socket; 
    int client_socket; 
    sockaddr_in client; 
    socklen_t client_size;
};

int main(){

    // (1) Criando as threads (a primeira será a do listenCommands e a segunda a do servidor):
    pthread_t threads[MAX_THREADS_NUM];
    int rc;

    // Obs.: o id 0 será sempre da thread listenCommands (e não precisa ser listado) e o id 1 da thread initServer.

    pthread_mutex_lock(&mtx_thread_info);
    
    rc = pthread_create(&threads[0], NULL, listenCommands, (void *) threads);

    // in_use_thread_ids.push_back(1);
    rc = pthread_create(&threads[1], NULL, initServer, (void *) threads);
    
    pthread_mutex_unlock(&mtx_thread_info);

    // (2) Espera a thread que ouve os comandos se encerrar pois se esta encerrou então todas as outras...
    // ... foram encerradas. 
    pthread_join(threads[0], NULL);

    exit(0);
}

void *initServer(void *arg){

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    pthread_t *threads;
    threads = (pthread_t *) arg;

    // (1) Criar um socket:
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1){
        perror("[-] Could not create socket :( \n");
    }

    pthread_mutex_lock(&mtx_thread_info);

    in_use_client_sockets.push_back(server_socket);

    pthread_mutex_unlock(&mtx_thread_info);


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
        perror("\n[-] Could not bind IP/port :( \n");
        pthread_cancel(threads[0]);
        pthread_exit(NULL);
    }

    // (3) Faz com que o socket possa aceitar um determinado número de conexões:
    if(listen(server_socket, SOMAXCONN) == -1){
        perror("[-] Could not start listening :( \n");
    }

    sockaddr_in client;
    socklen_t client_size = sizeof(client);
    int rc;

    while(true){

        // O vetor in_use_thread_ids não está sendo limpado de maneira correta:
        pthread_mutex_lock(&mtx_thread_info);
        if(in_use_thread_ids.size() > MAX_THREADS_NUM){
            cout << "[-] Número máximo de threads atingido, não é possível abrir mais conexões (" << in_use_thread_ids.size() << ") !" << endl;
            pthread_mutex_unlock(&mtx_thread_info);
            continue;
        }
        pthread_mutex_unlock(&mtx_thread_info);

        // (5) Aceitar uma conexação:
        // (5.1) Criando um endereço de socket para o cliente:
        struct thread_arg *info = (struct thread_arg *)malloc(sizeof(thread_arg));
        int client_socket = accept(server_socket, (sockaddr *)&info->client, &info->client_size);

        info->client_socket = client_socket;

        if(client_socket == -1){
            perror("[-] Problem on accepting a connection");
        }
        else{
            // (5.2) Aqui falta o mutex para colocar no vetor o id da thread e cuidar com o index menos 1 na função run:
            pthread_mutex_lock(&mtx_thread_info);

            info->id = getThreadId(threads, client_socket);
            rc = pthread_create(&threads[info->id], NULL, runClient, (void *) info);

            pthread_mutex_unlock(&mtx_thread_info);
        }

    }

    pthread_mutex_lock(&mtx_thread_info);

    int index = getElementIndex(in_use_client_sockets, server_socket);
    in_use_client_sockets.erase(in_use_client_sockets.begin() + index);
    close(server_socket);

    pthread_mutex_unlock(&mtx_thread_info);

    pthread_exit(NULL);
}

void *runClient(void *arg){

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    struct thread_arg *info;
    info = (struct thread_arg *) arg; 

    int id = info->id;
    int server_socket = info->server_socket;
    int client_socket = info->client_socket;
    sockaddr_in client = info->client;
    socklen_t client_size = info->client_size;
    free(info);

    // No buffer cabem 4096 bytes sendo assim teremos que transferir um arquivo em pedaços de...
    // ... no máximo 4095 byes: 
    int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    while(true){
        // (2) Limpando o buffer para recebimento de mensagem:
        memset(buffer, 0, BUFFER_SIZE);
        
        // (3) Esperando uma mensagem:
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);

        // (4) Erro de conexão:
        if(bytes_received == -1){
            // perror("[-] Erro na conexão :(\n");
            cout << "[-] Erro na conexão :(" << endl;
            cout.flush();
            close(client_socket);
            // pthread_exit(NULL);
            break;
        }
        // (5) Cliente desconectado:
        else if(bytes_received == 0){
            cout << "\n[+] Cliente desconectado (thread = " << id << ")" << endl;
            cout.flush();
            break;
        }
        // (6) Comando para desconectar:
        else if(regex_match(buffer, exit_pattern)){
            send(client_socket, "bye bro!", sizeof("bye bro"), 0);
            break;
        }
        // (7) Comando list:
        else if(regex_match(buffer, list_pattern)){

            // (7.1) Cria um vetor (lista) com os nomes dos arquivos no diretório:
            char path[] = "/home/bruno/REC/Trabalho (Arpa-Inet)/ServerFiles/";
            vector<string> file_list = listDir(path);

            // (7.2) Envia a lista:
            int r = sendListOfString(file_list, client_socket);
            if(r == 1){
                cout << "\n[+] Erro ao tentar enviar list (thread = " << id << ")" << endl; 
                cout.flush();
                break;
            }
        }
        // (8) Comando de upload:
        else if(regex_match(buffer, upload_pattern)){
            // (8.1) Obtém o nome do arquivo:
            char command[sizeof("update")];
            char file_name[256];
            sscanf(buffer, "%s %[^\n]", command, file_name);
            // (8.2) Cria ou sobrescreve o arquivo:
            FILE *fptr;
            fptr = openFile(server_dir, file_name);
            // (8.3) Chama a função para obter o upload:
            int flag = getUpload(client_socket, fptr);
            fclose(fptr);
        }
        else if(regex_match(buffer, delete_pattern)){
            // (9.1) Obtém o nome do arquivo:
            char command[sizeof("delete")];
            char file_name[256];
            sscanf(buffer, "%s %[^\n]", command, file_name);   

            if(regex_match(file_name, invalid_file)){
                send(client_socket, "Invalid file name", sizeof("Invalid file name"), 0);
                continue;
            }
            
            // (9.2)
            char file_path[strlen(file_name) + strlen(server_dir) + 1];
            strcpy(file_path, server_dir);
            strcat(file_path, file_name);

            int flag = remove(file_path);

            if(flag == 0){
                send(client_socket, "File deleted successfully", sizeof("File deleted successfully"), 0);
            }
            else{
                send(client_socket, "Error trying to delete file", sizeof("Error trying to delete file"), 0);
            }
            continue;
        }
        // (9) Comando de download:
        else if(regex_match(buffer, download_pattern)){
            // (9.1) Obtém o nome do arquivo e sua extensão:
            char command[sizeof("download")];
            char file_init[256];
            char file_ext[256];
            char path[256];
            sscanf(buffer, "%s %[^.]%[^ ] %[^\n]", command, file_init, file_ext, path);

            // (9.2) Remonta o nome do arquivo a ser baixado (com sua extensão):
            char file_name[strlen(file_init) + strlen(file_ext) + 1];
            // Obs.: o tamanho do char array deve-ser o número de caracteres + 1 (provavelmente pois termina...
            // ... com char "\0").
            strcpy(file_name, file_init);
            strcat(file_name, file_ext);

            // (9.3) Os seguintes arquivos existem porém não são válidos, assim o servidor deve enviar "No":
            if(regex_match(file_name, invalid_file)){
                send(client_socket, "No", sizeof("No"), 0);
                continue;
            }

            // (9.4) Verifica se o arquivo existe:
            FILE *fptr = existsFile(server_dir, file_name);
            if(fptr == NULL){
                send(client_socket, "No", sizeof("No"), 0);
                fclose(fptr);
            }
            else{
                send(client_socket, "Yes", sizeof("Yes"), 0);
                int flag = sendDownload(client_socket, fptr);
                fclose(fptr);
                if(flag == 1){
                    break;
                }
            }
        }
        else{
            // (5.3) Reenviar mensagem para o cliente:
            send(client_socket, buffer, bytes_received + 1, 0);
            // Note que se no buffer a palavra contém n caracteres (ex.: "ab" contém 2 caracteres) deve-se enviar como...
            // ... n + 1 de tamanho.
        }
    }

    cout << "\nBye client (thread " << id << ")" << endl;
    cout.flush();

    // (6) Antes de encerrar a thread para se alterar quaisquer estrutura que guarde informações sobre as...
    // ... threads deve-se bloquer o mutex mtx_thread_info:
    pthread_mutex_lock(&mtx_thread_info);
    int index;

    // (6.1) Remove o id da thread do vetor de id's em uso:
    index = getElementIndex(in_use_thread_ids, id);
    in_use_thread_ids.erase(in_use_thread_ids.begin() + index);
    free_thread_ids.push_back(id);

    // (6.2) Remove o client socket da thread do vetor de client sockets em uso (aqui que ocorreu o problema...
    // ... "double free or corruption (out) \nAbort" pois estava fazendo erase no vetor de id's):
    close(client_socket);
    index = getElementIndex(in_use_client_sockets, client_socket);
    in_use_client_sockets.erase(in_use_client_sockets.begin() + index);

    pthread_mutex_unlock(&mtx_thread_info);

    cout << "[+] Type some command: ";
    cout.flush();

    pthread_exit(NULL);
}

void *listenCommands(void *arg){

    pthread_t *threads;
    threads = (pthread_t *) arg;

    // (5.1) Buffer para receber comandos dentro do terminal do servidor (não utilizado):
    int BUFFER_SIZE = 4096;
    char command_buffer[BUFFER_SIZE];
    string command;
    
    while(true){
        // (5.2) Limpando o buffer para recebimento de comando:
        memset(command_buffer, 0, BUFFER_SIZE);
        // (5.3) Esperando um comando:
        cout << "[+] Type some command: ";
        cout.flush();

        // (5.4) Recebe a entrada com string class com getline() para obter toda a entrada...
        // ... (não terminando no primeiro espaço em branco):
        getline(cin, command);
        cin.clear();

        if(command == "shutdown"){
            pthread_mutex_lock(&mtx_thread_info);
            shutdownThreads(threads);
            pthread_mutex_unlock(&mtx_thread_info);
            break;
        }
    }

    cout << "bye master!" << endl;
    pthread_exit(NULL);
}

int shutdownThreads(pthread_t *threads){
    int rc;
    for(int x : in_use_client_sockets){
        close(x);
        cout << "Closing client socket " << x << endl;
    }
    for(int x : in_use_thread_ids){
        rc = pthread_cancel(threads[x]);
        cout << "Closing thread " << x << endl;
    }
    rc = pthread_cancel(threads[1]);
    return rc;
}

string convertToString(char *array, int size){
    string s = "";
    for(int i = 0; i < size; i++){
        s = s + array[i];
    }
    return s;
}

vector<string> listDir(char dir[]){
    DIR *d;

    if(strcmp(dir, "") == 0) d = opendir(".");
    else d = opendir(dir);
    
    // printf("tipo \t| tipo de arquivo\n");
    struct dirent *r;
    r = readdir(d);
    string file_name;
    vector<string> dir_list;
    
    do{
        file_name = convertToString(r->d_name, strlen(r->d_name));
        dir_list.push_back(file_name);
        r = readdir(d);
    }
    while(r != NULL);
    closedir(d);

    return dir_list;
}

int sendListOfString(vector<string> &list, int client_socket){

    // (0) Cria o char array com a string de número aleatório:
    string random_check_string = to_string(rand());
    int random_check_lenght = random_check_string.length();
    char random_check[random_check_lenght + 1];
    strcpy(random_check, random_check_string.c_str());

    // (1) Cria os buffers:
    int word_length;
    int bytes_received;
    char *receive_buffer = new char[LEN];
    char *send_buffer = new char[LEN];
        
    // (2) Envia e recebe o número aleatório (este número ira servir para identificar o final...
    // ... de envio do arquivo):
    send(client_socket, random_check, random_check_lenght + 2, 0);
    // Obs.: na conversão de string para char array o tamanho da string é sempre 1 a menos...
    // ... e para enviar no socket deve-se colocar o tamanho do char array + 1.
    bytes_received = recv(client_socket, receive_buffer, LEN, 0);

    if(strcmp(receive_buffer, random_check) != 0){
        return 1;
    }

    for(string x : list){
        memset(receive_buffer, 0, LEN);
        memset(send_buffer, 0, LEN);

        // (3.1) Copia a string para o buffer de envio:
        word_length = x.length();
        strcpy(send_buffer, x.c_str());

        // (3.2) Envia a string:
        send(client_socket, send_buffer, word_length + 2, 0);
        // Obs.: na conversão de string para char array o tamanho da string é sempre 1 a menos...
        // ... e para enviar no socket deve-se colocar o tamanho do char array + 1.

        // (3.3) Verifica se o cliente já recebeu e está esperando a próxima mensagem, isto é,...
        // ... espera recebe a palavra "waiting" do cliente:
        bytes_received = recv(client_socket, receive_buffer, LEN, 0);
        if(bytes_received == 0){
            return 1;
        }
        else if(strcmp(receive_buffer, "waiting") == 0){
            continue;
        }
    }
    send(client_socket, random_check, random_check_lenght + 2, 0);
    // Obs.: na conversão de string para char array o tamanho da string é sempre 1 a menos...
    // ... e para enviar no socket deve-se colocar o tamanho do char array + 1.
    return 0;
}

int getThreadId(pthread_t *threads, int client_socket){
    // (0) Antes de se executar essa função deve ser se bloqueado o mutex sobre...
    // ... os vetores de ids e client sockets.

    // (1) O client socket deve-ser colocado no vetor de client sockets em uso:
    in_use_client_sockets.push_back(client_socket);

    if(in_use_thread_ids.size() == 0){
        in_use_thread_ids.push_back(2);
        return 2;
    }
    else if(free_thread_ids.size() == 0){
        int last = in_use_thread_ids.back();
        in_use_thread_ids.push_back(last+1);
        return last+1;
    }
   
    int last_free = free_thread_ids.back();
    pthread_join(threads[last_free], NULL);
    
    free_thread_ids.pop_back();
    in_use_thread_ids.push_back(last_free);
    
    return last_free;
}

int getElementIndex(vector<int> &A, int n){
    for(int i = 0; i < A.size(); i++){
        if(n == A[i]) return i;
    }
    return -1;
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
    strcpy(file_path, server_dir);
    strcat(file_path, file_name);

    FILE *fptr;
    fptr = fopen(file_path, "wb");
    return fptr;
}

int getUpload(int client_socket, FILE *fptr){

    // (1) Cria buffers de envio e recebimento:
    char receive_buffer[LEN];
    memset(receive_buffer, 0, LEN);
    int bytes_received;

    // (1) Cria número aleatório para sinalização fim do envio:
    char random_check[11];
    // Obs.: a constante RAND_MAX tem 10 dígitos.
    int random_num = rand();
    sprintf(random_check, "%d", random_num);
    int random_check_len = strlen(random_check);

    // (2) Envia e recebe o número aleatório:
    send(client_socket, random_check, 11, 0);
    bytes_received = recv(client_socket, receive_buffer, LEN, 0);
    if(strcmp(random_check, receive_buffer) != 0){
        return 1;
    }

    // (3) Envia "waiting" para o cliente:
    send(client_socket, "waiting", sizeof("waiting"), 0);
    
    while(true){
        // (4.1) Limpa o buffer:
        memset(receive_buffer, LEN, 0);
        // (4.2) Recebe um pedaço do arquivo:
        bytes_received = recv(client_socket, receive_buffer, LEN, 0);
        // (4.3) Verifica se o pedaço do arquivo é na verdade o número aleatório...
        // ... ou se é vazio:
        if(strcmp(receive_buffer, random_check) == 0){
            break;
        }
        else if(bytes_received == 0){
            return 1;
        }
        // (4.4) Escreve o pedaço do arquivo:
        fwrite(receive_buffer, sizeof(char), bytes_received, fptr);
        // (4.5) Envia "waiting" para o cliente:
        send(client_socket, "waiting", sizeof("waiting"), 0);
    }

    return 0;
}

int sendDownload(int client_socket, FILE *fptr){

    // (0) Cria buffers de envio e recebimento:
    char receive_buffer[LEN];
    memset(receive_buffer, 0, LEN);
    int bytes_received;

    // (1) Espera "waiting" após ter enviado "Yes":
    bytes_received = recv(client_socket, receive_buffer, LEN, 0);
    if(bytes_received == 0 or strcmp(receive_buffer, "waiting") != 0){
        return 1;
    }
    memset(receive_buffer, 0, LEN);

    // (2) Cria número aleatório para sinalização fim do envio:
    char random_check[11];
    // Obs.: a constante RAND_MAX tem 10 dígitos.
    int random_num = rand();
    sprintf(random_check, "%d", random_num);
    int random_check_len = strlen(random_check);

    // (3) Envia e recebe o número aleatório:
    send(client_socket, random_check, 11, 0);
    bytes_received = recv(client_socket, receive_buffer, LEN, 0);
    if(strcmp(random_check, receive_buffer) != 0){
        return 1;
    }

     // (4) Cria buffer para o arquivo:
    char send_buffer[256];
    int read_size;

    while(true){
        // (4.1) Limpa os buffers:
        memset(receive_buffer, 0, LEN);
        memset(send_buffer, 0, LEN);

        read_size = fread(send_buffer, sizeof(char), 256, fptr);
        send(client_socket, send_buffer, read_size, 0);
        
        // (4.2) Se o número de bytes lido é menor que o tamanho do buffer significa que se chegou ao final do arquivo:
        if(read_size < 256){
            break;
        }

        // (4.3) Espera mensagem "waiting" para continuar a enviar:
        bytes_received = recv(client_socket, receive_buffer, LEN, 0);
        if(strcmp(receive_buffer, "waiting") != 0 || bytes_received == 0){
            return 1;
        }
    }

    // (5) Limpa os buffers:
    memset(receive_buffer, 0, LEN);
    memset(send_buffer, 0, LEN);

    // (6) Espera mensagem "waiting" para terminar de enviar:
    bytes_received = recv(client_socket, receive_buffer, LEN, 0);
    if(strcmp(receive_buffer, "waiting") != 0 || bytes_received == 0){
        return 1;
    }

    // (7) Envia o número aleatório para sinalizar fim do envio:
    send(client_socket, random_check, random_check_len + 1, 0);
    return 0;
}