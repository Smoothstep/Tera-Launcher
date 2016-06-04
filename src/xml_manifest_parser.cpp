
#include "xml_manifest_parser.h"

std::string & CManifest::LastError()
{
	return m_LastError;
}

bool CManifest::LoadManifestFile(const char * pData, size_t iSize)
{
	if (!m_XMLDocument.ReadXML(pData, iSize))
	{
		m_LastError = std::string("Unable to read xml data");
		return false;
	}

	return true;
}

bool CManifest::LoadManifestFile(std::string strManifestFile)
{
	boost::iostreams::mapped_file file(strManifestFile, std::ios::in);

	if (!file.is_open())
	{
		m_LastError = std::string("Unable to open: ") + strManifestFile;
		return false;
	}

	if (!m_XMLDocument.ReadXML(file.data(), file.size()))
	{
		m_LastError = std::string("Unable to read xml: ") + strManifestFile;
		return false;
	}

	return true;
}

bool CManifest::LoadManifestData(std::string strManifest)
{
	if (!m_XMLDocument.ReadXML(strManifest.c_str(), strManifest.size()))
	{
		m_LastError = std::string("Unable to read xml data");
		return false;
	}

	return true;
}

int CManifest::GetLatestVersion()
{
	CXMLNode* pNode = m_XMLDocument.Node()->GetNode("RequiredRelease");

	if (!pNode)
	{
		return -1;
	}

	return pNode->GetValue<int>();
}

int CManifest::GetReleaseByVersion(std::string version)
{
	CXMLNode* pReleaseNode = m_XMLDocument.Node()->GetNode("Releases");

	if (!pReleaseNode)
	{
		return -1;
	}

	for (auto it = pReleaseNode->begin(); it != pReleaseNode->end(); ++it)
	{
		CXMLNode* pNameNode = (*it)->GetNode("Name");

		if (!pNameNode)
		{
			continue;
		}

		if (version == pNameNode->GetValue<std::string>())
		{
			CXMLNode* pIdNode = (*it)->GetNode("Id");

			if (!pIdNode)
			{
				return -1;
			}

			return pIdNode->GetValue<int>();
		}
	}

	return -1;
}

std::string CManifest::GetLatestReleaseVersionString()
{
	CXMLNode* pReleaseNode = m_XMLDocument.Node()->GetNode("Releases");

	if (!pReleaseNode)
	{
		return "";
	}

	CXMLNode* pLatestReleaseNode = pReleaseNode->back();

	if (!pLatestReleaseNode)
	{
		return "";
	}

	CXMLNode* pNameNode = pLatestReleaseNode->GetNode("Name");

	if (!pNameNode)
	{
		return "";
	}

	return pNameNode->GetValue<std::string>();
}

bool CManifest::GetReleasesToDownloadFor(int iMyRelease, int iTargetRelease, std::vector<int>& v)
{
	CXMLNode* pReleaseUpdatePathesNode = m_XMLDocument.Node()->GetNode("ReleaseUpdatePaths");

	if (!pReleaseUpdatePathesNode)
	{
		m_LastError = "No such node in tree: ReleaseUpdatePaths";
		return false;
	}

	int iLastRelease = iMyRelease;

	while (iLastRelease != iTargetRelease)
	{
		int iReleaseNext = iLastRelease;

		for (CXMLNode::iterator it = pReleaseUpdatePathesNode->begin(); it != pReleaseUpdatePathesNode->end(); ++it)
		{
			CXMLNode* pFromNode = (*it)->GetNode("From");

			if (!pFromNode)
			{
				m_LastError = std::string("No such node in tree: From");
				return false;
			}

			int from = pFromNode->GetValue<int>();

			if (from != iLastRelease)
			{
				continue;
			}

			CXMLNode* pToNode = (*it)->GetNode("To");

			if (!pToNode)
			{
				m_LastError = std::string("No such node in tree: To");
				return false;
			}

			int to = pToNode->GetValue<int>();

			if (to < iReleaseNext)
			{
				continue;
			}

			iReleaseNext = to;
		}

		v.push_back(iReleaseNext);
		iLastRelease = iReleaseNext;
	}

	return true;
}

bool CManifest::GetDownloadPathForRelease(int iMyRelease, int iTargetRelease, std::string & v)
{
	CXMLNode* pReleaseUpdatePathesNode = m_XMLDocument.Node()->GetNode("ReleaseUpdatePaths");

	if (!pReleaseUpdatePathesNode)
	{
		m_LastError = "No such node in tree: ReleaseUpdatePaths";
		return false;
	}

	for (CXMLNode::iterator it = pReleaseUpdatePathesNode->begin(); it != pReleaseUpdatePathesNode->end(); ++it)
	{
		CXMLNode* pFromNode = (*it)->GetNode("From");

		if (!pFromNode)
		{
			m_LastError = std::string("No such node in tree: From");
			return false;
		}

		int from = pFromNode->GetValue<int>();

		if (from != iMyRelease)
		{
			continue;
		}

		CXMLNode* pToNode = (*it)->GetNode("To");

		if (!pToNode)
		{
			m_LastError = std::string("No such node in tree: To");
			return false;
		}

		int to = pToNode->GetValue<int>();

		if (to != iTargetRelease)
		{
			continue;
		}

		CXMLNode* pExtraDataNode = (*it)->GetNode("ExtraData");

		if (!pExtraDataNode)
		{
			m_LastError = std::string("No such node in tree: ExtraData");
			return false;
		}

		if (pExtraDataNode->empty())
		{
			m_LastError = std::string("Empty ExtraData");
			return false;
		}

		CXMLNode* pValueNode = (*pExtraDataNode->begin())->GetNode("Value");

		if (!pValueNode)
		{
			m_LastError = std::string("No Value in ExtraData");
			return false;
		}

		v = pValueNode->GetValue<std::string>();

		return true;
	}

	return false;
}
