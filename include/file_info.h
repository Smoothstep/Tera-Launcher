#ifndef __SHA1_H__
#define __SHA1_H__

#pragma once

#include "parser.h"

#include <map>

class CFileInfo
{
private:
	CXMLDocument m_XMLDocument;

protected:
	std::string m_LastErrorMessage;

protected:
	std::map<std::string, size_t> m_mPathOffsets;

public:
	CFileInfo();
	~CFileInfo();

	bool CheckSHA1Valid(const std::string &strFile, const std::string &strFileRelative);
	bool CheckSizeValid(const std::string &strFile, const std::string &strFileRelative);

	bool LoadFileInfo(const std::string &strFile);
	bool LoadFileInfo(const char* pData, size_t iSize);

	std::string& GetLastErrorMessage();

private:
	bool GetPathOffsets();
};

#endif
