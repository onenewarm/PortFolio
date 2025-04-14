#pragma once
#include <Network\CPacket.h>
#include "CommonProtocol.h"

using namespace onenewarm;

template<typename T>
class PacketHandler
{
public:
    bool PacketProc(T* netInterface, __int64 sessionId, CPacketRef& recvPktRef);
    virtual bool Handle_EN_PACKET_CS_GAME_REQ_LOGIN(__int64 sessionId, __int64 AccountNo, char* SessionKey) { return true; }
    virtual bool Handle_EN_PACKET_CS_GAME_RES_LOGIN(__int64 sessionId, BYTE Status) { return true; }
    virtual bool Handle_EN_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER(__int64 sessionId, char* Nickname, short x, short y, char direction) { return true; }
    virtual bool Handle_EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER(__int64 sessionId, __int64 AccountNo, char* Nickname, short x, short y, char direction) { return true; }
    virtual bool Handle_EN_PACKET_CS_GAME_RES_DELETE_CHARACTER(__int64 sessionId, __int64 accountNo) { return true; }
    virtual bool Handle_EN_PACKET_CS_GAME_REQ_MOVE_START(__int64 sessionId, BYTE direction) { return true; }
    virtual bool Handle_EN_PACKET_CS_GAME_REQ_MOVE_STOP(__int64 sessionId, short x, short y) { return true; }
    virtual bool Handle_EN_PACKET_CS_GAME_RES_MOVE_START(__int64 sessionId, __int64 AccountNo, byte Direction) { return true; }
    virtual bool Handle_EN_PACKET_CS_GAME_RES_MOVE_STOP(__int64 sessionId, __int64 AccountNo, short x, short y) { return true; }
    virtual bool Handle_EN_PACKET_CS_GAME_RES_SYNC(__int64 sessionId, short x, short y) { return true; }
    virtual bool Handle_EN_PACKET_SS_MONITOR_LOGIN(__int64 sessionId, int ServerNo) { return true; }
    virtual bool Handle_EN_PACKET_SS_MONITOR_DATA_UPDATE(__int64 sessionId, BYTE DataType, int DataValue, int TimeStamp) { return true; }
    CPacketRef Make_EN_PACKET_CS_GAME_REQ_LOGIN_Packet(T* netInterface, __int64 AccountNo, char* SessionKey);
    CPacketRef Make_EN_PACKET_CS_GAME_RES_LOGIN_Packet(T* netInterface, BYTE Status);
    CPacketRef Make_EN_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER_Packet(T* netInterface, char* Nickname, short x, short y, char direction);
    CPacketRef Make_EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER_Packet(T* netInterface, __int64 AccountNo, char* Nickname, short x, short y, char direction);
    CPacketRef Make_EN_PACKET_CS_GAME_RES_DELETE_CHARACTER_Packet(T* netInterface, __int64 accountNo);
    CPacketRef Make_EN_PACKET_CS_GAME_REQ_MOVE_START_Packet(T* netInterface, BYTE direction);
    CPacketRef Make_EN_PACKET_CS_GAME_REQ_MOVE_STOP_Packet(T* netInterface, short x, short y);
    CPacketRef Make_EN_PACKET_CS_GAME_RES_MOVE_START_Packet(T* netInterface, __int64 AccountNo, byte Direction);
    CPacketRef Make_EN_PACKET_CS_GAME_RES_MOVE_STOP_Packet(T* netInterface, __int64 AccountNo, short x, short y);
    CPacketRef Make_EN_PACKET_CS_GAME_RES_SYNC_Packet(T* netInterface, short x, short y);
    CPacketRef Make_EN_PACKET_SS_MONITOR_LOGIN_Packet(T* netInterface, int ServerNo);
    CPacketRef Make_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(T* netInterface, BYTE DataType, int DataValue, int TimeStamp);
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
    case EN_PACKET_CS_GAME_REQ_LOGIN:
    {
        __int64 AccountNo;
        char SessionKey[64];
        try {
            *(*recvPktRef) >> AccountNo;
            (*recvPktRef)->GetData(SessionKey, 64);
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_REQ_LOGIN, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_REQ_LOGIN(sessionId, AccountNo, SessionKey);
    }
    case EN_PACKET_CS_GAME_RES_LOGIN:
    {
        BYTE Status;
        try {
            *(*recvPktRef) >> Status;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_RES_LOGIN, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_RES_LOGIN(sessionId, Status);
    }
    case EN_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER:
    {
        char Nickname[40];
        short x;
        short y;
        char direction;
        try {
            (*recvPktRef)->GetData(Nickname, 40);
            *(*recvPktRef) >> x;
            *(*recvPktRef) >> y;
            *(*recvPktRef) >> direction;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER(sessionId, Nickname, x, y, direction);
    }
    case EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER:
    {
        __int64 AccountNo;
        char Nickname[40];
        short x;
        short y;
        char direction;
        try {
            *(*recvPktRef) >> AccountNo;
            (*recvPktRef)->GetData(Nickname, 40);
            *(*recvPktRef) >> x;
            *(*recvPktRef) >> y;
            *(*recvPktRef) >> direction;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER(sessionId, AccountNo, Nickname, x, y, direction);
    }
    case EN_PACKET_CS_GAME_RES_DELETE_CHARACTER:
    {
        __int64 accountNo;
        try {
            *(*recvPktRef) >> accountNo;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_RES_DELETE_CHARACTER, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_RES_DELETE_CHARACTER(sessionId, accountNo);
    }
    case EN_PACKET_CS_GAME_REQ_MOVE_START:
    {
        BYTE direction;
        try {
            *(*recvPktRef) >> direction;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_REQ_MOVE_START, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_REQ_MOVE_START(sessionId, direction);
    }
    case EN_PACKET_CS_GAME_REQ_MOVE_STOP:
    {
        short x;
        short y;
        try {
            *(*recvPktRef) >> x;
            *(*recvPktRef) >> y;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_REQ_MOVE_STOP, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_REQ_MOVE_STOP(sessionId, x, y);
    }
    case EN_PACKET_CS_GAME_RES_MOVE_START:
    {
        __int64 AccountNo;
        byte Direction;
        try {
            *(*recvPktRef) >> AccountNo;
            *(*recvPktRef) >> Direction;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_RES_MOVE_START, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_RES_MOVE_START(sessionId, AccountNo, Direction);
    }
    case EN_PACKET_CS_GAME_RES_MOVE_STOP:
    {
        __int64 AccountNo;
        short x;
        short y;
        try {
            *(*recvPktRef) >> AccountNo;
            *(*recvPktRef) >> x;
            *(*recvPktRef) >> y;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_RES_MOVE_STOP, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_RES_MOVE_STOP(sessionId, AccountNo, x, y);
    }
    case EN_PACKET_CS_GAME_RES_SYNC:
    {
        short x;
        short y;
        try {
            *(*recvPktRef) >> x;
            *(*recvPktRef) >> y;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_GAME_RES_SYNC, L"Failed unmarshalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_GAME_RES_SYNC(sessionId, x, y);
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
        BYTE DataType;
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
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_REQ_LOGIN_Packet(T* netInterface, __int64 AccountNo, char* SessionKey)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_REQ_LOGIN;
    *(*pktRef) << AccountNo;
    (*pktRef)->PutData(SessionKey, 64);
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_RES_LOGIN_Packet(T* netInterface, BYTE Status)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_RES_LOGIN;
    *(*pktRef) << Status;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER_Packet(T* netInterface, char* Nickname, short x, short y, char direction)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER;
    (*pktRef)->PutData(Nickname, 40);
    *(*pktRef) << x;
    *(*pktRef) << y;
    *(*pktRef) << direction;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER_Packet(T* netInterface, __int64 AccountNo, char* Nickname, short x, short y, char direction)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER;
    *(*pktRef) << AccountNo;
    (*pktRef)->PutData(Nickname, 40);
    *(*pktRef) << x;
    *(*pktRef) << y;
    *(*pktRef) << direction;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_RES_DELETE_CHARACTER_Packet(T* netInterface, __int64 accountNo)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_RES_DELETE_CHARACTER;
    *(*pktRef) << accountNo;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_REQ_MOVE_START_Packet(T* netInterface, BYTE direction)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_REQ_MOVE_START;
    *(*pktRef) << direction;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_REQ_MOVE_STOP_Packet(T* netInterface, short x, short y)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_REQ_MOVE_STOP;
    *(*pktRef) << x;
    *(*pktRef) << y;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_RES_MOVE_START_Packet(T* netInterface, __int64 AccountNo, byte Direction)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_RES_MOVE_START;
    *(*pktRef) << AccountNo;
    *(*pktRef) << Direction;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_RES_MOVE_STOP_Packet(T* netInterface, __int64 AccountNo, short x, short y)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_RES_MOVE_STOP;
    *(*pktRef) << AccountNo;
    *(*pktRef) << x;
    *(*pktRef) << y;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_GAME_RES_SYNC_Packet(T* netInterface, short x, short y)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_GAME_RES_SYNC;
    *(*pktRef) << x;
    *(*pktRef) << y;
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
CPacketRef PacketHandler<T>::Make_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(T* netInterface, BYTE DataType, int DataValue, int TimeStamp)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_SS_MONITOR_DATA_UPDATE;
    *(*pktRef) << DataType;
    *(*pktRef) << DataValue;
    *(*pktRef) << TimeStamp;
    return pktRef;
}