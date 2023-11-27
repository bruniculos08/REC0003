#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <bits/stdc++.h>

using namespace std;

template <char C>
std::istream& expect(std::istream& in)
{
    if ((in >> std::ws).peek() == C) {
        in.ignore();
    }
    else {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

char regex_upload[] = "upload [a-zA-Z_] [a-zA-Z_0-9]*\\.[a-zA-Z0-9]+";
char regex_download[] = "download [^].[a-zA-Z0-9]+ [^]+";

int main(void){

    // char send_buffer[] = "download size.txt /home/bruno/REC/Trabalho (Arpa-Inet)/ClientFiles/";

    // char command[sizeof("download")];
    // char file_name[256];
    // char file_ext[256];
    // char path[256];
    // sscanf(send_buffer, "%s %[^.]%[^ ] %[^\n]", command, file_name, file_ext, path);

    // cout << file_ext << endl;

    // return 0;

    FILE *fptr;
    fptr = fopen("ClientFiles/unnamed.jpg", "rb");

    FILE *copy_fptr;
    copy_fptr = fopen("ClaientFiles/unnamed_copy.jpg", "wb");

    cout << copy_fptr << endl;

    // int read_size;
    // char send_buffer[10];

    // while(true){
    //     read_size = fread(send_buffer, sizeof(char), 10, fptr);
    //     cout << read_size << endl;
    //     fwrite(send_buffer, sizeof(char), read_size, copy_fptr);
    //     if(read_size < 10) break;
    // }

    return 0;

    struct addrinfo* res = NULL;
    getaddrinfo("0.tcp.sa.ngrok.io", "19664", 0, &res);

    struct addrinfo* i;

    int j = 0;
    for(i=res; i!=NULL; i=i->ai_next){
        char str[INET6_ADDRSTRLEN];
        if (i->ai_addr->sa_family == AF_INET) {
            struct sockaddr_in *p = (struct sockaddr_in *)i->ai_addr;
            cout << j << endl;
            j++;
            printf("%s\n", inet_ntop(AF_INET, &p->sin_addr, str, sizeof(str)));
        } 
        else if (i->ai_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *p = (struct sockaddr_in6 *)i->ai_addr;
            printf("%s\n", inet_ntop(AF_INET6, &p->sin6_addr, str, sizeof(str)));
        }
    }
    
}