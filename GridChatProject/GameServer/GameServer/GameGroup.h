#pragma once
#include <Network\Group.h>
#include <DB\DBConnection.h>

namespace onenewarm
{
	//-----------------------------------------------------------------
	// 화면 이동 범위.
	//-----------------------------------------------------------------
	#define dfRANGE_MOVE_TOP	0
	#define dfRANGE_MOVE_LEFT	0
	#define dfRANGE_MOVE_RIGHT	6400
	#define dfRANGE_MOVE_BOTTOM	6400
	
	//---------------------------------------------------------------
	// 공격범위.
	//---------------------------------------------------------------
	#define dfATTACK1_RANGE_X		80
	#define dfATTACK1_RANGE_Y		10
	
	//---------------------------------------------------------------
	// 공격 데미지.
	//---------------------------------------------------------------
	#define dfATTACK1_DAMAGE		1
	
	
	//-----------------------------------------------------------------
	// 캐릭터 이동 속도   // 25fps 기준 이동속도
	//-----------------------------------------------------------------
	#define dfSPEED_PLAYER_X	6	// 3   50fps
	#define dfSPEED_PLAYER_Y	4	// 2   50fps
	
	
	//-----------------------------------------------------------------
	// 이동 오류체크 범위
	//-----------------------------------------------------------------
	#define dfERROR_RANGE		50


	//----------------------------------------------------------------
	//SECT의 크기
	//----------------------------------------------------------------
	#define SECT_SIZE 160
	#define SECT_RANGE 1

	//(방향 디파인 값 8방향 사용)
	#define dfPACKET_MOVE_DIR_LL					0 // 왼쪽을 보고 있는 상태
	#define dfPACKET_MOVE_DIR_LU					1
	#define dfPACKET_MOVE_DIR_UU					2 // 위를 보고 있는 상태
	#define dfPACKET_MOVE_DIR_RU					3
	#define dfPACKET_MOVE_DIR_RR					4 // 오른쪽을 보고 있는 상태
	#define dfPACKET_MOVE_DIR_RD					5
	#define dfPACKET_MOVE_DIR_DD					6 // 아래를 보고 있는 상태
	#define dfPACKET_MOVE_DIR_LD					7

	struct SectorPos
	{
		int m_Col;
		int m_Row;
	};

	struct SECTOR_AROUND
	{
		int m_Count = 0;
		SectorPos m_Arounds[(1 + (2 * SECT_RANGE)) * (1 + (2 * SECT_RANGE))];
	};


	class Player
	{
	public:
		bool m_IsActive;
		__int64 m_SessionID;
		__int64 m_AccountNo;
		wchar_t m_Nickname[20]; // 0포함
		bool m_EraseFlag;

		SectorPos m_CurSector;
		SectorPos m_PrevSector;
		bool m_IsMove;
		BYTE m_Direction;
		BYTE m_MoveDirection;
		short m_X;
		short m_Y;

		Player() : m_IsActive(false)
		{
		}
	};

	SectorPos GetSector(short x, short y);


	void InitPlayer(Player* dest, __int64 sessionId, __int64 accountNo, const wchar_t* nickname, short x, short y);

	class GameGroup : public Group
	{
	public:
		void	OnMessage(CPacketRef pktRef, __int64 sessionId);
		void	OnSessionRelease(__int64 sessionId);
		void	OnEnter(__int64 sessionId, __int64 enterKey);
		void	OnLeave(__int64 sessionId);
		void	OnUpdate();
		void	OnAttach();
		void	OnDetach();

		GameGroup();
		~GameGroup();
		void	SendSectorAround(Player* player, CPacket* pkt);
		void	SendSectorAround(SECTOR_AROUND& around, CPacket* pkt);

	public:
		void	HandleReqMoveStart(CPacket* pkt, __int64 sessionId);
		void	HandleReqMoveStop(CPacket* pkt, __int64 sessionId);


		void MoveSect(__int64 sessionId, SectorPos sect);
	public:
		SECTOR_AROUND	m_SectorArounds[dfRANGE_MOVE_BOTTOM / SECT_SIZE][dfRANGE_MOVE_RIGHT / SECT_SIZE];
		Player* m_Players;
		list<__int64> m_Sectors[dfRANGE_MOVE_BOTTOM / SECT_SIZE][dfRANGE_MOVE_RIGHT / SECT_SIZE];
		WORD m_PlayerCount;

		DBConnection* m_DBConnection;
	};
}



