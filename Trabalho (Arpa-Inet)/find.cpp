#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
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

int main(void){

    char temp[] = "a /home/bruno/REC/Trabalho (Arpa-Inet)/find.cpp \\";
    // getline(cin, temp);
    // stringstream command(temp);
    // string token;
    // vector<string> tokens;
    // while(getline(command, token, ',')){
    //     tokens.push_back(token);
    // }
    // cout << tokens.size() << endl;

    char temp1[100];
    char temp2[100];

    sscanf(temp, "%s %[^\n]", temp1, temp2);

    cout << temp1 << endl;
    cout << temp2 << endl;
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