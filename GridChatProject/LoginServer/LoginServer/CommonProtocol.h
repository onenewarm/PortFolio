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
	dfLOGIN_STATUS_NONE = -1,		// 미인증상태
	dfLOGIN_STATUS_FAIL = 0,		// 세션오류
	dfLOGIN_STATUS_OK = 1,		// 성공
	dfLOGIN_STATUS_GAME = 2,		// 게임중
	dfLOGIN_STATUS_ACCOUNT_MISS = 3,		// account 테이블에 AccountNo 없음
	dfLOGIN_STATUS_SESSION_MISS = 4,		// Session 테이블에 AccountNo 없음
	dfLOGIN_STATUS_STATUS_MISS = 5,		// Status 테이블에 AccountNo 없음
	dfLOGIN_STATUS_NOSERVER = 6,		// 서비스중인 서버가 없음.
};


enum en_PACKET_CS_GAME_RES_LOGIN
{
	dfGAME_LOGIN_FAIL = 0,		// 세션키 오류 또는 Account 데이블상의 오류
	dfGAME_LOGIN_OK = 1,		// 성공
	dfGAME_LOGIN_NOCHARACTER = 2,		// 성공 / 캐릭터 없음 > 캐릭터 선택화면으로 전환. 
	dfGAME_LOGIN_VERSION_MISS = 3,		// 서버,클라 버전 다름
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
	dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN = 1,		// 로그인서버 실행여부 ON / OFF
	dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU = 2,		// 로그인서버 CPU 사용률
	dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM = 3,		// 로그인서버 메모리 사용 MByte
	dfMONITOR_DATA_TYPE_LOGIN_SESSION = 4,		// 로그인서버 세션 수 (컨넥션 수)
	dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS = 5,		// 로그인서버 인증 처리 초당 횟수
	dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL = 6,		// 로그인서버 패킷풀 사용량
};


enum en_PACKET_CS_MONITOR_TOOL_RES_LOGIN
{
	dfMONITOR_TOOL_LOGIN_OK = 1,		// 로그인 성공
	dfMONITOR_TOOL_LOGIN_ERR_NOSERVER = 2,		// 서버이름 오류 (매칭미스)
	dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY = 3,		// 로그인 세션키 오류
};
