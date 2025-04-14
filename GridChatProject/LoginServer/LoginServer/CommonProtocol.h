#pragma once
#include <Windows.h>

enum : WORD
{
    EN_PACKET_CS_LOGIN_REQ_LOGIN = 101,
    EN_PACKET_CS_LOGIN_RES_LOGIN = 102,
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


enum en_PACKET_CS_GAME_RES_LOGIN
{
	dfGAME_LOGIN_FAIL = 0,		// ����Ű ���� �Ǵ� Account ���̺���� ����
	dfGAME_LOGIN_OK = 1,		// ����
	dfGAME_LOGIN_NOCHARACTER = 2,		// ���� / ĳ���� ���� > ĳ���� ����ȭ������ ��ȯ. 
	dfGAME_LOGIN_VERSION_MISS = 3,		// ����,Ŭ�� ���� �ٸ�
};



enum en_PACKET_SS_LOGINSERVER_LOGIN
{
	dfSERVER_TYPE_GAME = 1,
	dfSERVER_TYPE_CHAT = 2,
	dfSERVER_TYPE_MONITOR = 3,
};

enum en_PACKET_SS_HEARTBEAT
{
	dfTHREAD_TYPE_WORKER = 1,
	dfTHREAD_TYPE_DB = 2,
	dfTHREAD_TYPE_GAME = 3,
};

enum en_PACKET_SS_MONITOR_DATA_UPDATE
{
	dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN = 1,		// �α��μ��� ���࿩�� ON / OFF
	dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU = 2,		// �α��μ��� CPU ����
	dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM = 3,		// �α��μ��� �޸� ��� MByte
	dfMONITOR_DATA_TYPE_LOGIN_SESSION = 4,		// �α��μ��� ���� �� (���ؼ� ��)
	dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS = 5,		// �α��μ��� ���� ó�� �ʴ� Ƚ��
	dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL = 6,		// �α��μ��� ��ŶǮ ��뷮
};


enum en_PACKET_CS_MONITOR_TOOL_RES_LOGIN
{
	dfMONITOR_TOOL_LOGIN_OK = 1,		// �α��� ����
	dfMONITOR_TOOL_LOGIN_ERR_NOSERVER = 2,		// �����̸� ���� (��Ī�̽�)
	dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY = 3,		// �α��� ����Ű ����
};
