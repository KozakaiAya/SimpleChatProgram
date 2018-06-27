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
#include <chrono>
#include <poll.h>

#include "Message.h"
#include "User.h"

#define USER_TIME_OUT 6000000

typedef pair<int, string> msgID;

using namespace std;

bool isConnected = false;
bool tryExit = false;
bool realExit = false;
bool connectionLoss = false;

queue<SndMsg> sendQueue;
queue<RecvMsg> recvQueue;
mutex sendQ_mutex;
mutex recvQ_mutex;
set<msgID> ackSet;
mutex ackSet_mutex;

void messageRecv(int sockFD);

void messageSend(int sockFD);

void keepAlive(int sockFD);


int main(void)
{
    int client;
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

    cout << "Client Started. Type \"/HELP\" to see available comands." << endl;

    thread *recvThread;
    thread *sendThread;
    thread *keepAliveThread;

    while (1)
    {
        string command;
        cin >> command;
        if (realExit) break;
        if (command[0] != '/')
        {
            cerr << "Command should start with \'/\', type /HELP to get the available commands" << endl;
        }
        command.erase(0, 1);

        //cout << "Command: " << command << endl;
        if (command == "CONNECT")
        {
            if (!isConnected)
            {
                client = socket(AF_INET, SOCK_STREAM, 0);
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
                    connectionLoss = false;

                    recvThread = new thread(messageRecv, client);
                    sendThread = new thread(messageSend, client);
                    keepAliveThread = new thread(keepAlive, client);
                    keepAliveThread->detach();
                }
                else
                {
                    cerr << "Connection Failed." << endl;
                }
            }
            else
            {
                cout << "Already connected" << endl;
            }
        }
        else if (command == "DISCONNECT")
        {
            if (!isConnected)
                cerr << "You should connect to a server first" << endl;
            else
            {
                SndMsg sendMsg(client, client, MsgType::CMD, CmdType::DISC, "");
                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
                isConnected = false;
            }
        }
        else if (command == "TIME")
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
        }
        else if (command == "NAME")
        {
            if (!isConnected)
                cerr << "You should connect to a server first" << endl;
            else
            {
                SndMsg sendMsg(client, client, MsgType::CMD, CmdType::NAME, "");
                //cout << "Prepare to Send: " << sendMsg << endl;
                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
            }
        }
        else if (command == "LIST")
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
        }
        else if (command == "SEND")
        {
            if (!isConnected)
                cerr << "You should connect to a server first" << endl;
            else
            {
                string sendFD, payload;
                cout << "ID: ";
                cin >> sendFD;
                cout << "Message: ";
                cin >> ws;
                getline(cin, payload);
                SndMsg sendMsg(client, stoi(sendFD), MsgType::CMD, CmdType::SEND, payload);

                ackSet_mutex.lock();
                ackSet.insert(make_pair(sendMsg.sendToID, sendMsg.payload));
                ackSet_mutex.unlock();

                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
            }
        }
        else if (command == "EXIT")
        {
            if (!isConnected)
            {
                break;
            }
            else
            {
                SndMsg sendMsg(client, client, MsgType::CMD, CmdType::DISC, "");
                tryExit = true;
                sendQ_mutex.lock();
                sendQueue.push(sendMsg);
                sendQ_mutex.unlock();
            }
        }
        else if (command == "HELP")
        {
            cout << "Available commands: " << endl << "/CONNECT\t/DISCONNECT\t/TIME\t/NAME\t/LIST\t/SEND\t/EXIT"
                 << endl;
        }
        else
        {
            cout << "Unknown command, type \"/HELP\" to see available commands" << endl;
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
    //cout << "Recv Thread Start" << endl;
    char buf[65536];
    while (1)
    {
        //cout << "Recv Thread Start" << endl;
        //if (!isConnected) break;
        struct pollfd pollFD;
        int ret;

        pollFD.fd = sockFD; // your socket handler
        pollFD.events = POLLIN;
        ret = poll(&pollFD, 1, USER_TIME_OUT); // 1 second for timeout
        switch (ret)
        {
            case -1:
                // Error
                break;
            case 0:
                // Timeout
                cout << "Server timeout" << endl;
                connectionLoss = true;
                isConnected = false;
                break;
            default:
                int n = recv(sockFD, buf, 65536, 0);
                buf[n] = '\0';
                break;
        }
        if (connectionLoss) break;
        RecvMsg msg(buf);

        //cout << "Recv: " << msg << endl;

        if (!((msg.type == MsgType::ACK) && (msg.cmdType == CmdType::SEND))) cout << msg.payload << endl;
        //recvQueue.pop();
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
            string realPayload = msg.payload.substr(msg.payload.find('\n') + 1,
                                                    msg.payload.length() - msg.payload.find('\n') - 1);
            bool isIn = ackSet.find(make_pair(msg.sendToID, realPayload)) != ackSet.end();
            if (isIn)
            {
                cout << "Message Sent" << endl;
                ackSet.erase(make_pair(msg.sendToID, msg.payload));
            }
            ackSet_mutex.unlock();
        }
        if ((msg.type == MsgType::ACK) && (msg.cmdType == CmdType::DISC) && tryExit)
        {
            realExit = true;
            cout << "Input anything to exit..." << endl;
            //while (!recvQueue.empty())
            //{
            //    recvQueue.pop();
            //}
            break;
        }
        if ((msg.type == MsgType::ACK) && (msg.cmdType == CmdType::DISC) && !isConnected)
        {
            break;
        }
    }
    shutdown(sockFD, 2);
    return;
}


void messageSend(int sockFD)
{
    //cout << "Send Thread Start" << endl;
    while (1)
    {
        //cout << "Send Thread Start" << endl;
        if (!isConnected && sendQueue.empty()) break;
        if (connectionLoss) break;
        if (realExit) break;
        sendQ_mutex.lock();
        while (!sendQueue.empty())
        {
            SndMsg msg = sendQueue.front();
            //cout << "Send: " << msg << endl;
            msg.msgSend();
            sendQueue.pop();
        }
        sendQ_mutex.unlock();
    }
    //shutdown(sockFD, 0);
    return;
}


void keepAlive(int sockFD)
{
    while (1)
    {
        if (realExit) break;
        //cout << "Send Keepalive" << endl;
        SndMsg msg(sockFD, sockFD, MsgType::CMD, CmdType::LIVE, "");
        if (isConnected)
        {
            sendQ_mutex.lock();
            sendQueue.push(msg);
            sendQ_mutex.unlock();
        }
        else
        {
            break;
        }
        this_thread::sleep_for(chrono::seconds(100));
    }
}
