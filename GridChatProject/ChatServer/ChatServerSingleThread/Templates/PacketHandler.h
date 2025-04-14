#pragma once
#include <Network\CPacket.h>
#include "CommonProtocol.h"

using namespace onenewarm;

template<typename T>
class {{ output }}
{
public:
    bool PacketProc(T* netInterface, __int64 sessionId, CPacketRef recvPktRef);

    {%- for protocol in parser.m_Protocols %}
    virtual bool Handle_{{ protocol.packet_name.upper() }}(__int64 sessionId{%- for field in protocol.fields %}, {{ "char*" if "Array" in field[0] else field[0] }} {{ field[1] }} {%- endfor %}) { return true; }
    {%- endfor%}

    {%- for protocol in parser.m_Protocols %}
    CPacketRef Make_{{ protocol.packet_name.upper() }}_Packet(T* netInterface{%- for field in protocol.fields %}, {{ "char*" if "Array" in field[0] else field[0] }} {{ field[1] }} {%- endfor %});
    {%- endfor %}
};

template<typename T>
bool {{output}}<T>::PacketProc(T* netInterface, __int64 sessionId, CPacketRef recvPktRef)
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
    {%- for proto in parser.m_Protocols %}
    case {{ proto.packet_name.upper() }}:
    {
        {%- for field in proto.fields %}
            {%- if "Array" in field[0] %}
                {%- if field[0].find(',') != -1 %}
        char {{ field[1] }}[{{ field[0].split(',')[1].replace(']', '').strip() }}];
                {%- else %}
        char {{ field[1] }}[{{ field[0].split('[')[1].replace(']', '').strip() }}];
                {%- endif %}
            {%- else %}
        {{ field[0] }} {{ field[1] }};
            {%- endif %}
        {%- endfor %}
        try {
            {%- for field in proto.fields %}
                {%- if "Array" in field[0] %}
                    {%- if field[0].find(',') != -1 %}
            (*recvPktRef)->GetData({{ field[1] }}, {{ field[0].split('[')[1].split(',')[0].replace(']', '').strip()}});
                    {%- else %}
            (*recvPktRef)->GetData({{ field[1] }}, {{ field[0].split('[')[1].replace(']', '').strip() }});
                    {%- endif %}
                {%- else %}
            *(*recvPktRef) >> {{ field[1] }};
                {%- endif %}
            {%- endfor %}
        }
        catch (int e) {
            netInterface->OnSessionError(sessionId, {{ proto.packet_name.upper() }}, L"Failed unmashalled packet data.", __FILE__, __LINE__);
            return false;
        }

        return Handle_{{ proto.packet_name.upper() }}(sessionId{%- for field in proto.fields %}, {{ field[1] }} {%- endfor %});
    }
    {%- endfor %}
    default:
    {
        netInterface->OnSessionError(sessionId, -1, L"Recved unvalid packet type.", __FILE__, __LINE__);
        return false;
    }
    }

}


{%- for proto in parser.m_Protocols %}
template<typename T>
CPacketRef {{ output }}<T>::Make_{{ proto.packet_name.upper() }}_Packet(T* netInterface{%- for field in proto.fields %}, {{ "char*" if "Array" in field[0] else field[0] }} {{ field[1] }} {%- endfor %})
{
    CPacketRef pktRef = netInterface->AllocPacket();
    *(*pktRef) << {{ proto.packet_name.upper() }};
    {%- for field in proto.fields %}
        {%- if "Array" in field[0] %}
            {%- if field[0].find(',') != -1 %}
    (*pktRef)->PutData({{ field[1] }}, {{ field[0].split('[')[1].split(',')[0].replace(']', '').strip() }});
            {%- else %}
    (*pktRef)->PutData({{ field[1] }}, {{ field[0].split('[')[1].replace(']', '').strip() }});
            {%- endif %}
        {%- else %}
    *(*pktRef) << {{ field[1] }};
        {%- endif %}
    {%- endfor %}
    return pktRef;
}
{%- endfor %}