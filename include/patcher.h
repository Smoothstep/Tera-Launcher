#ifndef _PATCHER_H_
#define _PATCHER_H_

#pragma once

#include "xml_manifest_parser.h"
#include "file_info.h"
#include "patcher_callback.h"
#include "patcher_service.h"

#include <set>

#define NUM_DEFAULT_PATCH_THREADS 4
#define PATCHER_CONFIG_PATH "config\\patcher.ini"

static const std::string g_strDownloadHost		= "dl.tera.gameforge.com";
static const std::string g_strDownloadClient	= "dl.tera.gameforge.com/tera/client/pgc_v2/";
static const std::string g_strDownloadManifest	= "http://dl.tera.gameforge.com/tera/client/pgc_v2/pgc_v2.patchmanifest";

class CArchive;
class CArchiveFile;
class CArchiveDirectoryFile;

class CPatcher
{
protected:
	CInitializationDocument *m_Config;

private:
	int32_t m_iMyVersion;

	std::string m_strVersion;
	std::string m_strGamePath;
	std::string m_strClientPath;
	std::string m_strTempPath;

	std::string m_strDownloadHost;
	std::string m_strDownloadClient;
	std::string m_strDownloadManifest;

	uint8_t*	m_DownloadBuffer;
	uint32_t	m_DownloadBufferSize;
	uint32_t	m_CurrentBufferOffset;

	CManifest m_Manifest;
	CFileInfo m_FileInfo;

	CPatchService m_PatchService;

	Callback::CPatchCallback& m_Callback;

	boost::thread m_PatchThread;
	boost::atomic_bool m_bStop;

	boost::asio::io_service m_HTTPService;
	boost::asio::ip::tcp::socket m_HTTPSocket;

public:
	CPatcher(Callback::CPatchCallback& callback);
	~CPatcher();

	bool SetupPath(const std::string& strDirectory);

	bool Initialize();

	bool UpdateToLatest();
	void AsyncUpdateToLatest();

	void Stop();
	bool IsStopping();

	void MakeLog();

	bool UnpackArchive(
		const std::string& file, 
		std::set<size_t>& sUnpackedOffsets, 
		CArchive& archive, 
		CArchiveFile* pFilePart);

	int UnpackArchiveFiles(std::vector<CArchiveFile*>& files);

	bool MTUnpackSingleArchiveFile(
		CArchiveFile* pArchive, 
		boost::atomic_int32_t& result);

	bool MTUnpackArchiveFiles(
		boost::iterator_range<std::vector<CArchiveFile*>::iterator> archive, 
		boost::atomic_int32_t& result);

private:
	bool FetchFileInfo(
		const std::string& file, 
		CArchiveDirectoryFile* pInfoFileDirectory);

	int HTTPDownloadFile(
		const std::string& link, 
		const std::string& filename, 
		bool bCheckCompleted, 
		bool bFlush);

	bool HTTPValid(boost::asio::streambuf& response);
	bool HTTPDownloadManifest(const std::string link);

	bool ReadConfiguration();
	bool GetCurrentVersion();

	inline int Patch(
		std::vector<CArchiveFile*>& files, 
		CArchiveFile** m_ppPatchFile);

	inline std::string TmpPath(std::string s)
	{
		s.insert(s.begin(), '\\');
		s.insert(s.begin(), m_strTempPath.begin(), m_strTempPath.end());
		
		return s;
	}

	inline std::string GamePath(std::string s)
	{
		s.insert(s.begin(), '\\');
		s.insert(s.begin(), m_strGamePath.begin(), m_strGamePath.end());

		return s;
	}
};

extern CPatcher * GetPatcher();
extern CPatcher * CreatePatcher(Callback::CPatchCallback& callback);

#endif