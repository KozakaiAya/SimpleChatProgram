//
// Created by qian on 18-6-19.
//
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

#ifndef SIMPLECHATPROGRAM_USERLIST_H
#define SIMPLECHATPROGRAM_USERLIST_H

class User
{
private:
    sockaddr_in userSocket;
public:
    int id;
    string ip;
    int port;
    bool isValid;

    User(int id, sockaddr_in connectIn);

    sockaddr_in getSockAddrIn();
};


#endif //SIMPLECHATPROGRAM_USERLIST_H
