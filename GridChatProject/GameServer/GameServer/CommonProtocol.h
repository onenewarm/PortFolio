#pragma once
#include <Windows.h>

enum : WORD
{
    EN_PACKET_CS_GAME_REQ_LOGIN = 1001,
    EN_PACKET_CS_GAME_RES_LOGIN = 1002,
    EN_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER = 1003,
    EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER = 1004,
    EN_PACKET_CS_GAME_RES_DELETE_CHARACTER = 1005,
    EN_PACKET_CS_GAME_REQ_MOVE_START = 1006,
    EN_PACKET_CS_GAME_REQ_MOVE_STOP = 1007,
    EN_PACKET_CS_GAME_RES_MOVE_START = 1008,
    EN_PACKET_CS_GAME_RES_MOVE_STOP = 1009,
    EN_PACKET_CS_GAME_RES_SYNC = 1010,
    EN_PACKET_SS_MONITOR_LOGIN = 20001,
    EN_PACKET_SS_MONITOR_DATA_UPDATE = 20002,
};

enum en_PACKET_SS_MONITOR_DATA_UPDATE
{
	dfMONITOR_DATA_TYPE_GAME_SERVER_RUN = 10,		// GameServer 실행 여부 ON / OFF
	dfMONITOR_DATA_TYPE_GAME_SERVER_CPU = 11,		// GameServer CPU 사용률
	dfMONITOR_DATA_TYPE_GAME_SERVER_MEM = 12,		// GameServer 메모리 사용 MByte
	dfMONITOR_DATA_TYPE_GAME_SESSION = 13,		// 게임서버 세션 수 (컨넥션 수)
	dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER = 14,		// 게임서버 AUTH MODE 플레이어 수
	dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER = 15,		// 게임서버 GAME MODE 플레이어 수
	dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS = 16,		// 게임서버 Accept 처리 초당 횟수
	dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS = 17,		// 게임서버 패킷처리 초당 횟수
	dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS = 18,		// 게임서버 패킷 보내기 초당 완료 횟수
	dfMONITOR_DATA_TYPE_GAME_DB_WRITE_TPS = 19,		// 게임서버 DB 저장 메시지 초당 처리 횟수
	dfMONITOR_DATA_TYPE_GAME_DB_WRITE_MSG = 20,		// 게임서버 DB 저장 메시지 큐 개수 (남은 수)
	dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS = 21,		// 게임서버 AUTH 스레드 초당 프레임 수 (루프 수)
	dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS = 22,		// 게임서버 GAME 스레드 초당 프레임 수 (루프 수)
	dfMONITOR_DATA_TYPE_GAME_PACKET_POOL = 23,		// 게임서버 패킷풀 사용량
};