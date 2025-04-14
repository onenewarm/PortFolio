#pragma once
#include <Network\CPacket.h>
#include "CommonProtocol.h"

using namespace onenewarm;

template<typename T>
class PacketHandler
{
public:
    bool PacketProc(T* netInterface, __int64 sessionId, CPacketRef& recvPktRef);
    virtual bool Handle_EN_PACKET_CS_LOGIN_REQ_LOGIN(__int64 sessionId, __int64 AccountNo, char* SessionKey) { return true; }
    virtual bool Handle_EN_PACKET_CS_LOGIN_RES_LOGIN(__int64 sessionId, __int64 AccountNo, byte status, char* GameSessionKey, char* ChatSessionKey, char* ID, char* Nickname, char* GameServerIP, WORD GameServerPort, char* ChatServerIP, WORD ChatServerPort) { return true; }
    virtual bool Handle_EN_PACKET_SS_MONITOR_LOGIN(__int64 sessionId, int ServerNo) { return true; }
    virtual bool Handle_EN_PACKET_SS_MONITOR_DATA_UPDATE(__int64 sessionId, byte DataType, int DataValue, int TimeStamp) { return true; }
    CPacketRef Make_EN_PACKET_CS_LOGIN_REQ_LOGIN_Packet(T* netInterface, __int64 AccountNo, char* SessionKey);
    CPacketRef Make_EN_PACKET_CS_LOGIN_RES_LOGIN_Packet(T* netInterface, __int64 AccountNo, byte status, char* GameSessionKey, char* ChatSessionKey, char* ID, char* Nickname, char* GameServerIP, WORD GameServerPort, char* ChatServerIP, WORD ChatServerPort);
    CPacketRef Make_EN_PACKET_SS_MONITOR_LOGIN_Packet(T* netInterface, int ServerNo);
    CPacketRef Make_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(T* netInterface, byte DataType, int DataValue, int TimeStamp);
};

template<typename T>
bool PacketHandler<T>::PacketProc(T* netInterface, __int64 sessionId, CPacketRef& recvPktRef)
{
    WORD packetId;
    try {
        *(*recvPktRef) >> packetId;
    }
    catch (int e) {
        netInterface->OnSessionError(sessionId, -1, L"Failed unmarshalled packet type.", __FILE__, __LINE__);
        return false;
    }

    switch (packetId)
    {
    case EN_PACKET_CS_LOGIN_REQ_LOGIN:
    {
        __int64 AccountNo;
        char SessionKey[64];
        try {
            *(*recvPktRef) >> AccountNo;
            (*recvPktRef)->GetData(SessionKey, 64);
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_LOGIN_REQ_LOGIN, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_LOGIN_REQ_LOGIN(sessionId, AccountNo, SessionKey);
    }
    case EN_PACKET_CS_LOGIN_RES_LOGIN:
    {
        __int64 AccountNo;
        byte status;
        char GameSessionKey[64];
        char ChatSessionKey[64];
        char ID[40];
        char Nickname[40];
        char GameServerIP[32];
        WORD GameServerPort;
        char ChatServerIP[32];
        WORD ChatServerPort;
        try {
            *(*recvPktRef) >> AccountNo;
            *(*recvPktRef) >> status;
            (*recvPktRef)->GetData(GameSessionKey, 64);
            (*recvPktRef)->GetData(ChatSessionKey, 64);
            (*recvPktRef)->GetData(ID, 40);
            (*recvPktRef)->GetData(Nickname, 40);
            (*recvPktRef)->GetData(GameServerIP, 32);
            *(*recvPktRef) >> GameServerPort;
            (*recvPktRef)->GetData(ChatServerIP, 32);
            *(*recvPktRef) >> ChatServerPort;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_LOGIN_RES_LOGIN, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_LOGIN_RES_LOGIN(sessionId, AccountNo, status, GameSessionKey, ChatSessionKey, ID, Nickname, GameServerIP, GameServerPort, ChatServerIP, ChatServerPort);
    }
    case EN_PACKET_SS_MONITOR_LOGIN:
    {
        int ServerNo;
        try {
            *(*recvPktRef) >> ServerNo;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_SS_MONITOR_LOGIN, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
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
            netInterface->OnSessionError(sessionId, EN_PACKET_SS_MONITOR_DATA_UPDATE, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
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
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_LOGIN_REQ_LOGIN_Packet(T* netInterface, __int64 AccountNo, char* SessionKey)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_LOGIN_REQ_LOGIN;
    *(*pktRef) << AccountNo;
    (*pktRef)->PutData(SessionKey, 64);
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_LOGIN_RES_LOGIN_Packet(T* netInterface, __int64 AccountNo, byte status, char* GameSessionKey, char* ChatSessionKey, char* ID, char* Nickname, char* GameServerIP, WORD GameServerPort, char* ChatServerIP, WORD ChatServerPort)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_LOGIN_RES_LOGIN;
    *(*pktRef) << AccountNo;
    *(*pktRef) << status;
    (*pktRef)->PutData(GameSessionKey, 64);
    (*pktRef)->PutData(ChatSessionKey, 64);
    (*pktRef)->PutData(ID, 40);
    (*pktRef)->PutData(Nickname, 40);
    (*pktRef)->PutData(GameServerIP, 32);
    *(*pktRef) << GameServerPort;
    (*pktRef)->PutData(ChatServerIP, 32);
    *(*pktRef) << ChatServerPort;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_SS_MONITOR_LOGIN_Packet(T* netInterface, int ServerNo)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_SS_MONITOR_LOGIN;
    *(*pktRef) << ServerNo;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(T* netInterface, byte DataType, int DataValue, int TimeStamp)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_SS_MONITOR_DATA_UPDATE;
    *(*pktRef) << DataType;
    *(*pktRef) << DataValue;
    *(*pktRef) << TimeStamp;
    return pktRef;
}