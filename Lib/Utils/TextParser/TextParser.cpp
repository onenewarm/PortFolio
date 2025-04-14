#include "TextParser.h"
#include "..\CCrashDump.h"
#include <stdio.h>

using namespace std;

onenewarm::TextParser::TextParser() : m_IsFoundKey(0)
{
}

onenewarm::TextParser::~TextParser()
{
}


bool onenewarm::TextParser::SkipNoneCommand(std::wstring* dest, std::wstring& src)
{
	int iSrc = 0;
	while (iSrc < src.size())
	{
		if (!(src[iSrc] == L'\r'|| src[iSrc] == L' ' || src[iSrc] == L'\t'))
		{
			
			if (src[iSrc] == '/')
			{
				if (iSrc + 1 < src.size())
				{
					if (src[iSrc + 1] == '/')
					{
						iSrc += 2;
						while (src[iSrc] != '\n' && iSrc < src.size())
						{
							iSrc++;
						}
					}
					else if (src[iSrc + 1] == '*')
					{
						iSrc += 2;
						while (src[iSrc] != '*' && src[iSrc + 1] != '/')
						{
							iSrc++;
							if (iSrc == src.size() - 1) return false;
						}
						iSrc++;
					}
					else *dest += src[iSrc];
				}
				else *dest += src[iSrc];
			}
			else {
				do {
					if (src[iSrc] == L'\n') {
						if (dest->size() == 0) break;
						if ((*dest)[dest->size() - 1] == L'\n') break;
					}

					*dest += src[iSrc];
				} while (0);
			}
		}

		++iSrc;
	}
	return true;
}



const wchar_t* onenewarm::TextParser::GetNextWord(wstring* dest, const wchar_t* src, const wchar_t* srcEnd)
{
	const wchar_t* frontPtr = src;
	const wchar_t* lastPtr = src;
	
	while (true) {
		if (frontPtr > srcEnd) return frontPtr;

		while (*lastPtr != L'=' && *lastPtr != L'\n' && *lastPtr != L'{' && *lastPtr != L'}') {
			++lastPtr;
		}

		if (frontPtr == lastPtr) {
			lastPtr++;
			frontPtr = lastPtr;
			continue;
		}

		while (frontPtr != lastPtr) {

			dest->push_back(*frontPtr);
			++frontPtr;
		}

		return lastPtr + 1;
	}
}


bool onenewarm::TextParser::GetValue(int* dest, const wstring& space, const wstring& key)
{
	if (m_ParseSpaceIntMap.find(space) == m_ParseSpaceIntMap.end()) return false;
	auto parseMap = m_ParseSpaceIntMap[space];
	
	if (parseMap.find(key) == parseMap.end()) return false;
	*dest = parseMap[key][0];

	return true;
}

bool onenewarm::TextParser::GetValue(wstring* dest, const wstring& space, const wstring& key)
{
	if (m_ParseSpaceStrMap.find(space) == m_ParseSpaceStrMap.end()) return false;
	auto parseMap = m_ParseSpaceStrMap[space];

	if (parseMap.find(key) == parseMap.end()) return false;
	wstring val = parseMap[key][0];

	*dest = val;

	return true;
}

bool onenewarm::TextParser::GetValue(vector<wstring>* dest, const wstring& space, const wstring& key)
{
	if (m_ParseSpaceStrMap.find(space) == m_ParseSpaceStrMap.end()) return false;
	auto parseMap = m_ParseSpaceStrMap[space];

	if (parseMap.find(key) == parseMap.end()) return false;

	for (int cnt = 0; cnt < parseMap[key].size(); ++cnt) {
		dest->push_back(parseMap[key][cnt]);
	}

	return true;
}

bool onenewarm::TextParser::GetValue(vector<int>* dest, const wstring& space, const wstring& key)
{
	if (m_ParseSpaceIntMap.find(space) == m_ParseSpaceIntMap.end()) return false;
	auto parseMap = m_ParseSpaceIntMap[space];

	if (parseMap.find(key) == parseMap.end()) return false;

	for (int cnt = 0; cnt < parseMap[key].size(); ++cnt) {
		dest->push_back(parseMap[key][cnt]);
	}

	return true;
}

bool onenewarm::TextParser::LoadFile(std::wstring fileName)
{
	FILE* openFile;
	DWORD errCode;
	errno_t openErr = _wfopen_s(&openFile, &fileName[0], L"rb");

	if (openErr != 0 || openFile == NULL) {
		CCrashDump::Crash();
		return false;
	}

	size_t fileSize;
	fseek(openFile, 0, SEEK_END);
	fileSize = ftell(openFile);
	fseek(openFile, 0, SEEK_SET);

	fseek(openFile, 2, SEEK_CUR);
	fileSize = (fileSize / 2) - 1;

	std::wstring buffer;

	buffer.resize(fileSize);

	size_t readBytes = fread_s(&buffer[0], fileSize * 2, 2, fileSize, openFile);

	if (readBytes != fileSize){
		errCode = GetLastError();
		CCrashDump::Crash();
	}

	wstring skipBuf;

	if (SkipNoneCommand(&skipBuf, buffer) == false) {
		CCrashDump::Crash(); // 주석을 잘못 입력
	}

	const wchar_t* curPtr = &skipBuf[0];

	wstring spaceName;
	wstring key;

	while (true) {
		wstring buf;
		curPtr = GetNextWord(&buf, curPtr, &skipBuf[skipBuf.size() - 1]);

		if (curPtr > &skipBuf[skipBuf.size() - 1]) break;

		if (buf[0] == L':') {
			wstring tmp;
			tmp = buf.substr(1, buf.size());
			spaceName = tmp;
			m_ParseSpaceStrMap.insert({ spaceName, unordered_map<wstring, vector<wstring>>() });
			m_ParseSpaceIntMap.insert({ spaceName, unordered_map<wstring, vector<int>>() });
			continue;
		}
		
		if (m_IsFoundKey == true) {
			if (buf[0] == L'"') {
				if (m_ParseSpaceStrMap.find(key) == m_ParseSpaceStrMap.end()) {
					m_ParseSpaceStrMap[spaceName].insert({key, vector<wstring>()});
				}

				wstring tmp;
				tmp = buf.substr(1, buf.size() - 2);
				m_ParseSpaceStrMap[spaceName][key].push_back(tmp);
			}
			else {
				if (m_ParseSpaceIntMap.find(key) == m_ParseSpaceIntMap.end()) {
					m_ParseSpaceIntMap[spaceName].insert({ key, vector<int>() });
				}

				m_ParseSpaceIntMap[spaceName][key].push_back(stoi(buf));
			}
			m_IsFoundKey = false;
		}
		else {
			key = buf;
			m_IsFoundKey = true;
		}
	}

	fclose(openFile);

	return true;
}

void onenewarm::ConvertAtoW(wchar_t* out, const char* src)
{
	LONG64 srcLen = strlen(src);

	for (LONG64 cnt = 0; cnt < srcLen; ++cnt) {
		out[cnt] = (wchar_t)src[cnt];
	}

	out[srcLen] = L'\0';
}

void onenewarm::ConvertWtoA(char* out, const wchar_t* src)
{
	LONG64 srcLen = wcslen(src);

	for (LONG64 cnt = 0; cnt < srcLen; ++cnt) {
		out[cnt] = (char)src[cnt];
	}

	out[srcLen] = L'\0';
}
