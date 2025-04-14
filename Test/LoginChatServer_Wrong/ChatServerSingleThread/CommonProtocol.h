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

enum en_PACKET_CS_LOGIN_RES_LOGIN
{
	dfLOGIN_STATUS_NONE = -1,		// ����������
	dfLOGIN_STATUS_FAIL = 0,		// ���ǿ���
	dfLOGIN_STATUS_OK = 1,		// ����
	dfLOGIN_STATUS_GAME = 2,		// ������
	dfLOGIN_STATUS_ACCOUNT_MISS = 3,		// account ���̺� AccountNo ����
	dfLOGIN_STATUS_SESSION_MISS = 4,		// Session ���̺� AccountNo ����
	dfLOGIN_STATUS_STATUS_MISS = 5,		// Status ���̺� AccountNo ����
	dfLOGIN_STATUS_NOSERVER = 6,		// �������� ������ ����.
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
