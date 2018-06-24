//
// Created by qian on 18-6-19.
//
#include <cstdint>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include "Message.h"

string to_string(CmdType cmdType)
{
    switch (cmdType)
    {
        case CmdType::SEND:
            return "SEND";
            break;
        case CmdType::DISC:
            return "DISC";
            break;
        case CmdType::TIME:
            return "TIME";
            break;
        case CmdType::CONN:
            return "CONN";
            break;
        case CmdType::LIST:
            return "LIST:";
            break;
        case CmdType::NAME:
            return "NAME";
            break;
    }
}

string to_string(MsgType msgType)
{
    switch (msgType)
    {
        case MsgType::ACK:
            return "ACK";
            break;
        case MsgType::CMD:
            return "CMD";
            break;
        case MsgType::RPY:
            return "RPY";
            break;
        case MsgType::SND:
            return "SND";
            break;
    }
}

ostream &operator<<(ostream &os, const Message &msg)
{
    os << "Length: " << msg.length << "Type: " << to_string(msg.type);
    if (msg.type == MsgType::CMD || msg.type == MsgType::ACK)
    {
        os << "CmdType: " << to_string(msg.cmdType);
        if (msg.cmdType == CmdType::SEND)
            os << "Target: " << msg.sendToID;
    }
    os << "Payload: " << msg.payload;
    return os;
}

RecvMsg::RecvMsg(string msg)
{
    this->isValid = true;
    if ((msg.find("$$") == 0) && (msg.rfind("$$") == (msg.length() - 2)))
    {
        string lenT = msg.substr(9, 2);
        this->length = convertBinToUint16(lenT);
        if (this->length == msg.length())
        {
            string typeT = msg.substr(18, 3);
            if (typeT == "CMD")
                this->type = MsgType::CMD;
            else if (typeT == "ACK")
                this->type = MsgType::ACK;
            else if (typeT == "SND")
                this->type = MsgType::SND;
            else
                this->isValid = false;
            if (typeT == "CMD" || typeT == "ACK")
            {
                string cmdTypeT = msg.substr(31, 4);
                if (cmdTypeT == "TIME")
                    this->cmdType = CmdType::TIME;
                else if (cmdTypeT == "NAME")
                    this->cmdType = CmdType::NAME;
                else if (cmdTypeT == "LIST")
                    this->cmdType = CmdType::LIST;
                else if (cmdTypeT == "CONN")
                    this->cmdType = CmdType::CONN;
                else if (cmdTypeT == "SEND")
                {
                    this->cmdType = CmdType::SEND;
                    string sendTargetT = msg.substr(44, 2);
                    this->sendToID = convertBinToUint16(sendTargetT);
                } else if (cmdTypeT == "DISC")
                    this->cmdType = CmdType::DISC;
                else
                    this->isValid = false;

                if (this->cmdType == CmdType::SEND)
                    this->payload = msg.substr(48, msg.length() - 48 - 2);
                else
                    this->payload = msg.substr(37, msg.length() - 37 - 2);
            } else
            {
                string sendTargetT = msg.substr(30, 2);
                this->sendToID = convertBinToUint16(sendTargetT);
                this->payload = msg.substr(34, msg.length() - 34 - 2);
            }
        }
    } else
        this->isValid = false;
}

uint16_t RecvMsg::convertBinToUint16(string str)
{
    char lenBuf[2];
    lenBuf[0] = str.c_str()[0];
    lenBuf[1] = str.c_str()[1];
    uint16_t ret = *(reinterpret_cast<uint16_t *>(lenBuf));
    return ret;
}

SndMsg::SndMsg(int socketFD, int fd, MsgType msgType, string payload)
{
    this->socketFD = socketFD;
    this->sendToID = fd;
    this->type = msgType;
    this->payload = payload;
    this->length = 34 + payload.length() + 2;
}

SndMsg::SndMsg(int socketFD, int fd, MsgType msgType, CmdType cmdType, string payload)
{
    this->socketFD = socketFD;
    this->sendToID = fd;
    this->type = msgType;
    this->cmdType = cmdType;
    this->payload = payload;
    if (cmdType == CmdType::SEND)
        this->length = 48 + payload.length() + 2;
    else
        this->length = 37 + payload.length() + 2;
}

string SndMsg::convertUint16ToBin(uint16_t n)
{
    char buf[3];
    buf[0] = reinterpret_cast<char *>(&n)[0];
    buf[1] = reinterpret_cast<char *>(&n)[1];
    buf[2] = '\0';
    return string(buf);
}

int SndMsg::msgSend()
{
    string sndBuf = "$$Length:" + convertUint16ToBin(this->length) + "##" + "Type:" + to_string(this->type) + "##";
    if (this->type == MsgType::CMD || this->type == MsgType::ACK)
    {
        sndBuf += "CMDType:" + to_string(this->cmdType) + "##";
        if (this->cmdType == CmdType::SEND)
            sndBuf += "Target:" + convertUint16ToBin(this->sendToID) + "##";
    }
    if (this->type == MsgType::SND)
    {
        sndBuf += "Target:" + convertUint16ToBin(this->sendToID) + "##";
    }
    sndBuf += this->payload + "$$";
    send(this->socketFD, sndBuf.c_str(), sndBuf.length(), 0);
}