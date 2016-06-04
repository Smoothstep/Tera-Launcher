#ifndef __MAPPED_FILE__H_
#define __MAPPED_FILE__H_

#pragma once

#include <Windows.h>

typedef unsigned long DWORD;
typedef unsigned long long QWORD;

class CMappedFile
{
private:
	HANDLE m_hFile;
	HANDLE m_hMap;

	DWORD m_dwAllocationGranularity;
	QWORD m_qwFileSize;
	QWORD m_qwCurrentOffset;

public:
	CMappedFile() : m_hFile(NULL), m_hMap(NULL), m_qwCurrentOffset(0)
	{
		SYSTEM_INFO sysInfo;
		{
			GetSystemInfo(&sysInfo);
		}

		m_dwAllocationGranularity = sysInfo.dwAllocationGranularity;
	}

	~CMappedFile()
	{
		CloseHandle(m_hFile);
		CloseHandle(m_hMap);
	}

	bool Create(const char* szFile)
	{
		m_hFile = CreateFileA(szFile, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

		if (m_hFile == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		m_hMap = CreateFileMappingA(m_hFile, 0, PAGE_READONLY, 0, 0, 0);

		if (!m_hMap)
		{
			return false;
		}

		LARGE_INTEGER size;

		if (!GetFileSizeEx(m_hFile, &size))
		{
			return false;
		}

		m_qwFileSize = size.QuadPart;

		return true;
	}

	inline QWORD GetFileSize()
	{
		return m_qwFileSize;
	}

	inline LPVOID Read(size_t count = 0)
	{
		DWORD dwHigh = static_cast<DWORD>((m_qwCurrentOffset >> 32) & 0xFFFFFFFFul);
		DWORD dwLow = static_cast<DWORD>((m_qwCurrentOffset)& 0xFFFFFFFFul);

		QWORD qwView = m_qwCurrentOffset + count * m_dwAllocationGranularity;
		DWORD dwChun = count * m_dwAllocationGranularity;

		if (qwView > m_qwFileSize)
		{
			dwChun = static_cast<DWORD>(m_qwFileSize - m_qwCurrentOffset);

			if (dwChun == 0)
			{
				return NULL;
			}
		}

		m_qwCurrentOffset += dwChun;

		return MapViewOfFile(m_hMap, FILE_MAP_READ, dwHigh, dwLow, dwChun);
	}

	inline LPVOID Read(size_t count, size_t& read)
	{
		DWORD dwHigh	= static_cast<DWORD>((m_qwCurrentOffset >> 32)	& 0xFFFFFFFFul);
		DWORD dwLow		= static_cast<DWORD>((m_qwCurrentOffset)		& 0xFFFFFFFFul);

		QWORD qwView = m_qwCurrentOffset + count * m_dwAllocationGranularity;
		DWORD dwChun = count * m_dwAllocationGranularity;

		if (qwView > m_qwFileSize)
		{
			dwChun = static_cast<DWORD>(m_qwFileSize - m_qwCurrentOffset);

			if (dwChun == 0)
			{
				read = 0;
				return NULL;
			}
		}

		m_qwCurrentOffset += dwChun;

		return MapViewOfFile(m_hMap, FILE_MAP_READ, dwHigh, dwLow, read = dwChun);
	}

	inline LPVOID ReadAt(QWORD offset, size_t count, size_t& read)
	{
		DWORD dwHigh	= static_cast<DWORD>((offset >> 32) & 0xFFFFFFFFul);
		DWORD dwLow		= static_cast<DWORD>((offset)		& 0xFFFFFFFFul);

		QWORD qwView = offset + count * m_dwAllocationGranularity;
		DWORD dwChun = count * m_dwAllocationGranularity;

		if (qwView > m_qwFileSize)
		{
			if (offset >= m_qwFileSize)
			{
				read = 0;
				return NULL;
			}

			dwChun = static_cast<DWORD>(m_qwFileSize - offset);
		}

		return MapViewOfFile(m_hMap, FILE_MAP_READ, dwHigh, dwLow, read = dwChun);
	}

	inline bool Unmap(LPVOID lpData)
	{
		return UnmapViewOfFile(lpData);
	}
};

#endif