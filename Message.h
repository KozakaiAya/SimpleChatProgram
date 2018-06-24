//
// Created by qian on 18-6-19.
//

#ifndef SIMPLECHATPROGRAM_MESSAGE_H
#define SIMPLECHATPROGRAM_MESSAGE_H

#include <string>
#include <cstdint>

using namespace std;

enum class MsgType
{
    SND, RPY, CMD, ACK
};
enum class CmdType
{
    TIME, NAME, LIST, CONN, SEND, DISC
};

class Message
{
public:
    int length;
    MsgType type;
    CmdType cmdType;
    int sendToID;
    string payload;
    bool isValid;

    Message() = default;

    ~Message() {};
    //string getSendOrReceiveFormat();
};

class RecvMsg : public Message
{
public:
    RecvMsg(string msg);

private:
    static uint16_t convertBinToUint16(string str);
};

class SndMsg : public Message
{
public:
    int socketFD;

    SndMsg() : Message() {};

    //Normal Message
    SndMsg(int socketFD, int fd, MsgType msgType, string payload);

    // Command
    SndMsg(int socketFD,
           int fd, MsgType msgType, CmdType cmdType, string payload);

    int msgSend();

    static string convertUint16ToBin(uint16_t n);
};

ostream &operator<<(ostream &os, const Message &recvMsg);

string to_string(MsgType msgType);

string to_string(CmdType cmdType);

#endif //SIMPLECHATPROGRAM_MESSAGE_H
