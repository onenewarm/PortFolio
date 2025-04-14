#include "MonitorNetServer.h"
#include "CommonProtocol.h"


MonitorNetServer::MonitorNetServer()
{
	ZeroMemory(&m_ServerOns, sizeof(m_ServerOns));
	InitializeSRWLock(&m_ConnSessionLock);
}

MonitorNetServer::~MonitorNetServer()
{
}

bool MonitorNetServer::OnConnectionRequest(const char* IP, short port)
{
	// 할 일 없음
	return false;
}

void MonitorNetServer::OnClientJoin(__int64 sessionID)
{
}

void MonitorNetServer::OnSessionRelease(__int64 sessionID)
{
	AcquireSRWLockExclusive(&m_ConnSessionLock);
	if (m_ConnSessions.find(sessionID) != m_ConnSessions.end()) {
		m_ConnSessions.erase(sessionID);
	}
	ReleaseSRWLockExclusive(&m_ConnSessionLock);
}

void MonitorNetServer::OnRecv(__int64 sessionID, CPacketRef& pkt)
{
	WORD type;
	
	try
	{
		*(*pkt) >> type;
	}
	catch (int e)
	{
		OnSessionError(sessionID, -1, L"Failed unmashalling.", __FILE__, __LINE__);
		return;
	}

	switch (type) {
	case EN_PACKET_CS_MONITOR_TOOL_REQ_LOGIN:
		HandleMonitorToolLogin(sessionID, pkt);
		break;
	default:
		OnSessionError(sessionID, -1, L"Recved unvalied type packet.", __FILE__, __LINE__);
		break;
	}
}

void MonitorNetServer::OnSessionError(__int64 sessionId, int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_WARNING, filePath, fileLine, errMsg);

	switch (errCode)
	{
	case EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN:
	{
		CPacketRef resLoginMsg = AllocPacket();
		*(*resLoginMsg) << EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN << (BYTE)dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY;
		SendPacket(sessionId, *resLoginMsg);
		SendDisconnect(sessionId);
		break;
	}
	default:
	{
		Disconnect(sessionId);
		break;
	}
	}
}

void MonitorNetServer::OnSystemError(int errCode, const wchar_t* errMsg, const char* filePath, unsigned int fileLine)
{
	_LOG(LOG_LEVEL_ERROR, filePath, fileLine, errMsg);

	switch (errCode)
	{
	default:
	{
		CCrashDump::Crash();
		break;
	}
	}
}


void MonitorNetServer::OnStart(ConfigLoader* confLoader)
{
	wstring loginKey;
	confLoader->GetValue(&loginKey, L"SERVICE", L"LOGIN_KEY");
	char tmpLoginKey[33];
	ConvertWtoA(tmpLoginKey, loginKey.c_str());
	memcpy(m_LoginSessionKey, tmpLoginKey, loginKey.size());
}

void MonitorNetServer::OnStop()
{
	// 할 일 없음
}

void MonitorNetServer::OnWorker(__int64 completionKey)
{
	// 할 일 없음
}

void MonitorNetServer::OnWorkerExpired(__int64 completionKey)
{
	// 할 일 없음
}

void MonitorNetServer::SendBroadcast(CPacket* pkt)
{
	for (auto iter = m_ConnSessions.begin(); iter != m_ConnSessions.end(); ++iter) {
		SendPacket(*iter, pkt);
	}
}

void MonitorNetServer::HandleMonitorToolLogin(__int64 sessionID, CPacketRef& pkt)
{
	char LoginSessionKey[32];

	try
	{
		(*pkt)->GetData(LoginSessionKey, sizeof(LoginSessionKey));
	}
	catch (int e)
	{
		OnSessionError(sessionID, -1, L"Failed unmarshalling.", __FILE__, __LINE__);
		return;
	}
	

	for (int cnt = 0; cnt < sizeof(m_LoginSessionKey); ++cnt) {

		// TODO : 다른 서버에 접속하는 인증 키를 들고 오는 경우를 체크

		// SessionKey 체크
		if (m_LoginSessionKey[cnt] != LoginSessionKey[cnt]) {
			OnSessionError(sessionID, EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN, L"[NetClient]LoginSessionKey was not same.", __FILE__, __LINE__);
			return;
		}
	}

	// 성공
	AcquireSRWLockExclusive(&m_ConnSessionLock);
	m_ConnSessions.insert(sessionID);
	ReleaseSRWLockExclusive(&m_ConnSessionLock);

	SetLogin(sessionID);

	CPacketRef resLoginMsg = AllocPacket();
	*(*resLoginMsg) << EN_PACKET_CS_MONITOR_TOOL_RES_LOGIN << (BYTE)dfMONITOR_TOOL_LOGIN_OK;
	SendPacket(sessionID, *resLoginMsg);
}
