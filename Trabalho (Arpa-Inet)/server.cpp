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
#include <bits/stdc++.h>


// g++ server.cpp -pthread -o server && ./server

using namespace std;

void *runClient(void *arg);
void *initServer(void *arg);
void *listenCommands(void *arg);

int getThreadId(pthread_t *threads);
int shutdownThreads(pthread_t *threads);
int getElementIndex(vector<int> &A, int n);
int sendListOfString(vector<string> &list, int client_socket);

string convertToString(char *array, int size);
vector<string> listDir(char dir[]);

pthread_mutex_t mtx_thread_info; 

#define MAX_THREADS_NUM 50
// threads_num indica o número de threads ativas:
int threads_num = 0;
// threads_flag serve para comunicação entre as threads (ainda não utilizada):
int threads_flag = 0;

vector<int> in_use_thread_ids;
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

    rc = pthread_create(&threads[0], NULL, listenCommands, (void *) threads);
    threads_num++;

    rc = pthread_create(&threads[1], NULL, initServer, (void *) threads);
    in_use_thread_ids.push_back(1);
    threads_num++;

    // (2) Espera a thread que ouve os comandos se encerrar pois se esta encerrou então todas as outras...
    // ... foram encerradas. 
    pthread_join(threads[0], NULL);

    return 0;
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

    // (4) A thread inicial dos clientes é a 2:
    int actual_thread_id;
    int rc;

    while(true){

        if(threads_num >= MAX_THREADS_NUM){
            cout << "[-] Número máximo de threads atingido, não é possível abrir mais conexões!" << endl;
            continue;
        }

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

            actual_thread_id = getThreadId(threads);
            info->id = actual_thread_id;
            rc = pthread_create(&threads[actual_thread_id], NULL, runClient, (void *) info);
            actual_thread_id++;
            threads_num++;

            pthread_mutex_unlock(&mtx_thread_info);
        }

    }

    close(server_socket);
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
        // (5.2) Limpando o buffer para recebimento de mensagem:
        memset(buffer, 0, BUFFER_SIZE);
        // (5.3) Esperando uma mensagem:
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if(bytes_received == -1){
            perror("[-] Erro na conexão :(");
            close(server_socket);
            // pthread_exit(NULL);
            break;
        }
        else if(bytes_received == 0){
            cout << "\n[+] Cliente desconectado (thread = " << id << ")" << endl;
            cout.flush();
            break;
        }
        else if(strcmp(buffer, "bye") == 0){
            send(client_socket, "bye bro!", sizeof("bye bro"), 0);
            break;
        }

        // string command;
        // command = convertToString(buffer, strlen(buffer));
        // vector<string> tokens = getTokens(command, ' ');

        if(strcmp(buffer, "list") == 0){
            // (5.4.1) Envia "list" ao cliente para avisar sobre o recebimento do comando:
            send(client_socket, "list", sizeof("list") + 1, 0);

            // (5.4.2) Cria um vetor (lista) com os nomes dos arquivos no diretório:
            char path[] = "/home/bruno/REC/Trabalho (Arpa-Inet)/ServerFiles/";
            vector<string> file_list = listDir(path);
            
            // (5.4.3) Envia a lista:
            int r = sendListOfString(file_list, client_socket);
            if(r == 1) break;
        }
        else{
            // (5.3) Reenviar mensagem para o cliente:
            send(client_socket, buffer, bytes_received + 1, 0);
        }
    }

    cout << "\nBye client (thread " << id << ")" << endl;
    cout.flush();

    pthread_mutex_lock(&mtx_thread_info);

    int index = getElementIndex(in_use_thread_ids, id);
    in_use_thread_ids.erase(in_use_thread_ids.begin() + index);
    free_thread_ids.push_back(id);

    pthread_mutex_unlock(&mtx_thread_info);

    cout << "[+] Type some command: ";
    cout.flush();

    close(client_socket);
    pthread_exit(NULL);
}

void *listenCommands(void *arg){

    pthread_t *threads;
    threads = (pthread_t *) arg;

    // (5.1) Buffer para receber comandos dentro do terminal do servidor:
    int BUFFER_SIZE = 4096;
    char command_buffer[BUFFER_SIZE];
    string command;
    while(true){
        // (5.2) Limpando o buffer para recebimento de comando:
        memset(command_buffer, 0, BUFFER_SIZE);
        // (5.3) Esperando um comando:
        cout << "[+] Type some command: ";
        cin >> command_buffer;

        command = convertToString(command_buffer, strlen(command_buffer));

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
    for(int x : in_use_thread_ids){
        rc = pthread_cancel(threads[x]);
        cout << "Closing thread " << x << endl;
    }
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
    
    printf("tipo \t| tipo de arquivo\n");
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
    int length;
    char *recv_buffer = new char[100];
    int bytes_received = recv(client_socket, recv_buffer, 100, 0);

    for(string x : list){
        length = x.length();
        char *send_buffer = new char[length + 1];
        strcpy(send_buffer, x.c_str());

        bytes_received = recv(client_socket, recv_buffer, 100, 0);
        if(bytes_received == 0){
            return 1;
        }
        else if(strcmp(recv_buffer, "waiting") == 0){
            send(client_socket, send_buffer, length + 1, 0);
        }
    }
    return 0;
}

int getThreadId(pthread_t *threads){
    // (1) Antes de se executar essa função deve ser se bloqueado o mutex sobre...
    // ... os vetores de ids:

    if(in_use_thread_ids.size() == 0){
        pthread_mutex_unlock(&mtx_thread_info);
        in_use_thread_ids.push_back(2);
        return 2;
    }
    else if(free_thread_ids.size() == 0){
        int last = in_use_thread_ids.back();
        in_use_thread_ids.push_back(last+1);
        pthread_mutex_unlock(&mtx_thread_info);
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

vector<string> getTokens(string phrase, char delimiter){
    stringstream command(phrase);
    string token;
    vector<string> tokens;
    while(getline(command, token, delimiter)){
        tokens.push_back(token);
    }
    return tokens;
}