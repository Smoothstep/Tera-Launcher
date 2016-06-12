#ifndef _XML_MANIFEST_PARSER_H_
#define _XML_MANIFEST_PARSER_H_

#pragma once

#include "xml.h"

class CManifest
{
private:
	CXMLDocument m_XMLDocument;

protected:
	std::string m_LastError;

public:
	std::string& LastError();

	bool LoadManifestFile(const char* pData, size_t iSize);
	bool LoadManifestFile(std::string strManifestFile);
	bool LoadManifestData(std::string strManifest);

	int GetLatestVersion();
	int GetReleaseByVersion(std::string version);

	std::string GetLatestReleaseVersionString();

	bool GetReleasesToDownloadFor(int iMyRelease, int iTargetRelease, std::vector<int>& v);
	bool GetDownloadPathForRelease(int iMyRelease, int iTargetRelease, std::string& v);
};

#endif
