
#include "file_info.h"

#include <boost/uuid/sha1.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include <boost/algorithm/string.hpp>

#include <fstream>
#include <algorithm>
#include <functional>
#include <iterator>

CFileInfo::CFileInfo()
{
}

CFileInfo::~CFileInfo()
{
}

bool CFileInfo::CheckSHA1Valid(const std::string &strFile, const std::string &strFileRelative)
{
	boost::iostreams::mapped_file file(strFile, boost::iostreams::mapped_file::readonly);

	if (!file.is_open())
	{
		m_LastErrorMessage = "File does not exist: " + strFile;
		return false;
	}

	CPairNode* pFilesNode = m_XMLDocument.Node()->GetNode("Files");

	if (!pFilesNode)
	{
		m_LastErrorMessage = "Patchfile tree has no files node";
		return false;
	}

	CPairNode* pFileNode = (*pFilesNode)[m_mPathOffsets[strFile]];
	CPairNode* pSHANode = pFileNode->GetNode("SHA1");

	if (!pSHANode)
	{
		m_LastErrorMessage = "File tree has no SHA1 node";
		return false;
	}

	std::string strFileSHA = pSHANode->GetValue<std::string>();

	boost::uuids::detail::sha1 sha;
	{
		sha.process_bytes(file.const_data(), file.size());
	}

	unsigned int digest[5];
	{
		sha.get_digest(digest);
	}

	char hash[20];
	{
		for (int i = 0; i < 5; ++i)
		{
			uint8_t *pTmp = reinterpret_cast<uint8_t*>(digest);

			hash[i * 4 + 0] = pTmp[i * 4 + 3];
			hash[i * 4 + 1] = pTmp[i * 4 + 2];
			hash[i * 4 + 2] = pTmp[i * 4 + 1];
			hash[i * 4 + 3] = pTmp[i * 4 + 0];
		}
	}

	std::stringstream paramFileHash;
	{
		paramFileHash << std::hex;

		for (int i = 0; i < 20; ++i)
		{
			paramFileHash << ((hash[i] & 0x000000F0) >> 4);
			paramFileHash << ((hash[i] & 0x0000000F));
		}
	}

	return paramFileHash.str() == strFileSHA;
}

bool CFileInfo::CheckSizeValid(const std::string &strFile, const std::string &strFileRelative)
{
	std::ifstream file(strFile, std::ios::in | std::ios::binary);

	if (!file.is_open())
	{
		m_LastErrorMessage = "File does not exist: " + strFile;
		return false;
	}

	CPairNode* pFilesNode = m_XMLDocument.Node()->GetNode("Files");

	if (!pFilesNode)
	{
		m_LastErrorMessage = "Patchfile tree has no files node";
		return false;
	}

	auto it = m_mPathOffsets.find(strFileRelative);

	if (it == m_mPathOffsets.end())
	{
		m_LastErrorMessage = std::string("Unable to find ") + strFileRelative;
		return false;
	}

	CPairNode* pFileNode = (*pFilesNode)[it->second];
	CPairNode* pSizeNode = pFileNode->GetNode("Size");

	if (!pSizeNode)
	{
		m_LastErrorMessage = "File node has no size node";
		return false;
	}

	file.seekg(0, file.end);

	if (pSizeNode->GetValue<int64_t>() != file.tellg())
	{
		m_LastErrorMessage = "Size mismatch: " + strFile;
		return false;
	}

	return true;
}

bool CFileInfo::LoadFileInfo(const std::string& strFile)
{
	boost::iostreams::mapped_file file(strFile, std::ios::in);

	if (!file.is_open())
	{
		m_LastErrorMessage = std::string("Unable to open: ") + strFile;
		return false;
	}

	if (!m_XMLDocument.ReadXML(file.data(), file.size()))
	{
		m_LastErrorMessage = std::string("Unable to read xml: ") + strFile;
		return false;
	}

	if (!m_XMLDocument.Node())
	{
		m_LastErrorMessage = std::string("Invalid xml file");
		return false;
	}

	return GetPathOffsets();
}

bool CFileInfo::LoadFileInfo(const char * pData, size_t iSize)
{
	if (!m_XMLDocument.ReadXML(pData, iSize))
	{
		m_LastErrorMessage = std::string("Unable to read xml data");
		return false;
	}

	if (!m_XMLDocument.Node())
	{
		m_LastErrorMessage = std::string("Invalid xml data");
		return false;
	}

	return GetPathOffsets();
}

std::string & CFileInfo::GetLastErrorMessage()
{
	return m_LastErrorMessage;
}

bool CFileInfo::GetPathOffsets()
{
	CPairNode* pFilesNode = m_XMLDocument.Node()->GetNode("Files");

	if (!pFilesNode)
	{
		m_LastErrorMessage = "No File tree in properties";
		return false;
	}

	for (size_t i = 0; i < pFilesNode->size(); ++i)
	{
		CPairNode* pNameNode = (*pFilesNode)[i]->GetNode("Name");
		
		if (!pNameNode)
		{
			m_LastErrorMessage = "No name node in file node";
			return false;
		}

		CPairNode* pSizeNode = (*pFilesNode)[i]->GetNode("Size");

		if (!pSizeNode)
		{
			m_LastErrorMessage = "No size node in file node";
			return false;
		}

		m_mPathOffsets.insert(std::make_pair(pNameNode->GetValue<std::string>(), i));
	}

	return true;
}
