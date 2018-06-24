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
#include <set>

#include "Message.h"
#include "User.h"

typedef pair<int, string> msgID;

using namespace std;

bool isConnected = false;
bool tryExit = false;
bool realExit = false;

queue<SndMsg> sendQueue;
queue<RecvMsg> recvQueue;
mutex sendQ_mutex;
mutex recvQ_mutex;
set<msgID> ackSet;
mutex ackSet_mutex;

void messageRecv(int sockFD);

void messageSend(int sockFD);


int main(void)
{
    int client = socket(AF_INET, SOCK_STREAM, 0);
    /*while (1)
    {
        cout << "Please input port: " << endl;
        int port;
        cin >> port;
        client = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in localaddr;
        localaddr.sin_family = AF_INET;
        localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localaddr.sin_port = port;  // Any local port will do
        if (bind(client, (struct sockaddr *) &localaddr, sizeof(localaddr)) == 0)
            break;
    }*/

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

        cout << "Command: " << command << endl;
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
            {
                cout << "Successfully connected to " << server << endl;

                isConnected = true;

                recvThread = new thread(messageRecv, client);
                sendThread = new thread(messageSend, client);
            } else
            {
                cerr << "Connection Failed." << endl;
            }
        } else if (command == "DISCONNECT")
        {
            if (!isConnected)
                cerr << "You should connect to a server first" << endl;
            else
            {
                SndMsg sendMsg(client, client, MsgType::CMD, CmdType::DISC, "");
                isConnected = false;
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
                cout << "Prepare to Send: " << sendMsg << endl;
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
                SndMsg sendMsg(client, stoi(sendFD), MsgType::CMD, CmdType::SEND, payload);

                ackSet_mutex.lock();
                ackSet.insert(make_pair(sendMsg.sendToID, sendMsg.payload));
                ackSet_mutex.unlock();

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

        if (realExit) break;
    }
    realExit = true;
    sendThread->join();
    recvThread->join();
    cout << "Client Exit..." << endl;
    return 0;
}

void messageRecv(int sockFD)
{
    cout << "Recv Thread Start" << endl;
    char buf[65536];
    while (1)
    {
        //cout << "Recv Thread Start" << endl;
        int n = recv(sockFD, buf, 65536, 0);
        buf[n] = '\0';
        RecvMsg msg(buf);

        cout << "Recv: " << msg << endl;

        cout << msg.payload << endl;
        recvQueue.pop();
        if (msg.type == MsgType::SND)
        {
            sendQ_mutex.lock();
            SndMsg sndMsg(sockFD, msg.sendToID, MsgType::ACK, CmdType::SEND, msg.payload);
            sendQueue.push(sndMsg);
            sendQ_mutex.unlock();
        }
        if (msg.type == MsgType::ACK && (msg.cmdType == CmdType::SEND))
        {
            ackSet_mutex.lock();
            bool isIn = ackSet.find(make_pair(msg.sendToID, msg.payload)) != ackSet.end();
            if (isIn)
            {
                cout << "Messag Sent" << endl;
                ackSet.erase(make_pair(msg.sendToID, msg.payload));
            }
            ackSet_mutex.unlock();
        }
        if ((msg.type == MsgType::ACK) && (msg.cmdType == CmdType::DISC) && (tryExit))
        {
            realExit = true;
            break;
        }
    }
    return;
}


void messageSend(int sockFD)
{
    cout << "Send Thread Start" << endl;
    while (1)
    {
        //cout << "Send Thread Start" << endl;
        if (realExit) break;
        sendQ_mutex.lock();
        while (!sendQueue.empty())
        {
            SndMsg msg = sendQueue.front();
            cout << "Send: " << msg << endl;
            msg.msgSend();
            sendQueue.pop();
        }
        sendQ_mutex.unlock();
    }
    return;
}
