#pragma once
#include <Network\CPacket.h>
#include "CommonProtocol.h"

using namespace onenewarm;

template<typename T>
class MonitorPacketHandler
{
public:
    bool PacketProc(T* netInterface, __int64 sessionId, CPacketRef recvPktRef);
    virtual bool Handle_EN_PACKET_CS_CHAT_REQ_LOGIN(__int64 sessionId, __int64 AccountNo, char* ID, char* Nickname, char* SessionKey) { return true; }
    virtual bool Handle_EN_PACKET_CS_CHAT_RES_LOGIN(__int64 sessionId, byte Status, __int64 AccountNo) { return true; }
    virtual bool Handle_EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE(__int64 sessionId, __int64 AccountNo, WORD SectorX, WORD SectorY) { return true; }
    virtual bool Handle_EN_PACKET_CS_CHAT_RES_SECTOR_MOVE(__int64 sessionId, __int64 AccountNo, WORD SectorX, WORD SectorY) { return true; }
    virtual bool Handle_EN_PACKET_CS_CHAT_REQ_MESSAGE(__int64 sessionId, __int64 AccountNo, WORD MessageLen, char* Message) { return true; }
    virtual bool Handle_EN_PACKET_CS_CHAT_RES_MESSAGE(__int64 sessionId, __int64 AccountNo, char* ID, char* Nickname, WORD MessageLen, char* Message) { return true; }
    virtual bool Handle_EN_PACKET_CS_CHAT_REQ_HEARTBEAT(__int64 sessionId) { return true; }
    virtual bool Handle_EN_PACKET_SS_MONITOR_LOGIN(__int64 sessionId, int ServerNo) { return true; }
    virtual bool Handle_EN_PACKET_SS_MONITOR_DATA_UPDATE(__int64 sessionId, byte DataType, int DataValue, int TimeStamp) { return true; }
    CPacketRef Make_EN_PACKET_CS_CHAT_REQ_LOGIN_Packet(T* netInterface,, __int64 AccountNo, char* ID, char* Nickname, char* SessionKey);
    CPacketRef Make_EN_PACKET_CS_CHAT_RES_LOGIN_Packet(T* netInterface,, byte Status, __int64 AccountNo);
    CPacketRef Make_EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE_Packet(T* netInterface,, __int64 AccountNo, WORD SectorX, WORD SectorY);
    CPacketRef Make_EN_PACKET_CS_CHAT_RES_SECTOR_MOVE_Packet(T* netInterface,, __int64 AccountNo, WORD SectorX, WORD SectorY);
    CPacketRef Make_EN_PACKET_CS_CHAT_REQ_MESSAGE_Packet(T* netInterface,, __int64 AccountNo, WORD MessageLen, char* Message);
    CPacketRef Make_EN_PACKET_CS_CHAT_RES_MESSAGE_Packet(T* netInterface,, __int64 AccountNo, char* ID, char* Nickname, WORD MessageLen, char* Message);
    CPacketRef Make_EN_PACKET_CS_CHAT_REQ_HEARTBEAT_Packet(T* netInterface,);
    CPacketRef Make_EN_PACKET_SS_MONITOR_LOGIN_Packet(T* netInterface,, int ServerNo);
    CPacketRef Make_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(T* netInterface,, byte DataType, int DataValue, int TimeStamp);
};

