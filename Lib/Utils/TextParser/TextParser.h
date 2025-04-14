#pragma once
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace onenewarm
{
	void ConvertAtoW(wchar_t* out, const char* src);
	void ConvertWtoA(char* out, const wchar_t* src);

	class TextParser
	{

	public:
		TextParser();
		virtual ~TextParser();
		bool LoadFile(wstring fileName);
		bool GetValue(int* dest, const wstring& space, const wstring& key);
		bool GetValue(wstring* dest, const wstring& space, const wstring& key);
		bool GetValue(vector<wstring>* dest, const wstring& space, const wstring& key);
		bool GetValue(vector<int>* dest,  const wstring& space, const wstring& key);

	protected:
		bool SkipNoneCommand(wstring* dest, wstring& src);
		const wchar_t* GetNextWord(wstring* dest, const wchar_t* src, const wchar_t* srcEnd);

	protected:
		bool m_IsFoundKey;

		unordered_map<wstring, unordered_map<wstring, vector<wstring>>> m_ParseSpaceStrMap;
		unordered_map<wstring, unordered_map<wstring, vector<int>>> m_ParseSpaceIntMap;
	};

}