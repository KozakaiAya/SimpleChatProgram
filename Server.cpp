#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <queue>
#include <vector>
#include <signal.h>
#include <cstdlib>
#include <ctime>
#include <set>
#include <poll.h>


#include "Message.h"
#include "User.h"

#define USER_TIME_OUT 6000000

using namespace std;

queue<SndMsg> sendQueue;
queue<RecvMsg> recvQueue;
mutex sendQ_mutex;
mutex recvQ_mutex;
vector<User> userList;
vector<thread *> threadList;
set<int> userFDSet;

void ctrlCHandler(int x);

void userHandler(User &user);

void messageSend(void);

int main(void)
{
    cout << "Welcome to Chat server, you can only see the message send to and from the server" << endl;
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
            userFDSet.insert(connFD);
            userList.push_back(user);
            thread *thread1 = new thread(userHandler, ref(userList.back()));
            thread1->detach();
            threadList.push_back(thread1);
        }
        else
        {
            cerr << connFD << " No incoming connections." << endl;
        }
    }
}

void userHandler(User &user)
{
    char buf[65536];
    string ackString = "Server: Connection Established\tID: " + to_string(user.id);
    SndMsg ackMsg(user.id, user.id, MsgType::ACK, CmdType::CONN, ackString);
    ackMsg.msgSend();
    bool isTerminated = false;
    while (1)
    {
        struct pollfd pollFD;
        int ret;

        pollFD.fd = user.id; // your socket handler
        pollFD.events = POLLIN;
        ret = poll(&pollFD, 1, USER_TIME_OUT); // 1 second for timeout
        switch (ret)
        {
            case -1:
                // Error
                break;
            case 0:
                // Timeout
                cout << "User " << user.id << " timeout" << endl;
                user.isValid = false;
                isTerminated = true;
                break;
            default:
                int n = recv(user.id, buf, 65536, 0);
                buf[n] = '\0';
                break;
        }
        if (isTerminated) break;

        RecvMsg msg(buf);

        cout << "Recv: " << msg << endl;

        SndMsg sendMsg;
        if (msg.type == MsgType::CMD)
        {
            switch (msg.cmdType)
            {
                case CmdType::SEND:
                {
                    string buf = "From: " + to_string(user.id) + "\n" + msg.payload;
                    if (userFDSet.find(msg.sendToID) != userFDSet.end())
                    {
                        sendMsg = SndMsg(msg.sendToID, user.id, MsgType::SND, buf);
                    }
                    else
                    {
                        string errInfo = "No Client with ID: " + to_string(msg.sendToID);
                        sendMsg = SndMsg(user.id, msg.sendToID, MsgType::ERR, CmdType::SEND, errInfo);
                    }
                    break;
                }
                case CmdType::NAME:
                {
                    char bufC[512];
                    string buf;
                    if (gethostname(bufC, 512) == 0)
                    { // success = 0, failure = -1
                        buf = bufC;
                    }
                    sendMsg = SndMsg(user.id, user.id, MsgType::CMD, CmdType::NAME, buf);
                    break;
                }
                case CmdType::TIME:
                {
                    char timeBuf[100];
                    struct tm *timeInfo;
                    time_t rawTime;
                    time(&rawTime);
                    timeInfo = localtime(&rawTime);
                    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %I:%M:%S", timeInfo);
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
                    userFDSet.erase(user.id);
                    user.isValid = false;
                    shutdown(user.id, 2);
                    //terminate();
                    isTerminated = true;
                    break;
                }
                case CmdType::LIVE:
                {
                    break;
                }
            }
            //sendMsg.msgSend();
        }
        else if (msg.type == MsgType::ACK)
        {
            if (msg.cmdType == CmdType::SEND)
            {
                sendMsg = SndMsg(msg.sendToID, user.id, MsgType::ACK, CmdType::SEND, msg.payload);
                //sendMsg.msgSend();
            }
        }

        if (isTerminated) break;
        if ((msg.type != MsgType::CMD) || (msg.cmdType != CmdType::LIVE))
        {
            sendQ_mutex.lock();
            sendQueue.push(sendMsg);
            sendQ_mutex.unlock();
        }
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



