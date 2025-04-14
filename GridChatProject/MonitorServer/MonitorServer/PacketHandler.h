#pragma once
#include <Network\CPacket.h>
#include "CommonProtocol.h"

using namespace onenewarm;

template<typename T>
class PacketHandler
{
public:
    bool PacketProc(T* netInterface, __int64 sessionId, CPacketRef recvPktRef);
    virtual bool Handle_EN_PACKET_SS_MONITOR_LOGIN(__int64 sessionId, int ServerNo) { return true; }
    virtual bool Handle_EN_PACKET_SS_MONITOR_DATA_UPDATE(__int64 sessionId, byte DataType, int DataValue, int TimeStamp) { return true; }
    virtual bool Handle_EN_PACKET_CS_MONITOR_TOOL_REQ_LOGIN(__int64 sessionId, char* LoginSessionKey) { return true; }
    virtual bool Handle_EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN(__int64 sessionId, byte Status) { return true; }
    virtual bool Handle_EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE(__int64 sessionId, byte ServerNo, byte DataType, int DataValue, int TimeStamp) { return true; }
    CPacketRef Make_EN_PACKET_SS_MONITOR_LOGIN_Packet(T* netInterface, int ServerNo);
    CPacketRef Make_EN_PACKET_SS_MONITOR_DATA_UPDATE_Packet(T* netInterface, byte DataType, int DataValue, int TimeStamp);
    CPacketRef Make_EN_PACKET_CS_MONITOR_TOOL_REQ_LOGIN_Packet(T* netInterface, char* LoginSessionKey);
    CPacketRef Make_EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN_Packet(T* netInterface, byte Status);
    CPacketRef Make_EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE_Packet(T* netInterface, byte ServerNo, byte DataType, int DataValue, int TimeStamp);
};

template<typename T>
bool PacketHandler<T>::PacketProc(T* netInterface, __int64 sessionId, CPacketRef recvPktRef)
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
    case EN_PACKET_CS_MONITOR_TOOL_REQ_LOGIN:
    {
        char LoginSessionKey[32];
        try {
            (*recvPktRef)->GetData(LoginSessionKey, 32);
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_MONITOR_TOOL_REQ_LOGIN, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_MONITOR_TOOL_REQ_LOGIN(sessionId, LoginSessionKey);
    }
    case EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN:
    {
        byte Status;
        try {
            *(*recvPktRef) >> Status;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN(sessionId, Status);
    }
    case EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE:
    {
        byte ServerNo;
        byte DataType;
        int DataValue;
        int TimeStamp;
        try {
            *(*recvPktRef) >> ServerNo;
            *(*recvPktRef) >> DataType;
            *(*recvPktRef) >> DataValue;
            *(*recvPktRef) >> TimeStamp;
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE(sessionId, ServerNo, DataType, DataValue, TimeStamp);
    }
    default:
    {
        netInterface->OnSessionError(sessionId, -1, L"Recved unvalid packet type.", __FILE__, __LINE__);
        return false;
    }
    }

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
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_MONITOR_TOOL_REQ_LOGIN_Packet(T* netInterface, char* LoginSessionKey)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_MONITOR_TOOL_REQ_LOGIN;
    (*pktRef)->PutData(LoginSessionKey, 32);
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN_Packet(T* netInterface, byte Status)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN;
    *(*pktRef) << Status;
    return pktRef;
}
template<typename T>
CPacketRef PacketHandler<T>::Make_EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE_Packet(T* netInterface, byte ServerNo, byte DataType, int DataValue, int TimeStamp)
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << EN_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;
    *(*pktRef) << ServerNo;
    *(*pktRef) << DataType;
    *(*pktRef) << DataValue;
    *(*pktRef) << TimeStamp;
    return pktRef;
}