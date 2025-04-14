#include "GameServer.h"
#include "GameGroup.h"
#include "CommonProtocol.h"


using namespace onenewarm;

int dx[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
int dy[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };

void onenewarm::GameGroup::OnMessage(CPacketRef pktRef, __int64 sessionId)
{
	GameServer* server = (GameServer*)m_Server;
	server->PacketProc(server, sessionId, pktRef);
}

void onenewarm::GameGroup::OnSessionRelease(__int64 sessionId)
{
	GameServer* server = (GameServer*)m_Server;

	// 세션의 연결이 끊어진 상황. Player를 정리해주어야 한다.
	Player* player = &m_Players[(WORD)sessionId];
	SectorPos sectPos = player->m_CurSector;
	
	auto sectIter = m_Sectors[sectPos.m_Row][sectPos.m_Col].begin();
	auto endIter = m_Sectors[sectPos.m_Row][sectPos.m_Col].end();

	for (; sectIter != endIter; ++sectIter)
	{
		if (*sectIter == player->m_SessionID)
		{
			m_Sectors[sectPos.m_Row][sectPos.m_Col].erase(sectIter);
			break;
		}
			
	}

	player->m_IsActive = false;
	player->m_IsMove = false;

	CPacketRef deletePkt = server->Make_EN_PACKET_CS_GAME_RES_DELETE_CHARACTER_Packet(server, player->m_AccountNo);
	SendSectorAround(player, *deletePkt);
	m_PlayerCount--;
}

void onenewarm::GameGroup::OnEnter(__int64 sessionId, __int64 enterKey)
{
	GameServer* server = (GameServer*)m_Server;

	// 여기에 들어온 경우는 Auth그룹에서 Auth절차를 통과하고 들어온 유저
	// 즉, 처음으로 Player를 생성한다.

	// Player 생성
	short x = rand() % dfRANGE_MOVE_RIGHT;
	short y = rand() % dfRANGE_MOVE_BOTTOM;
	Player* enterPlayer = &(m_Players[(WORD)sessionId]);
	wchar_t nickname[20];


	///////////////////////////////////////////////////////
	//
	// DB에서 accountNo로 nickname 찾아오기
	//
	///////////////////////////////////////////////////////

	m_DBConnection->SendFormattedQuery("select usernick from account where accountno = %d", enterKey);

	MYSQL_ROW queryResRow;
	if (m_DBConnection->FetchRow(&queryResRow) == true)
	{
		m_DBConnection->FreeRes();
	}
	else
	{
		server->Disconnect(sessionId);
		return;
	}

	ConvertAtoW(nickname, queryResRow[0]);
	
	InitPlayer(enterPlayer, sessionId, enterKey, nickname, x, y);
	m_Sectors[enterPlayer->m_CurSector.m_Row][enterPlayer->m_CurSector.m_Col].push_back(sessionId);

	// 클라이언트에게 My캐릭터 생성 패킷을 보낸다.
	CPacketRef mycharPacket = server->Make_EN_PACKET_CS_GAME_RES_CREATE_MY_CHARACTER_Packet(server, (char*)nickname, x, y, dfPACKET_MOVE_DIR_DD);
	server->SendPacket(sessionId, *mycharPacket);

	// 섹터 내에 다른 유저들에게 Player 생성을 알린다.
	CPacketRef onEnterCreatePacket = server->Make_EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER_Packet(server, enterKey, (char*)enterPlayer->m_Nickname, enterPlayer->m_X, enterPlayer->m_Y, enterPlayer->m_Direction);
	SendSectorAround(enterPlayer, *onEnterCreatePacket);


	// 클라이언트에게 인접한 9개의 섹터에 존재하는 Other캐릭터 생성 패킷을 보낸다.
	SECTOR_AROUND* sectorAround = &(m_SectorArounds[enterPlayer->m_CurSector.m_Row][enterPlayer->m_CurSector.m_Col]);
	for (int cnt = 0; cnt < sectorAround->m_Count; ++cnt)
	{
		SectorPos sectPos = sectorAround->m_Arounds[cnt];
		int row = sectPos.m_Row; int col = sectPos.m_Col;

		for (auto iter = m_Sectors[row][col].begin(); iter != m_Sectors[row][col].end(); ++iter) {
			__int64 curPlayerId = *iter;
			WORD playerIdx = (WORD)curPlayerId;
			Player* player = &m_Players[playerIdx];
			
			if (player == enterPlayer) continue;

			CPacketRef createOtherPkt = server->Make_EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER_Packet(server, player->m_AccountNo, (char*)player->m_Nickname, player->m_X, player->m_Y,player->m_Direction);
			m_Server->SendPacket(sessionId, *createOtherPkt);

			if (player->m_IsMove == true)
			{
				CPacketRef moveStartPkt = server->Make_EN_PACKET_CS_GAME_RES_MOVE_START_Packet(server, player->m_AccountNo, player->m_Direction);
				m_Server->SendPacket(sessionId, *moveStartPkt);
			}
		}
	}

	m_PlayerCount++;
}

void onenewarm::GameGroup::OnLeave(__int64 sessionId)
{
}

void onenewarm::GameGroup::OnUpdate()
{
	// Player의 이동 처리
	int maxPlayerSize = m_Server->GetMaxSessions();
	for (int cnt = 0; cnt < maxPlayerSize; ++cnt)
	{
		if (m_Players[cnt].m_IsActive == true && m_Players[cnt].m_IsMove == true)
		{
			Player* player = &m_Players[cnt];
			short ny = player->m_Y + (dy[player->m_MoveDirection] * dfSPEED_PLAYER_Y);
			short nx = player->m_X + (dx[player->m_MoveDirection] * dfSPEED_PLAYER_X);

			if (ny < dfRANGE_MOVE_TOP) player->m_Y = dfRANGE_MOVE_TOP;
			else if (ny >= dfRANGE_MOVE_BOTTOM) player->m_Y = dfRANGE_MOVE_BOTTOM - 1;
			else player->m_Y = ny;

			if (nx < dfRANGE_MOVE_LEFT) player->m_X = dfRANGE_MOVE_LEFT;
			else if (nx >= dfRANGE_MOVE_RIGHT) player->m_X = dfRANGE_MOVE_RIGHT - 1;
			else player->m_X = nx;

			SectorPos curSect = GetSector(player->m_X, player->m_Y);

			if ((player->m_CurSector.m_Row != curSect.m_Row) || (player->m_CurSector.m_Col != curSect.m_Col))
				MoveSect(player->m_SessionID, curSect);
		}
	}
	
}

void onenewarm::GameGroup::OnAttach()
{
	int tmp;
	__int64 randSeed = (__int64) & tmp;

	srand(randSeed);
	WORD playerSize = m_Server->GetMaxSessions();
	m_Players = new Player[playerSize];
}

void onenewarm::GameGroup::OnDetach()
{
	
}

onenewarm::GameGroup::GameGroup() : m_Players(NULL), m_PlayerCount(0)
{
	m_DBConnection = new DBConnection("127.0.0.1", 3306, "root", "password", "accountdb", 1000);

	// SectorAround 미리 구해놓기
	for (int row = 0; row < dfRANGE_MOVE_BOTTOM / SECT_SIZE; ++row)
	{
		for (int col = 0; col < dfRANGE_MOVE_RIGHT / SECT_SIZE; ++col)
		{
			int leftTopY = row - SECT_RANGE; int leftTopX = col - SECT_RANGE;
			int rightBotY = row + SECT_RANGE; int rightBotX = col + SECT_RANGE;

			if (leftTopY < 0) leftTopY = 0;
			if (leftTopX < 0) leftTopX = 0;
			if (rightBotY >= dfRANGE_MOVE_BOTTOM / SECT_SIZE) rightBotY = dfRANGE_MOVE_BOTTOM / SECT_SIZE - 1;
			if (rightBotX >= dfRANGE_MOVE_RIGHT / SECT_SIZE) rightBotX = dfRANGE_MOVE_RIGHT / SECT_SIZE - 1;

			for (int y = leftTopY; y <= rightBotY; ++y)
			{
				for (int x = leftTopX; x <= rightBotX; ++x)
				{
					m_SectorArounds[row][col].m_Arounds[m_SectorArounds[row][col].m_Count].m_Col = x;
					m_SectorArounds[row][col].m_Arounds[m_SectorArounds[row][col].m_Count].m_Row = y;
					m_SectorArounds[row][col].m_Count++;
				}
			}
		}
	}
}

onenewarm::GameGroup::~GameGroup()
{
	delete m_DBConnection;
}

void onenewarm::GameGroup::SendSectorAround(Player* player, CPacket* pkt)
{
	SECTOR_AROUND* sectorAround = &(m_SectorArounds[player->m_CurSector.m_Row][player->m_CurSector.m_Col]);

	for (int cnt = 0; cnt < sectorAround->m_Count; ++cnt)
	{
		SectorPos sectPos = sectorAround->m_Arounds[cnt];
		int row = sectPos.m_Row; int col = sectPos.m_Col;

		for (auto iter = m_Sectors[row][col].begin(); iter != m_Sectors[row][col].end(); ++iter) {
			__int64 curPlayerId = *iter;
			WORD playerIdx = (WORD)curPlayerId;
			Player* reception = &m_Players[playerIdx];

			if (reception == player) continue;

			m_Server->SendPacket(reception->m_SessionID, pkt);
		}
	}
}

void onenewarm::GameGroup::SendSectorAround(SECTOR_AROUND& around, CPacket* pkt)
{
	for (int cnt = 0; cnt < around.m_Count; ++cnt)
	{
		SectorPos sectPos = around.m_Arounds[cnt];
		int row = sectPos.m_Row; int col = sectPos.m_Col;

		for (auto iter = m_Sectors[row][col].begin(); iter != m_Sectors[row][col].end(); ++iter) {
			__int64 curPlayerId = *iter;
			WORD playerIdx = (WORD)curPlayerId;
			Player* reception = &m_Players[playerIdx];

			m_Server->SendPacket(reception->m_SessionID, pkt);
		}
	}
}

onenewarm::SectorPos onenewarm::GetSector(short x, short y)
{
	if (x < 0 || y < 0) return { -1, -1 };

	return { x / SECT_SIZE, y / SECT_SIZE };
}

void onenewarm::GameGroup::MoveSect(__int64 sessionId, SectorPos sect)
{
	GameServer* server = (GameServer*)m_Server;

	Player* player = &m_Players[(WORD)sessionId];

	// 섹터 내에서 player 찾아서 지우기
	int curRow = player->m_CurSector.m_Row;
	int curCol = player->m_CurSector.m_Col;

	auto endIter = m_Sectors[curRow][curCol].end();


	for (auto iter = m_Sectors[curRow][curCol].begin(); iter != endIter; ++iter)
	{
		if (*iter == sessionId)
		{
			m_Sectors[curRow][curCol].erase(iter);
			
			break;
		}
	}

	player->m_PrevSector = player->m_CurSector;
	player->m_CurSector = sect;

	// 새로운 섹터에 player 추가하기
	m_Sectors[sect.m_Row][sect.m_Col].push_back(sessionId);

	// CurSect와 PrevSect에서 겹치지 않는 Sector 찾기
	// Delete 섹터와 Create 섹터로 나누기
	SECTOR_AROUND deleteAround;
	deleteAround.m_Count = 0;

	SECTOR_AROUND createAround;
	createAround.m_Count = 0;

	SECTOR_AROUND& prevAround = m_SectorArounds[player->m_PrevSector.m_Row][player->m_PrevSector.m_Col];
	SECTOR_AROUND& curAround = m_SectorArounds[player->m_CurSector.m_Row][player->m_CurSector.m_Col];

	// Delete 섹터 내에 세션들에게 Delete 패킷 보내기
	{
		for (int prevCnt = 0; prevCnt < prevAround.m_Count; ++prevCnt)
		{
			int prevRow = prevAround.m_Arounds[prevCnt].m_Row;
			int prevCol = prevAround.m_Arounds[prevCnt].m_Col;
			bool isIn = false;
			for (int curCnt = 0; curCnt < curAround.m_Count; ++curCnt)
			{
				int curRow = curAround.m_Arounds[curCnt].m_Row;
				int curCol = curAround.m_Arounds[curCnt].m_Col;

				if (prevRow == curRow && prevCol == curCol)
				{
					isIn = true;
					break;
				}
			}

			if (isIn == false && m_Sectors[prevRow][prevCol].size() > 0)
			{
				deleteAround.m_Arounds[deleteAround.m_Count++] = prevAround.m_Arounds[prevCnt];
			}
		}

		if (deleteAround.m_Count > 0)
		{
			CPacketRef deletePkt = server->Make_EN_PACKET_CS_GAME_RES_DELETE_CHARACTER_Packet(server, player->m_AccountNo);
			SendSectorAround(deleteAround, *deletePkt);

			for(int aroundCnt = 0 ; aroundCnt < deleteAround.m_Count ; ++aroundCnt)
			{
				int sectRow = deleteAround.m_Arounds[aroundCnt].m_Row;
				int sectCol = deleteAround.m_Arounds[aroundCnt].m_Col;
				for (auto playerIter = m_Sectors[sectRow][sectCol].begin(); playerIter != m_Sectors[sectRow][sectCol].end(); ++playerIter)
				{
					__int64 playerAccountNo = m_Players[(WORD)(*playerIter)].m_AccountNo;
					CPacketRef deleteOtherPkt = server->Make_EN_PACKET_CS_GAME_RES_DELETE_CHARACTER_Packet(server, playerAccountNo);
					server->SendPacket(sessionId, *deleteOtherPkt);
				}
			}
		}
		
	}

	// Create 섹터 내에 세션들에게 CreateOther 패킷 보내기
	{
		for (int curCnt = 0; curCnt < curAround.m_Count; ++curCnt)
		{
			int curRow = curAround.m_Arounds[curCnt].m_Row;
			int curCol = curAround.m_Arounds[curCnt].m_Col;
			bool isIn = false;
			for (int prevCnt = 0; prevCnt < prevAround.m_Count; ++prevCnt)
			{
				int prevRow = prevAround.m_Arounds[prevCnt].m_Row;
				int prevCol = prevAround.m_Arounds[prevCnt].m_Col;

				if (prevRow == curRow && prevCol == curCol)
				{
					isIn = true;
					break;
				}
			}

			if (isIn == false && m_Sectors[curRow][curCol].size() > 0)
			{
				createAround.m_Arounds[createAround.m_Count++] = curAround.m_Arounds[curCnt];
			}
		}

		if (createAround.m_Count > 0)
		{
			CPacketRef createPkt = server->Make_EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER_Packet(server, player->m_AccountNo, (char*)player->m_Nickname, player->m_X, player->m_Y, player->m_Direction);
			SendSectorAround(createAround, *createPkt);

			if (player->m_IsMove == true)
			{
				CPacketRef moveStartPkt = server->Make_EN_PACKET_CS_GAME_RES_MOVE_START_Packet(server, player->m_AccountNo, player->m_MoveDirection);
				SendSectorAround(createAround, *moveStartPkt);
			}

			for (int aroundCnt = 0; aroundCnt < createAround.m_Count; ++aroundCnt)
			{
				int sectRow = createAround.m_Arounds[aroundCnt].m_Row;
				int sectCol = createAround.m_Arounds[aroundCnt].m_Col;
				for (auto playerIter = m_Sectors[sectRow][sectCol].begin(); playerIter != m_Sectors[sectRow][sectCol].end(); ++playerIter)
				{
					Player* otherPlayer = &m_Players[(WORD)(*playerIter)];
					CPacketRef createOtherPkt = server->Make_EN_PACKET_CS_GAME_RES_CREATE_OTHER_CHARACTER_Packet(server, otherPlayer->m_AccountNo, (char*)otherPlayer->m_Nickname, otherPlayer->m_X, otherPlayer->m_Y, otherPlayer->m_Direction);
					m_Server->SendPacket(sessionId, *createOtherPkt);

					if (otherPlayer->m_IsMove == true)
					{
						CPacketRef moveStartPkt = server->Make_EN_PACKET_CS_GAME_RES_MOVE_START_Packet(server, otherPlayer->m_AccountNo, otherPlayer->m_MoveDirection);
						m_Server->SendPacket(sessionId, *moveStartPkt);
					}
				}
			}
		}
	}
}

void onenewarm::InitPlayer(Player* dest, __int64 sessionId, __int64 accountNo, const wchar_t* nickname, short x, short y)
{
	dest->m_IsActive = true;
	dest->m_SessionID = sessionId;
	dest->m_AccountNo = accountNo;
	::wmemcpy(dest->m_Nickname, nickname, wcslen(nickname) + 1);
	dest->m_EraseFlag = false;

	dest->m_CurSector = GetSector(x, y);
	dest->m_PrevSector = { -1, -1 };
	dest->m_IsMove = false;
	dest->m_Direction = dfPACKET_MOVE_DIR_DD;
	dest->m_MoveDirection = -1;
	dest->m_X = x;
	dest->m_Y = y;
}
