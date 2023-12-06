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
#include <time.h>
#include <sys/time.h>
#include <gnuplot-iostream.h>
#include <chrono>

// g++ script.cpp -pthread -lboost_iostreams -o script && ./script

using namespace std;
using namespace std::chrono;

#define LEN 4096
#define PACKET 256

int receiveListOfStrings(vector<string> &listResponse, int my_socket);
int sendUpdate(int my_socket, FILE *fptr);
int getFileSize(FILE *fptr);
int getDownload(int my_socket, FILE *fptr);
int checkPath(char path[]);
void *perform(void *arg);

FILE *existsFile(char path[],char file_name[]);
FILE *openFile(char path[], char file_name[]);

regex point_pattern{"[. ]+"};
regex list_pattern{"list[ ]*"};
regex exit_pattern{"exit[ ]*"};
regex upload_pattern{"upload [^/]+"};
regex delete_pattern{"delete [^/]+"};
regex shutdown_pattern{"shutdown[ ]*"};
regex download_pattern{"download [^/]+[.][a-zA-Z0-9]+ [^]+"};

pthread_barrier_t barrier;
pthread_mutex_t time_info;

double time_sum;
int downloads_num;
int threads_num;
int threads_max;

char client_dir[] = "ClientFiles/";
char csv_file[] = "Results/performance.csv";

int main(){

    threads_max = 500;
    downloads_num = 500;
    int interval = 50;

    Gnuplot gp("gnuplot");
    gp << "set title 'Tempo para " << downloads_num << " downloads com número variável de threads'\n";

    vector<pair<double, double>> points;
    pthread_t threads[threads_max];

    for (int i = 1; i <= threads_max; i+=interval){
        threads_num = i;
        time_sum = 0;

        pthread_barrier_init(&barrier, NULL, (unsigned int) threads_num);
        for (int j = 0; j < threads_num ; j++){
            int *id = (int *)malloc(sizeof(int));
            *id = j;
            pthread_create(&threads[j], NULL, perform, (void *) id);
        }
        for (int j = 0; j < threads_num ; j++){
            pthread_join(threads[j], NULL);
        }
        pthread_barrier_destroy(&barrier);

        points.push_back(make_pair((double) i, time_sum/((double) i)) );
        if(threads_num == 1) threads_num = 0;
    }

    gp << "set terminal 'png'\n";
    // gp << "set output \"ServerFiles/graphic-better_server.png\"\n";
    // gp << "set output \"ServerFiles/graphic-server.png\"\n";
    gp << "set output \"Results/graphic-server.png\"\n";

    gp << "set xlabel \"number of threads\"\n";
    gp << "set ylabel \"time in millisecond\"\n";
    gp << "set grid\n";

    gp << "plot '-' with lines title 'points'\n";
    gp.send1d(points);

    // gp << "replot\n";
    // gp << "set term x11\n";

    // cout << "\n[-]Type any buttom to exit\n";
    // cin.get();
    
    exit(0);
}

