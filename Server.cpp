#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <queue>
#include <vector>
#include <signal.h>
#include <cstdlib>
#include <ctime>


#include "Message.h"
#include "User.h"

using namespace std;

queue<SndMsg> sendQueue;
queue<RecvMsg> recvQueue;
mutex sendQ_mutex;
mutex recvQ_mutex;
vector<User> userList;
vector<thread *> threadList;

void ctrlCHandler(int x);

void userHandler(User &user);

void messageSend(void);

int main(void)
{
    signal(SIGINT, ctrlCHandler);

    int sock;
    struct sockaddr_in sockName;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        cerr << "Socket" << endl;
        exit(EXIT_FAILURE);
    }

    sockName.sin_family = AF_INET;
    sockName.sin_port = htons(5246);
    sockName.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *) &sockName, sizeof(sockName)) < 0)
    {
        cerr << "Bind Failed" << endl;
        exit(EXIT_FAILURE);
    }

    if (listen(sock, 1) == -1)
    {
        cerr << "Listen Failed" << endl;
        exit(EXIT_FAILURE);
    }

    thread *sendMsgThread = new thread(messageSend);
    sendMsgThread->detach();
    threadList.push_back(sendMsgThread);

    while (1)
    {
        struct sockaddr_in connIn;
        socklen_t connInLength = sizeof(struct sockaddr);
        int connFD = accept(sock, (struct sockaddr *) &connIn, &connInLength);
        if (connFD > 0)
        {
            cout << "Client: " << connFD << " connected." << endl;
            User user(connFD, connIn);
            userList.push_back(user);
            thread *thread1 = new thread(userHandler, ref(user));
            thread1->detach();
            threadList.push_back(thread1);
        } else
        {
            cerr << connFD << " No incoming connections." << endl;
        }
    }
}

void userHandler(User &user)
{
    char buf[65536];
    SndMsg ackMsg(user.id, user.id, MsgType::ACK, CmdType::CONN, "Server: Connection Established");
    ackMsg.msgSend();
    bool isTerminated = false;
    while (1)
    {
        int n = recv(user.id, buf, 65536, 0);
        buf[n] = '\0';
        RecvMsg msg(buf);

        cout << "Recv: " << msg << endl;

        SndMsg sendMsg;
        if (msg.type == MsgType::CMD)
        {
            switch (msg.cmdType)
            {
                case CmdType::SEND:
                {
                    sendMsg = SndMsg(msg.sendToID, user.id, MsgType::SND, msg.payload);
                    break;
                }
                case CmdType::NAME:
                {
                    char nameBuf[100];
                    int retN = gethostname(nameBuf, 100);
                    nameBuf[retN] = '\0';
                    sendMsg = SndMsg(user.id, user.id, MsgType::CMD, CmdType::NAME, nameBuf);
                    break;
                }
                case CmdType::TIME:
                {
                    char timeBuf[100];
                    struct tm *timeInfo;
                    time_t rawTime;
                    time(&rawTime);
                    timeInfo = localtime(&rawTime);
                    strftime(timeBuf, sizeof(timeBuf), "%d-%m-%Y %I:%M:%S", timeInfo);
                    sendMsg = SndMsg(user.id, user.id, MsgType::CMD, CmdType::TIME, timeBuf);
                    break;
                }
                case CmdType::LIST:
                {
                    string listBuf;
                    uint16_t count;
                    for (auto x: userList)
                    {
                        if (x.isValid)
                        {
                            listBuf += to_string(x.id) + "\t" + x.ip + "\t" + to_string(x.port) + "\n";
                            count++;
                        }
                    }
                    listBuf = "Count: " + to_string(count) + "\n" + listBuf;
                    sendMsg = SndMsg(user.id, user.id, MsgType::CMD, CmdType::LIST, listBuf);
                    break;
                }
                case CmdType::DISC:
                {
                    sendMsg = SndMsg(user.id, user.id, MsgType::ACK, CmdType::DISC, "Server: Connection Closed");
                    sendMsg.msgSend();
                    user.isValid = false;
                    shutdown(user.id, 0);
                    //terminate();
                    isTerminated = true;
                    break;
                }
            }
            //sendMsg.msgSend();
        } else if (msg.type == MsgType::ACK)
        {
            if (msg.cmdType == CmdType::SEND)
            {
                sendMsg = SndMsg(msg.sendToID, user.id, MsgType::ACK, CmdType::SEND, msg.payload);
                //sendMsg.msgSend();
            }
        }

        if (isTerminated) break;
        sendQ_mutex.lock();
        sendQueue.push(sendMsg);
        sendQ_mutex.unlock();
    }
    return;
}

void ctrlCHandler(int x)
{
    for (auto x:userList)
    {
        SndMsg sendMsg(x.id, x.id, MsgType::ACK, CmdType::DISC, "Server: Connection Closed");
        sendMsg.msgSend();
        shutdown(x.id, 0);
    }
    cout << "Server Shutdown" << endl;
    return;
}

void messageSend(void)
{
    cout << "Send Thread Start" << endl;
    while (1)
    {
        //cout << "Send Thread Start" << endl;
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



