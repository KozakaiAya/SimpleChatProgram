//
// Created by qian on 18-6-19.
//

#include "User.h"

User::User(int fd, sockaddr_in connectIn)
{
    this->id = fd;
    this->userSocket = connectIn;
    this->ip = inet_ntoa(connectIn.sin_addr);
    this->port = connectIn.sin_port;
    this->isValid = true;
}

sockaddr_in User::getSockAddrIn()
{
    return this->userSocket;
}

