#pragma once
#include <Windows.h>

enum : WORD
{
    EN_PACKET_CS_CHAT_REQ_LOGIN = 1,
    EN_PACKET_CS_CHAT_RES_LOGIN = 2,
    EN_PACKET_CS_CHAT_REQ_SECTOR_MOVE = 3,
    EN_PACKET_CS_CHAT_RES_SECTOR_MOVE = 4,
    EN_PACKET_CS_CHAT_REQ_MESSAGE = 5,
    EN_PACKET_CS_CHAT_RES_MESSAGE = 6,
    EN_PACKET_CS_CHAT_REQ_HEARTBEAT = 7,
    EN_PACKET_SS_MONITOR_LOGIN = 20001,
    EN_PACKET_SS_MONITOR_DATA_UPDATE = 20002,
};

enum : BYTE
{
    dfCHAT_LOGIN_ERR_SESSIONKEY = 0,		// �α��� ����Ű ����
    dfCHAT_LOGIN_OK = 1,		// �α��� ����
};


enum en_PACKET_SS_MONITOR_DATA_UPDATE
{
    dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU = 31,		// ä�ü��� ChatServer CPU ����
    dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM = 32,		// ä�ü��� ChatServer �޸� ��� MByte
    dfMONITOR_DATA_TYPE_CHAT_SESSION = 33,		// ä�ü��� ���� �� (���ؼ� ��)
    dfMONITOR_DATA_TYPE_CHAT_PLAYER = 34,		// ä�ü��� �������� ����� �� (���� ������)
    dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS = 35,		// ä�ü��� UPDATE ������ �ʴ� �ʸ� Ƚ��
    dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL = 36,		// ä�ü��� ��ŶǮ ��뷮
    dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL = 37,		// ä�ü��� UPDATE MSG Ǯ ��뷮
};