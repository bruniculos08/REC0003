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

// g++ better_server.cpp -pthread -o better_server && ./better_server

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

string convertToString(char *array, int size);
vector<string> listDir(char dir[]);

#define LEN 4096
#define PACKET 1024
#define MAX_THREADS_NUM 500

regex point_pattern{"[. ]+"};
regex list_pattern{"list[ ]*"};
regex exit_pattern{"exit[ ]*"};
regex upload_pattern{"upload [^/]+"};
regex delete_pattern{"delete [^/]+"};
regex shutdown_pattern{"shutdown[ ]*"};
regex download_pattern{"download [^/]+[.][a-zA-Z0-9]+ [^]+"};

multiset<int> threads_used;
multiset<int> sockets_used;
multiset<int> free_threads;

pthread_mutex_t mtx_thread_info;

char server_dir[LEN];
char ipv4[16];

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

    string temp;

    cout << "[+] Choose the server dir (ex.: my_folder/): " << flush;
    getline(cin, temp);
    strcpy(server_dir, temp.c_str());
    cin.clear();

    cout << "[+] Choose the ipv4 (ex.: 127.0.0.1): " << flush;
    getline(cin, temp);
    strcpy(ipv4, temp.c_str());
    cin.clear();

    pthread_mutex_lock(&mtx_thread_info);
    
    rc = pthread_create(&threads[0], NULL, listenCommands, (void *) threads);

    rc = pthread_create(&threads[1], NULL, initServer, (void *) threads);
    // Obs.: o id 0 será sempre da thread listenCommands e o id 1 da thread initServer.
    
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
        cout << "[-] Could not create socket" << endl;
        close(server_socket);
        pthread_cancel(threads[0]);
        pthread_exit(0);
    }

    pthread_mutex_lock(&mtx_thread_info);

    sockets_used.insert(server_socket);

    pthread_mutex_unlock(&mtx_thread_info);

    // (2) Vincular (Bind) o socket a um IP e uma porta:
    sockaddr_in server;
    // (2.1) A familia AF_INET indica uso de IPV4: 
    server.sin_family = AF_INET;
    // (2.2) htons() converte um inteiro para formato u_int16 (portas são representadas por valores deste tipo):
    server.sin_port = htons(60000);
    // (2.3) inet_pton() converte um endereço de formato em texto ("localhost" ou "127.0.0.1") para seu formato em...
    // ... binário e armazena no buffer passado como parâmetro (server.sin_addr):
    // inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
    inet_pton(AF_INET, ipv4, &server.sin_addr);

    // (2.4) Associa o número de socket a estrutura de dados sockaddr respectiva:
    if(bind(server_socket, (sockaddr*) &server, sizeof(server)) == -1){
        cout << "\n[-] Could not bind IP/port" << endl;
        pthread_cancel(threads[0]);
        pthread_exit(NULL);
    }
    // (3) Faz com que o socket possa aceitar um determinado número de conexões:
    if(listen(server_socket, SOMAXCONN) == -1){
        cout << "\n[-] Could not start listening" << endl;
        pthread_cancel(threads[0]);
        pthread_exit(NULL);
    }

    sockaddr_in client;
    socklen_t client_size = sizeof(client);
    int rc;

    while(true){

        // (4) Verifica se o número máximo de threads foi alcançado:
        pthread_mutex_lock(&mtx_thread_info);
        if(threads_used.size() > MAX_THREADS_NUM){
            cout << "[-] Número máximo de threads atingido, não é possível abrir mais conexões (" << threads_used.size() << ") !" << endl;
            pthread_mutex_unlock(&mtx_thread_info);
            continue;
        }
        pthread_mutex_unlock(&mtx_thread_info);


        // (5.1) Criando um endereço de socket para o cliente:
        struct thread_arg *info = (struct thread_arg *)malloc(sizeof(thread_arg));
        // (5.2) Esperando uma conexação:
        int client_socket = accept(server_socket, (sockaddr *)&info->client, &info->client_size);

        if(client_socket == -1){
            cout << "[-] Problem on accepting a connection" << endl;
            close(client_socket);
            free(info);
            continue;
        }
        else{
            // (5.3) Aqui falta o mutex para colocar no vetor o id da thread e cuidar com o index menos 1 na função run:
            pthread_mutex_lock(&mtx_thread_info);

            info->client_socket = client_socket;
            info->id = getThreadId(threads, client_socket);
            rc = pthread_create(&threads[info->id], NULL, runClient, (void *) info);

            pthread_mutex_unlock(&mtx_thread_info);
        }
    }
    pthread_mutex_lock(&mtx_thread_info);

    // int index = getElementIndex(sockets_used, server_socket);
    // sockets_used.erase(sockets_used.begin() + index);
    // close(server_socket);

    sockets_used.erase(sockets_used.find(server_socket));
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
        // (1) Limpando o buffer para recebimento de mensagem:
        memset(buffer, 0, BUFFER_SIZE);
        // (2) Esperando uma mensagem:
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        // (3) Erro de conexão:
        if(bytes_received == -1){
            cout << "\n[-] Connection error" << endl;
            cout << "[+] Type some command: " << flush;
            close(client_socket);
            // pthread_exit(NULL);
            break;
        }
        // (4) Cliente desconectado:
        else if(bytes_received == 0){
            cout << "\n[+] Client disconected (thread = " << id << ")" << endl;
            cout << "[+] Type some command: " << flush;
            break;
        }
        // (5) Comando para desconectar:
        else if(regex_match(buffer, exit_pattern)){
            send(client_socket, "bye bro!", sizeof("bye bro!"), 0);
            break;
        }
        // (6) Comando list:
        else if(regex_match(buffer, list_pattern)){

            // (7.1) Cria um vetor (lista) com os nomes dos arquivos no diretório:
            vector<string> file_list = listDir(server_dir);

            // (7.2) Envia a lista:
            int r = sendListOfString(file_list, client_socket);
            if(r == 1){
                cout << "\n[+] Erro ao tentar enviar list (thread = " << id << ")" << endl;
                break;
            }
        }
        // (7) Comando de upload:
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

            // (8.4) Se o upload não funciou ocorreu um erro de conexão e a conexão deve...
            // ... ser encerrada:
            if(flag == 1) break;
        }
        // (8) Comando de delete:
        else if(regex_match(buffer, delete_pattern)){
            // (9.1) Obtém o nome do arquivo:
            char command[sizeof("delete")];
            char file_name[256];
            sscanf(buffer, "%s %[^\n]", command, file_name);   

            // (9.2) Se o nome do arquivo a ser deletado é inválido não executa o comando:
            if(regex_match(file_name, point_pattern)){
                send(client_socket, "Invalid file name", sizeof("Invalid file name"), 0);
                continue;
            }
            
            // (9.3)
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
            if(regex_match(file_name, point_pattern)){
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
                // (9.5) Se o download falhou a conexão deve ser encerrada:
                if(flag == 1){
                    break;
                }
            }
        }
        else{
            // (10) Reenviar mensagem para o cliente:
            send(client_socket, buffer, bytes_received + 1, 0);
            // Note que se no buffer a palavra contém n caracteres (ex.: "ab" contém 2 caracteres) deve-se enviar como...
            // ... n + 1 de tamanho.
        }
    }

    cout << "\nBye client (thread " << id << ")" << flush;

    // (11) Antes de encerrar a thread para se alterar quaisquer estrutura que guarde informações sobre as...
    // ... threads deve-se bloquer o mutex mtx_thread_info:
    pthread_mutex_lock(&mtx_thread_info);

    // (11.1) Remove o id da thread do vetor de id's em uso:
    threads_used.erase(threads_used.find(id));
    free_threads.insert(id);

    // (11.2) Remove o client socket da thread do vetor de client sockets em uso (aqui que ocorreu o problema...
    // ... "double free or corruption (out) \nAbort" pois estava fazendo erase no vetor de id's):
    close(client_socket);
    sockets_used.erase(sockets_used.find(client_socket));    

    pthread_mutex_unlock(&mtx_thread_info);

    cout << "\n[+] Type some command: " << flush;

    pthread_exit(NULL);
}

