//
// Created by qian on 18-6-19.
//

#include "User.h"

User::User(sockaddr_in connectIn)
{
    this->userSocket = connectIn;
    this->ip = inet_ntoa(connectIn.sin_addr);
    this->port = connectIn.sin_port;
    this->isValid = true;
}

sockaddr_in User::getSockAddrIn()
{
    return this->userSocket;
}

