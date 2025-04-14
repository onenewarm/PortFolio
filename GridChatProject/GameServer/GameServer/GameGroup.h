#pragma once
#include <Network\Group.h>
#include <DB\DBConnection.h>

namespace onenewarm
{
	//-----------------------------------------------------------------
	// ȭ�� �̵� ����.
	//-----------------------------------------------------------------
	#define dfRANGE_MOVE_TOP	0
	#define dfRANGE_MOVE_LEFT	0
	#define dfRANGE_MOVE_RIGHT	6400
	#define dfRANGE_MOVE_BOTTOM	6400
	
	//---------------------------------------------------------------
	// ���ݹ���.
	//---------------------------------------------------------------
	#define dfATTACK1_RANGE_X		80
	#define dfATTACK1_RANGE_Y		10
	
	//---------------------------------------------------------------
	// ���� ������.
	//---------------------------------------------------------------
	#define dfATTACK1_DAMAGE		1
	
	
	//-----------------------------------------------------------------
	// ĳ���� �̵� �ӵ�   // 25fps ���� �̵��ӵ�
	//-----------------------------------------------------------------
	#define dfSPEED_PLAYER_X	6	// 3   50fps
	#define dfSPEED_PLAYER_Y	4	// 2   50fps
	
	
	//-----------------------------------------------------------------
	// �̵� ����üũ ����
	//-----------------------------------------------------------------
	#define dfERROR_RANGE		50


	//----------------------------------------------------------------
	//SECT�� ũ��
	//----------------------------------------------------------------
	#define SECT_SIZE 160
	#define SECT_RANGE 1

	//(���� ������ �� 8���� ���)
	#define dfPACKET_MOVE_DIR_LL					0 // ������ ���� �ִ� ����
	#define dfPACKET_MOVE_DIR_LU					1
	#define dfPACKET_MOVE_DIR_UU					2 // ���� ���� �ִ� ����
	#define dfPACKET_MOVE_DIR_RU					3
	#define dfPACKET_MOVE_DIR_RR					4 // �������� ���� �ִ� ����
	#define dfPACKET_MOVE_DIR_RD					5
	#define dfPACKET_MOVE_DIR_DD					6 // �Ʒ��� ���� �ִ� ����
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
		wchar_t m_Nickname[20]; // 0����
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