void *listenCommands(void *arg){

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    pthread_t *threads;
    threads = (pthread_t *) arg;

    string command;
    
    while(true){
        // (1) Esperando um comando:
        cout << "[+] Type some command: " << flush;

        // (2) Recebe a entrada com string class com getline() para obter toda a entrada...
        // ... (não terminando no primeiro espaço em branco):
        getline(cin, command);

        // (3) Se for o comando "shutdown" desliga o servidor:
        if(regex_match(command, shutdown_pattern)){
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
    for(int x : sockets_used){
        close(x);
        cout << "Closing client socket " << x << endl;
    }
    for(int x : threads_used){
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
        if(!regex_match(file_name, point_pattern)){
            dir_list.push_back(file_name);
        }
        
        r = readdir(d);
    }
    while(r != NULL);
    closedir(d);

    return dir_list;
}

int sendListOfString(vector<string> &list, int client_socket){

    // (0) Coloca o número de arquivos em uma string:
    int names_num = list.size();
    int len = snprintf(NULL, 0, "%d", names_num);
    char num[len+1];
    snprintf(num, len+1, "%d", names_num);

    // (1) Cria os buffers:
    int word_length;
    int bytes_received;
    char *receive_buffer = new char[LEN];
    char *send_buffer = new char[LEN];
    memset(receive_buffer, 0, LEN);
    memset(send_buffer, 0, LEN);
        
    // (2) Envia o número de arquivos/nomes:
    send(client_socket, num, len + 1, 0);
    // Obs.: na conversão de string para char array o tamanho da string é sempre 1 a menos...
    // ... e para enviar no socket deve-se colocar o tamanho do char array + 1.
    bytes_received = recv(client_socket, receive_buffer, LEN, 0);

    // (3) Recebe devolta o número de arquivos/nomes:
    if(bytes_received == 0 or strcmp(receive_buffer, num) != 0){
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
    // Obs.: na conversão de string para char array o tamanho da string é sempre 1 a menos...
    // ... e para enviar no socket deve-se colocar o tamanho do char array + 1.
    return 0;
}

int getThreadId(pthread_t *threads, int client_socket){
    // (0) Antes de se executar essa função deve ser se bloqueado o mutex sobre...
    // ... os vetores de ids e client sockets.

    // (1) O client socket deve-ser colocado no vetor de client sockets em uso:
     sockets_used.insert(client_socket);

    if(threads_used.size() <= 0 and free_threads.size() <= 0){
        // A falta da segunda condição (a direita do 'and') estava causando deadlock pois há casos em que...
        // ... o id 2 estava no vetor free_threads porém o vetor threads_used estava vazio, assim se criava...
        // ... uma thread com id 2 se mantendo o id 2 no vetor free_threads o que possibilitava que outra...
        // ... thread com id 2 fosse cria mesmo existindo o id 2 no vetor threads_used ou que a fosse colocado...
        // ... duas vezes o id 2 no vetor free_threads.
        threads_used.insert(2);
        return 2;
    }
    else if(free_threads.size() <= 0){
        // A falta da função sort() estava causa deadlock antes pois o vetor threads_used não estava ordenado,...
        // ... assim, ao se somar 1 ao valor na última posição do vetor se obtinha o valor de id de uma thread...
        // ... em uso (igual de outra posição do vetor threads_used).
        int last = *threads_used.rbegin();
        threads_used.insert(last + 1);
        return last+1;
    }

    int last_free = *free_threads.rbegin();
    pthread_join(threads[last_free], NULL);
    free_threads.erase(*free_threads.rbegin());
    threads_used.insert(last_free);

    return last_free;
}

// Retorna o tamanho do arquivo em bytes:
int getFileSize(FILE *fptr){
    fseek(fptr, 0L, SEEK_END);
    int size = ftell(fptr);
    rewind(fptr);
    return size;
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

    // (0) Cria buffers de envio e recebimento:
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
    if(strcmp(random_check, receive_buffer) != 0 or bytes_received == 0){
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

int getUpload(int client_socket, FILE *fptr){

    // (0) Cria buffers de envio e recebimento:
    char receive_buffer[LEN];
    memset(receive_buffer, 0, LEN);
    int bytes_received;

    // (1) Envia "waiting" para o cliente:
    send(client_socket, "waiting", sizeof("waiting"), 0);

    // (2) Recebe e reenvia o tamanho do arquivo:
    bytes_received = recv(client_socket, receive_buffer, LEN, 0);
    if(bytes_received == 0){
        return 1;
    }

    send(client_socket, receive_buffer, bytes_received + 1, 0);

    int file_size = atoi(receive_buffer);
    int position = 0;

    while(true){
        // (4.1) Limpa o buffer:
        memset(receive_buffer, LEN, 0);
        // (4.2) Recebe um pedaço do arquivo:
        bytes_received = recv(client_socket, receive_buffer, LEN, 0);
        // (4.3) Verifica se o pedaço do arquivo é vazio:
        if(bytes_received == 0){
            return 1;
        }
        // (4.4) Escreve o pedaço do arquivo:
        fwrite(receive_buffer, sizeof(char), bytes_received, fptr);
        position += bytes_received;
        // (4.5) Envia "waiting" para o cliente:
        if(file_size > position) send(client_socket, "waiting", sizeof("waiting"), 0);
        else break;
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

    // (2) Obtém tamanho do arquivo:
    int size = getFileSize(fptr);
    int len = snprintf(NULL, 0, "%d", size);
    char num[len+1];
    snprintf(num, len+1, "%d", size);

    // (3) Envia e recebe o tamanho do arquivo:
    send(client_socket, num, len+1, 0);
    bytes_received = recv(client_socket, receive_buffer, LEN, 0);
    if(bytes_received == 0 or strcmp(num, receive_buffer) != 0){
        return 1;
    }

     // (4) Cria buffer para o arquivo:
    char send_buffer[PACKET];
    int read_size;

    while(true){
        // (4.1) Limpa os buffers:
        memset(receive_buffer, 0, LEN);
        memset(send_buffer, 0, LEN);

        read_size = fread(send_buffer, sizeof(char), PACKET, fptr);
        send(client_socket, send_buffer, read_size, 0);
        
        // (4.2) Se o número de bytes lido é menor que o tamanho do buffer significa que se chegou ao final do arquivo:
        if(read_size < PACKET){
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

    return 0;
}