template<typename T>
bool MonitorPacketHandler<T>::PacketProc(T* netInterface, __int64 sessionId, CPacketRef recvPktRef)
{
    WORD packetId;
    try {
        *(*recvPktRef) >> packetId;
    }
    catch (int e) {
        netInterface->OnSessionError(sessionId, -1, L"Failed unmashalled packet type.", __FILE__, __LINE__);
        return false;
    }

    switch (packetId)
    {
    case EN_PACKET_CS_CHAT_REQ_LOGIN:
    {
        __int64 AccountNo;
        char ID[40];
        char Nickname[40];
        char SessionKey[64];
        try {
            *(*recvPktRef) >> AccountNo;
            (*recvPktRef)->GetData(ID, 40);
            (*recvPktRef)->GetData(Nickname, 40);
            (*recvPktRef)->GetData(SessionKey, 64);
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_LOGIN, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_CHAT_REQ_LOGIN(sessionId, AccountNo, ID, Nickname, SessionKey);
    }
    case EN_PACKET_CS_CHAT_RES_LOGIN:
    {
        byte Status;
        __int64 AccountNo;
        try {
            *(*recvPktRef) >> Status;
            *(*recvPktRef) >> AccountNo;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_CHAT_RES_LOGIN, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_CHAT_RES_LOGIN(sessionId, Status, AccountNo);
    }
    case EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
    {
        __int64 AccountNo;
        WORD SectorX;
        WORD SectorY;
        try {
            *(*recvPktRef) >> AccountNo;
            *(*recvPktRef) >> SectorX;
            *(*recvPktRef) >> SectorY;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE(sessionId, AccountNo, SectorX, SectorY);
    }
    case EN_PACKET_CS_CHAT_RES_SECTOR_MOVE:
    {
        __int64 AccountNo;
        WORD SectorX;
        WORD SectorY;
        try {
            *(*recvPktRef) >> AccountNo;
            *(*recvPktRef) >> SectorX;
            *(*recvPktRef) >> SectorY;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_CHAT_RES_SECTOR_MOVE, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_CHAT_RES_SECTOR_MOVE(sessionId, AccountNo, SectorX, SectorY);
    }
    case EN_PACKET_CS_CHAT_REQ_MESSAGE:
    {
        __int64 AccountNo;
        WORD MessageLen;
        char Message[200];
        try {
            *(*recvPktRef) >> AccountNo;
            *(*recvPktRef) >> MessageLen;
            (*recvPktRef)->GetData(Message, MessageLen);
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_MESSAGE, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_CHAT_REQ_MESSAGE(sessionId, AccountNo, MessageLen, Message);
    }
    case EN_PACKET_CS_CHAT_RES_MESSAGE:
    {
        __int64 AccountNo;
        char ID[40];
        char Nickname[40];
        WORD MessageLen;
        char Message[200];
        try {
            *(*recvPktRef) >> AccountNo;
            (*recvPktRef)->GetData(ID, 40);
            (*recvPktRef)->GetData(Nickname, 40);
            *(*recvPktRef) >> MessageLen;
            (*recvPktRef)->GetData(Message, MessageLen);
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_CHAT_RES_MESSAGE, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_CHAT_RES_MESSAGE(sessionId, AccountNo, ID, Nickname, MessageLen, Message);
    }
    case EN_PACKET_CS_CHAT_REQ_HEARTBEAT:
    {
        try {
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_CHAT_REQ_HEARTBEAT, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_CHAT_REQ_HEARTBEAT(sessionId);
    }
    case EN_PACKET_SS_MONITOR_LOGIN:
    {
        int ServerNo;
        try {
            *(*recvPktRef) >> ServerNo;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_SS_MONITOR_LOGIN, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_SS_MONITOR_LOGIN(sessionId, ServerNo);
    }
    case EN_PACKET_SS_MONITOR_DATA_UPDATE:
    {
        byte DataType;
        int DataValue;
        int TimeStamp;
        try {
            *(*recvPktRef) >> DataType;
            *(*recvPktRef) >> DataValue;
            *(*recvPktRef) >> TimeStamp;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_SS_MONITOR_DATA_UPDATE, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_SS_MONITOR_DATA_UPDATE(sessionId, DataType, DataValue, TimeStamp);
    }
    default:
    {
        netInterface->OnSessionError(sessionId, -1, L"Recved unvalid packet type.", __FILE__, __LINE__);
        return false;
    }
    }

}
template<typename T>
CPacketRef MonitorPacketHandler<T>::Make_EN_PACKET_CS_CHAT_REQ_LOGIN_Packet(T* netInterface,, __int64 AccountNo, char* ID, char* Nickname, char* SessionKey)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_CHAT_REQ_LOGIN;
    *(*pktRef) << AccountNo;
    (*pktRef)->PutData(ID, 40);
    (*pktRef)->PutData(Nickname, 40);
    (*pktRef)->PutData(SessionKey, 64);
    return pktRef;
}
template<typename T>
CPacketRef MonitorPacketHandler<T>::Make_EN_PACKET_CS_CHAT_RES_LOGIN_Packet(T* netInterface,, byte Status, __int64 AccountNo)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_CHAT_RES_LOGIN;
    *(*pktRef) << Status;
    *(*pktRef) << AccountNo;
    return pktRef;
}
template<typename T>
CPacketRef MonitorPacketHandler<T>::Make_EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE_Packet(T* netInterface,, __int64 AccountNo, WORD SectorX, WORD SectorY)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE;
    *(*pktRef) << AccountNo;
    *(*pktRef) << SectorX;
    *(*pktRef) << SectorY;
    return pktRef;
}
template<typename T>
CPacketRef MonitorPacketHandler<T>::Make_EN_PACKET_CS_CHAT_RES_SECTOR_MOVE_Packet(T* netInterface,, __int64 AccountNo, WORD SectorX, WORD SectorY)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_CHAT_RES_SECTOR_MOVE;
    *(*pktRef) << AccountNo;
    *(*pktRef) << SectorX;
    *(*pktRef) << SectorY;
    return pktRef;
}
template<typename T>
CPacketRef MonitorPacketHandler<T>::Make_EN_PACKET_CS_CHAT_REQ_MESSAGE_Packet(T* netInterface,, __int64 AccountNo, WORD MessageLen, char* Message)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_CHAT_REQ_MESSAGE;
    *(*pktRef) << AccountNo;
    *(*pktRef) << MessageLen;
    (*pktRef)->PutData(Message, MessageLen);
    return pktRef;
}
template<typename T>
CPacketRef MonitorPacketHandler<T>::Make_EN_PACKET_CS_CHAT_RES_MESSAGE_Packet(T* netInterface,, __int64 AccountNo, char* ID, char* Nickname, WORD MessageLen, char* Message)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_CHAT_RES_MESSAGE;
    *(*pktRef) << AccountNo;
    (*pktRef)->PutData(ID, 40);
    (*pktRef)->PutData(Nickname, 40);
    *(*pktRef) << MessageLen;
    (*pktRef)->PutData(Message, MessageLen);
    return pktRef;
}
template<typename T>
CPacketRef MonitorPacketHandler<T>::Make_EN_PACKET_CS_CHAT_REQ_HEARTBEAT_Packet(T* netInterface,)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_CHAT_REQ_HEARTBEAT;
    return pktRef;
}
template<typename T>
CPacketRef MonitorPacketHandler<T>::Make_EN_PACKET_SS_MONITOR_LOGIN_Packet(T* netInterface,, int ServerNo)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_SS_MONITOR_LOGIN;
    *(*pktRef) << ServerNo;
    return pktRef;
}
template<typename T>
CPacketRef MonitorPacketHandler<T>::Make_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(T* netInterface,, byte DataType, int DataValue, int TimeStamp)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_SS_MONITOR_DATA_UPDATE;
    *(*pktRef) << DataType;
    *(*pktRef) << DataValue;
    *(*pktRef) << TimeStamp;
    return pktRef;
}