void *perform(void *arg){

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    int *t;
    t = (int *) arg;
    int id = *t;

    cout << "(Thread = " << id << ") - " << "Ready to start!" << endl;
    pthread_barrier_wait(&barrier);

    label_restart_connection:

    // (0) Criar um socket:
    int my_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(my_socket == -1){
        cout << "[-] Could not create socket" << endl;
        pthread_exit(NULL);
    }

    // (1) Aqui criamos a struct com o endereço do servidor:
    sockaddr_in server_address;
    // (1.1) A familia AF_INET indica uso de IPV4: 
    server_address.sin_family = AF_INET;
    // (1.2) htons() converte um inteiro para formato u_int16 (portas são representadas por valores deste tipo):
    server_address.sin_port = htons(60000);

    // (1.3) Encontra o endereço ipv4 por meio de requisição DNS:
    // char url[] = "0.tcp.sa.ngrok.io:14832";
    // char *IPv4 = discoverIPv4(url, "14832");
    // cout << "IPv4 address found: " << IPv4 << endl;

    // (1.4) inet_pton() converte um endereço de formato em texto ("localhost" ou "127.0.0.1") para seu formato em...
    // ... binário e armazena no buffer passado como parâmetro (server.sin_addr):
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    // (2) Conectar com o servidor:
    if(connect(my_socket, (sockaddr *)&server_address, sizeof(server_address)) == -1){
        cout << "[-] Cant connect to the server" << endl; 
        pthread_exit(NULL);
    }

    // (3) Comunicação com o servidor:
    char receive_buffer[LEN];
    char send_buffer[LEN];
    int msg_len;

    int count = downloads_num;

    // (5) Criando estrutura para marcação do tempo e iniciando a marcação:
    struct timeval tv_ini, tv_fim;
    unsigned long time_diff, sec_diff, usec_diff, msec_diff;
    if(gettimeofday(&tv_ini, NULL) != 0){
        cout << "[-] Error at gettimeofday()" << endl;
        pthread_exit(NULL);
    }

    auto start = high_resolution_clock::now();

    while (true)
    {
        memset(receive_buffer, 0, LEN);
        memset(send_buffer, 0, LEN);

        // (6) Escreve algum comando específico:

        // (7) Verifica o número de downloads feitos:
        count--;
        if(count <= 0){
            strcpy(send_buffer, "exit");
        } 
        // else strcpy(send_buffer, "download unnamed.jpg ClientFiles/");
        else strcpy(send_buffer, "download morgan.txt ClientFiles/");

        // (8) Verificar se a entrada é um comando de upload:
        if(regex_match(send_buffer, upload_pattern)){
            // (8.1) Coloca os argumentos do comando upload em variáveis:
            char command[sizeof("update")];
            char file_name[256];
            sscanf(send_buffer, "%s %[^\n]", command, file_name);

            cout << "Started download" << endl;

            FILE *fptr = existsFile(client_dir, file_name);

            if(fptr != NULL){
                // (8.2) Envia o comando ao servidor:
                send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
                // (8.3) Chama a função para enviar arquivo:
                int flag = sendUpdate(my_socket, fptr);
                // (8.4) Volta ao incio e tenta reiniciar a conexão:
                if(flag == 1){
                    cout << "Erro de conexão, upload cancelado." << endl;
                    cout << "Tentando reconectar ao servidor..." << endl;
                    goto label_restart_connection;
                } 
            }
            else cout << "Tried to upload non existing file." << endl;
            continue;
        }
        // (9) Verificar se a entrada é um comando de delete:
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
        // (10) Verificar se a entrada é um comando de download:
        else if(regex_match(send_buffer, download_pattern)){
            // (10.1) Coloca os argumentos do comando upload em variáveis:
            char command[sizeof("download")];
            char file_init[256];
            char file_ext[256];
            char path[256];
            sscanf(send_buffer, "%s %[^.]%[^ ] %[^\n]", command, file_init, file_ext, path);

            // (10.2) Remonta o nome do arquivo a ser baixado:
            char file_name[strlen(file_init) + strlen(file_ext) + 1];
            // Obs.: o tamanho do char array deve-ser o número de caracteres + 1 (provavelmente pois termina...
            // ... com char "\0").
            strcpy(file_name, file_init);
            strcat(file_name, file_ext);

            // (10.3) Se o caminho é inválido não envia o comando ao servidor:
            if(checkPath(path) == 1){
                cout << "[-] Invalid path." << endl;
                continue;
            }
            
            // (10.4) Verifica se o arquivo existe no servidor:
            send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
            msg_len = recv(my_socket, receive_buffer, LEN, 0);

            // (10.5) Tenta fazer download do arquivo:
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
        // (11) Verifica se a entrada é um comando de list:
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
        // (12) Verifica se a entrada é um comando de exit:
        else if(regex_match(send_buffer, exit_pattern)){
            send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
            msg_len = recv(my_socket, receive_buffer, LEN, 0);
            cout << "[+] Server answer: " << receive_buffer << endl;
            break;
        }
        // (13) Envia algum texto e recebe o mesmo do servidor:
        send(my_socket, send_buffer, strlen(send_buffer) + 1, 0);
        msg_len = recv(my_socket, receive_buffer, LEN, 0);
        
        if(strcmp(receive_buffer, "") == 0){
            cout << "[+] Server has been shutdown... " << endl;
            break;
        }
        cout << "[+] Server answer: " << receive_buffer << " [invalid sintax or not a command]" << endl;
    }

    if(gettimeofday(&tv_fim, NULL) != 0){
        cout << "[-] Error at gettimeofday()" << endl;
        pthread_exit(NULL);
    }

    // (14) Calcula a diferenca entre os tempos, em usec:
    // time_diff = (1000000L*tv_fim.tv_sec + tv_fim.tv_usec) - (1000000L*tv_ini.tv_sec + tv_ini.tv_usec);

    // (15) Converte para segundos + microsegundos (parte fracionária):
    // sec_diff = time_diff / 1000000L;
    // usec_diff = time_diff % 1000000L;
     
    // (16) Converte para msec:
    // msec_diff = time_diff / 1000;

    auto stop = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(stop - start);

    pthread_mutex_lock(&time_info);

    // time_sum += (double) msec_diff;
    time_sum += (double) duration.count();

    pthread_mutex_unlock(&time_info);

    close(my_socket);
    cout << "[+] Connection closed, thread " << id << endl;
    pthread_exit(NULL);    
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
    int len = recv(my_socket, receive_buffer, LEN, 0); 
    if(len == 0){
        return 1;
    }
    char num[len + 1];
    strcpy(num, receive_buffer);

    // (2) Reenvia o número aleatório:
    send(my_socket, num, len + 1, 0);

    int names_num = atoi(num);
    for(int i = 0; i < names_num; i++){

        // (4) Recebe a primeira mensagem (char array):
        msg_len = recv(my_socket, receive_buffer, LEN, 0);

        // (5) Verifica se a mensagem (char array) é vazia:
        if(msg_len == 0){
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

    // (2) Espera waiting do servidor:
    msg_len = recv(my_socket, receive_buffer, LEN, 0);
    if(msg_len == 0 or strcmp(receive_buffer, "waiting") != 0){
        return 1;
    }
    memset(receive_buffer, 0, LEN);

    // (3) Obtém tamanho do arquivo:
    int size = getFileSize(fptr);
    int len = snprintf(NULL, 0, "%d", size);
    char num[len+1];
    snprintf(num, len+1, "%d", size);

    // (3) Envia tamanho do arquivo e recebe novamente:
    send(my_socket, num, len + 1, 0);
    msg_len = recv(my_socket, receive_buffer, LEN, 0);
    if(msg_len == 0 or strcmp(receive_buffer, num) != 0){
        return 1;
    }
    memset(receive_buffer, 0, LEN);

    // (3) Cria buffer para o arquivo:
    char send_buffer[PACKET];
    int read_size;

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

    // (2) Recebe o tamanho do arquivo:
    int len = recv(my_socket, receive_buffer, LEN, 0);
    if(len == 0){
        cout << "[+] Connection problem, stop download" << endl;
        cout.clear();
        return 1;
    }
    char num[len + 1];
    strcpy(num, receive_buffer);

    // cout << "Download de tamanho " << num << " bytes" << endl;

    // (3) Reenvia o tamanho do arquivo:
    send(my_socket, num, len + 1, 0);

    int file_size = atoi(num);
    int position = 0;
    while(true){
        // (4) Limpa o buffer:
        memset(receive_buffer, LEN, 0);
        // (5) Recebe um pedaço do arquivo:
        msg_len = recv(my_socket, receive_buffer, LEN, 0);
        // (6) Verifica se o pedaço do arquivo é vazio:
        if(msg_len == 0) return 1;
        // (7) Escreve o pedaço do arquivo:
        fwrite(receive_buffer, sizeof(char), msg_len, fptr);
        position += msg_len;
        // (8) Envia "waiting" para o servidor:
        if(file_size > position) send(my_socket, "waiting", sizeof("waiting"), 0);
        else break;
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