//
// Created by qian on 18-6-19.
//

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <queue>

#include "Message.h"
#include "User.h"

using namespace std;

bool isConnected = false;
bool tryExit = false;
bool realExit = true;

queue<SndMsg> sendQueue;
queue<RecvMsg> recvQueue;
mutex sendQ_mutex;
mutex recvQ_mutex;

void messageRecv(int sockFD);

void messageSend(int sockFD);


int main(void)
{
    int client = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in localaddr;
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localaddr.sin_port = 9999;  // Any local port will do
    bind(client, (struct sockaddr *) &localaddr, sizeof(localaddr));

    thread *recvThread;
    thread *sendThread;

    while (1)
    {
        string command;
        cin >> command;
        if (command[0] != '/')
        {
            cerr << "Command should start with \'/\', type /HELP to get the available commands" << endl;
        }
        command.erase(0, 1);
        if (command == "CONNECT")
        {
            cout << "Please input server IP and Port, example: \"127.0.0.1:2333\"" << endl;
            string server;
            cin >> server;
            string serverIP = server.substr(0, server.find(':'));
            int serverPort = stoi(server.substr(server.find(':') + 1, server.length() - serverIP.length() - 1));
            struct sockaddr_in server_addr;
            server_addr.sin_port = htons(serverPort);
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(serverIP.c_str());
            if (connect(client, (struct sockaddr *) &server_addr, sizeof(server_addr)) == 0)
                cout << "Successfully connected to " << server << endl;

            isConnected = true;

            recvThread = new thread(messageRecv, client);
            sendThread = new thread(messageSend, client);
        } else if (command == "DISCONNECT")
        {
            if (!isConnected)
                cerr << "You should connect to a server first" << endl;
            else
            {
                SndMsg sendMsg(client, client, MsgType::CMD, CmdType::DISC, "");
                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
            }
        } else if (command == "TIME")
        {
            if (!isConnected)
                cerr << "You should connect to a server first" << endl;
            else
            {
                SndMsg sendMsg(client, client, MsgType::CMD, CmdType::TIME, "");
                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
            }
        } else if (command == "NAME")
        {
            if (!isConnected)
                cerr << "You should connect to a server first" << endl;
            else
            {
                SndMsg sendMsg(client, client, MsgType::CMD, CmdType::NAME, "");
                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
            }
        } else if (command == "LIST")
        {
            if (!isConnected)
                cerr << "You should connect to a server first" << endl;
            else
            {
                SndMsg sendMsg(client, client, MsgType::CMD, CmdType::LIST, "");
                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
            }
        } else if (command == "SEND")
        {
            if (!isConnected)
                cerr << "You should connect to a server first" << endl;
            else
            {
                string sendFD, payload;
                cout << "ID: ";
                cin >> sendFD;
                cout << "Message: ";
                cin >> payload;
                SndMsg sendMsg(client, stoi(sendFD), MsgType::CMD, CmdType::TIME, "");
                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
            }
        } else if (command == "EXIT")
        {
            if (!isConnected)
            {
                break;
            } else
            {
                SndMsg sendMsg(client, client, MsgType::CMD, CmdType::DISC, "");
                tryExit = true;
                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
            }
        }

        //Display Message
        recvQ_mutex.lock();
        while (!recvQueue.empty())
        {
            RecvMsg msg = recvQueue.front();
            cout << msg.payload;
            recvQueue.pop();
            if (msg.type == MsgType::SND)
            {
                sendQ_mutex.lock();
                SndMsg sndMsg(client, msg.sendToID, MsgType::ACK, CmdType::SEND, msg.payload);
                sendQ_mutex.unlock();
            }
            if ((msg.type == MsgType::ACK) && (msg.cmdType == CmdType::DISC) && (tryExit))
                break;
        }
        recvQ_mutex.unlock();
    }
    realExit = true;
    sendThread->join();
    recvThread->join();
    cout << "Client Exit..." << endl;
    return 0;
}

void messageRecv(int sockFD)
{
    char buf[65536];
    while (1)
    {
        if (realExit) break;
        int n = recv(sockFD, buf, 65536, 0);
        buf[n] = '\0';
        RecvMsg msg(buf);
        recvQ_mutex.lock();
        recvQueue.push(msg);
        recvQ_mutex.unlock();
    }
    return;
}


void messageSend(int sockFD)
{
    while (1)
    {
        if (realExit) break;
        sendQ_mutex.lock();
        while (!sendQueue.empty())
        {
            SndMsg msg = sendQueue.front();
            msg.msgSend();
            sendQueue.pop();
        }
        sendQ_mutex.unlock();
    }
    return;
}
