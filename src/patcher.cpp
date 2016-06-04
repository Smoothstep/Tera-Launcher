
#include "patcher.h"

#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/compare.hpp>

#include <boost/algorithm/string_regex.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/optional.hpp>
#include <boost/assert.hpp>

#include <boost/serialization/set.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <set>

#include "Launcher.h"
#include "tera_zip_format.h"
#include "torrent.h"

#define endl ""
#define P(x) (char*)(x)

#define GIGABYTE 1073741824

template<class C, class T>
static __forceinline C Cast(T& value)
{
	C ret;
	
#ifndef BOOST_NO_EXCEPTIONS
	try
	{
		ret = boost::lexical_cast<C>(value);
#else
		ret = boost::lexical_cast<C>(value);
#endif
#ifndef BOOST_NO_EXCEPTIONS
	}
	catch (const boost::bad_lexical_cast& excp)
	{
		std::cerr << excp.what();

		BOOST_ASSERT_MSG(false, static_cast<std::stringstream&>(std::stringstream() << 
			"Unable to cast to " << typeid(C).name() << " from " << typeid(T).name() << " : " << excp.what()));

		ret = C();
	}
#endif

	return ret;
}

CPatcher::CPatcher(Callback::CPatchCallback& callback) :
	m_Callback(callback),
	m_HTTPSocket(m_HTTPService),
	m_bStop(false)
{
}

CPatcher::~CPatcher()
{
	MakeLog();

	if (m_DownloadBuffer)
	{
		delete[] m_DownloadBuffer;
		m_DownloadBuffer = NULL;
	}
}

bool CPatcher::Initialize()
{
	if (!ReadConfiguration())
	{
		alert(m_Callback,"Fatal: Could not initialize Patcher." << endl);
		return false;
	}

	if (!HTTPDownloadManifest(m_strDownloadManifest))
	{
		alert(m_Callback,"Fatal: Could not download manifest." << endl);
		return false;
	}

	m_iMyVersion = m_Manifest.GetReleaseByVersion(m_strVersion);

	if (m_iMyVersion == -1)
	{
		alert(m_Callback,"Info: Expecting a new client." << endl);
	}

	return true;
}

bool CPatcher::FetchFileInfo(std::string file, CArchiveDirectoryFile * pInfoFileDirectory)
{
	int32_t error;

	if (!pInfoFileDirectory)
	{
		alert(m_Callback, "Error: Invalid directory file: " << file);
		return false;
	}

	FILE* f = NULL;
	fopen_s(&f, file.c_str(), "rb");

	if (f == NULL)
	{
		alert(m_Callback, "Error: File does not exist: " << file);
		return false;
	}

	SEEK(f, 0, SEEK_END);

	uint64_t iEnd = TELL(f);
	uint64_t iSize = pInfoFileDirectory->GetHeaderRef().compressed_size + pInfoFileDirectory->GetHeaderRef().extra_field_len + 
		pInfoFileDirectory->GetHeaderRef().filename_len + ARC_FILE_SIZE - sizeof(int);

	if (iEnd < iSize)
	{
		fclose(f);

		alert(m_Callback, "Error: Impossible directory file: " << file);
		return false;
	}

	if (SEEK(f, iEnd - iSize, SEEK_SET) == EOF)
	{
		fclose(f);

		alert(m_Callback, "Error: Impossible directory file: " << file);
		return false;
	}

	CArchiveFile archiveFile(-1);

	if ((error = CArchive::LoadFile(f, &archiveFile)) != kSuccess)
	{
		fclose(f);

		alert(m_Callback, "Error: Unable to load info file: " << file << " Message: " << ErrorMessage(error));
		return false;
	}

	fclose(f);

	if ((error = archiveFile.Decompress()) != kSuccess)
	{
		alert(m_Callback, "Error: Could not decompress info file: " << file << " Message: " << ErrorMessage(error));
		return false;
	}

	CArchive archive;

	if ((error = archive.MEMLoadArchive(P(&archiveFile.GetDecompressedData()[0]), archiveFile.GetDecompressedData().size(), false)) != kSuccess)
	{
		alert(m_Callback, "Error: Failed to load decompressed info file: " << file << " Message: " << ErrorMessage(error));
		return false;
	}

	if ((error = archive.DecompressAll()) != kSuccess)
	{
		alert(m_Callback, "Error: Failed to decompress decompressed info file: " << file << " Message: " << ErrorMessage(error));
		return false;
	}

	if (!m_FileInfo.LoadFileInfo(P(&archive.GetArchiveFiles()[0]->GetDecompressedData()[0]), archive.GetArchiveFiles()[0]->GetDecompressedData().size()))
	{
		alert(m_Callback, "Error: Could not load info file: " << file << " Message: " << m_FileInfo.GetLastErrorMessage());
		return false;
	}

	return true;
}

bool CPatcher::HTTPValid(boost::asio::streambuf& response)
{
	std::istream response_stream(&response);

	std::string http_version;
	{
		response_stream >> http_version;
	}

	uint32_t status_code;
	{
		response_stream >> status_code;
	}

	std::string status_message;
	{
		std::getline(response_stream, status_message);
	}

	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		alert(m_Callback, "Error: Invalid response" << endl);
		return false;
	}

	if (status_code != 200 && status_code != 206)
	{
		alert(m_Callback, "Error: Response returned with status code " << status_code << endl);
		return false;
	}

	return true;
}

int CPatcher::HTTPDownloadFile(std::string link, std::string filename, bool bCheckCompleted, bool bFlush)
{
	uint64_t size, currentSize, fileSize;

	boost::filesystem::path pFile(m_strTempPath);
	boost::filesystem::path pInfo(m_strTempPath);

	pFile += "\\" + filename;
	pInfo += "\\" + filename + ".info";

	fileSize = 0;

	if (boost::filesystem::exists(pInfo))
	{
		boost::filesystem::ifstream ifTmpInfoFile(pInfo);

		if (!ifTmpInfoFile.is_open())
		{
			alert(m_Callback,"Error: Could not open temporary info file: " << pInfo << endl);
			return 0;
		}

		for (std::string line; std::getline(ifTmpInfoFile, line);)
		{
			boost::trim(line);

			if (bCheckCompleted && boost::starts_with(line, "Completed"))
			{
				size_t iPos = line.find_last_of("=");

				if (iPos)
				{
					int iCompleted = Cast<int>(line.substr(iPos + 1));

					if (iCompleted)
					{
						return 2;
					}
				}
			}
			else if (boost::starts_with(line, "size"))
			{
				size_t iPos = line.find_first_of("=");

				if (iPos)
				{
					fileSize = Cast<uint64_t>(line.substr(iPos + 1));
				}
			}
		}

		ifTmpInfoFile.close();
	}

	std::ofstream ofTmpFile(pFile.string(), !bFlush ? 
		std::ofstream::out | std::ofstream::binary | std::ios_base::app : 
		std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);

	std::ofstream ofTmpInfoFile(pInfo.string(), std::ofstream::out | std::ofstream::binary);

	if (!ofTmpFile.is_open() || !ofTmpInfoFile.is_open())
	{
		alert(m_Callback, "Error: Could not open tmp file: " << filename << endl);
		return 0;
	}

	ofTmpFile.seekp(0, std::ios::end);
	{
		currentSize = size = ofTmpFile.tellp();

		if (currentSize)
		{
			m_Callback.EnqueAlert(new TADownload(currentSize, -1));
		}
	}

	if (fileSize)
	{
		if (fileSize == currentSize)
		{
			alert(m_Callback,"Info: File already fully downloaded: " << filename << endl);
			return 1;
		}
	}

	ofTmpInfoFile << "link=" << link << "\n";

	boost::system::error_code error;
	
	m_HTTPSocket.close();
	{
		boost::asio::ip::tcp::resolver				resolver(m_HTTPService);
		boost::asio::ip::tcp::resolver::query		query(m_strDownloadHost, "http");
		boost::asio::ip::tcp::resolver::iterator	endpoint_iterator = resolver.resolve(query, error);
		boost::asio::ip::tcp::resolver::iterator	end;

		if (error)
		{
			alert(m_Callback,"Error: Could not resolve download host: " << m_strDownloadHost << endl);
			return 0;
		}

		for (error = boost::asio::error::host_not_found; error && endpoint_iterator != end; )
		{
			if (m_HTTPSocket.is_open())
			{
				m_HTTPSocket.close();
			}

			m_HTTPSocket.connect(*endpoint_iterator++, error);
		}
	}

	if (error)
	{
		alert(m_Callback,"Error: Could not connect to gameforge download server." << endl);
		return 0;
	}

	boost::asio::streambuf request;

	std::ostream request_stream(&request);

	request_stream << "GET "	<< link				<< " HTTP/1.1\r\n";
	request_stream << "Host: "	<< m_strDownloadHost<< "\r\n";

	if (size)
	{
		request_stream << "Range: bytes=" << size << "-\r\n";
	}

	request_stream << "Accept: */*\r\n";
	request_stream << "Connection: close\r\n\r\n";

	boost::asio::write(m_HTTPSocket, request, error);

	if (error)
	{
		if (m_bStop)
		{
			return 0;
		}

		alert(m_Callback,"Error: Could not send request." << endl);
		return 0;
	}

	boost::asio::streambuf response;
	size_t iHeaderEnd = boost::asio::read_until(m_HTTPSocket, response, "\r\n\r\n", error);

	if (error)
	{
		if (m_bStop)
		{
			return 0;
		}

		alert(m_Callback,"Error: Could not read response: " << error.message() << endl);
		return 0;
	}

	if (!HTTPValid(response))
	{
		return 0;
	}

	std::string strResponse(P(boost::asio::detail::buffer_cast_helper(response.data())), 
		boost::asio::detail::buffer_size_helper(response.data()));
	{
		std::vector<std::string> vHeader;
		{
			boost::split(vHeader, strResponse, boost::is_any_of("\r\n"));
		}

		size_t iPos = std::string::npos;

		for (auto it = vHeader.begin(); it != vHeader.end(); ++it)
		{
			iPos = it->find("Content-Length:");

			if (iPos != it->npos)
			{
				iPos += strlen("Content-Length:");

				if (it->size() > iPos + 1)
				{
					size += Cast<uint64_t>(it->substr(iPos + 1));
					{
						ofTmpInfoFile << "size=" << size << "\n";
					}
				}
				else
				{
					alert(m_Callback, "Error: Received invalid response" << endl);
					return 0;
				}

				break;
			}
		}

		if (iPos == -1)
		{
			alert(m_Callback, "Error: Received invalid response" << endl);
			return 0;
		}
	}

	size_t iPos = strResponse.find("\r\n\r\n");

	if (iPos == -1)
	{
		alert(m_Callback, "Error: Invalid response" << endl);
		return 0;
	}

	iPos += strlen("\r\n\r\n");

	if (strResponse.size() > iPos)
	{
		fileSize = strResponse.size() - iPos;
		ofTmpFile.write(strResponse.substr(iPos).data(), fileSize);
	}
	else
	{
		fileSize = 0;
	}

	while(!error)
	{
		m_CurrentBufferOffset += boost::asio::read(m_HTTPSocket, boost::asio::buffer(m_DownloadBuffer, m_DownloadBufferSize - m_CurrentBufferOffset), error);

		if (m_CurrentBufferOffset == m_DownloadBufferSize)
		{
			fileSize += m_CurrentBufferOffset;

			ofTmpFile.write(P(m_DownloadBuffer), m_CurrentBufferOffset);
			m_CurrentBufferOffset = 0;

			m_Callback.EnqueAlert(new TADownload(m_DownloadBufferSize, size - fileSize));
		}
	}

	if (error && error != boost::asio::error::eof)
	{
		if (m_bStop)
		{
			return 0;
		}

		alert(m_Callback,"Error: On HTTP download: " << error.message() << endl);
		return 0;
	}

	if (m_CurrentBufferOffset)
	{
		fileSize += m_CurrentBufferOffset;

		ofTmpFile.write(P(m_DownloadBuffer), m_CurrentBufferOffset);
		m_CurrentBufferOffset = 0;

		m_Callback.EnqueAlert(new TADownload(m_CurrentBufferOffset, size - fileSize));
	}

	if (fileSize != (size - currentSize))
	{
		assert(fileSize < (size - currentSize));

		alert(m_Callback,"Warning: Could not download file completely - Retrying in 1 minute." << endl);

		ofTmpFile.close();
		ofTmpInfoFile.close();

		boost::this_thread::sleep_for(boost::chrono::minutes(1));
		{
			m_HTTPService.post(boost::bind(&CPatcher::HTTPDownloadFile, this, link, filename, bCheckCompleted, bFlush));
			m_HTTPService.run_one();
		}

		return 1;
	}

	ofTmpInfoFile << "Completed=1\n";

	return 1;
}

bool CPatcher::MTUnpackSingleArchiveFile(CArchiveFile* pArchive, boost::atomic_int32_t& result)
{
	if (m_bStop)
	{
		return true;
	}

	int32_t iResult = result;

	if (iResult != kSuccess && iResult != kInsufficientMemory && iResult != kDeflateError)
	{
		return false;
	}

	int32_t iError = kInsufficientMemory;
	
	while (iError == kInsufficientMemory)
	{
		iError = pArchive->WriteDecompressed(m_strClientPath);
	}

	if (iError == kDeltaDecodeError)
	{
		if (m_FileInfo.CheckSizeValid(pArchive->GetFilePath(), pArchive->GetRelativePath()))
		{
			iError = kSuccess;
		}
	}

	pArchive->ClearAll();

	if (iError != kSuccess && iError != kInsufficientMemory)
	{
		alert(m_Callback, "Error: " << ErrorMessage(iError) << " File: " << pArchive->GetRelativePath());
		result = iError;
		return false;
	}

	pArchive->SetUnpacked(true);

	m_Callback.EnqueAlert(new TAFilePatch(pArchive->GetFilePath(), "Completed", pArchive->GetTrueSize()));

	return true;
}

bool CPatcher::MTUnpackArchiveFiles(boost::iterator_range<std::vector<CArchiveFile*>::iterator> archive, boost::atomic_int32_t & result)
{
	int32_t iResult = result;

	if (iResult != kSuccess && iResult != kInsufficientMemory && iResult != kDeflateError)
	{
		return false;
	}

	for (auto it = archive.begin(); it != archive.end() && !m_bStop; ++it)
	{
		CArchiveFile* pArchive = *it;

		int32_t iError = kInsufficientMemory;
		while (iError == kInsufficientMemory)
		{
			iError = pArchive->WriteDecompressed(m_strClientPath);
		}

		if (iError == kDeltaDecodeError)
		{
			if (m_FileInfo.CheckSizeValid(pArchive->GetFilePath(), pArchive->GetRelativePath()))
			{
				iError = kSuccess;
			}
		}

		pArchive->ClearAll();

		if (iError != kSuccess)
		{
			alert(m_Callback, "Error: " << ErrorMessage(iError) << " File: " << pArchive->GetRelativePath());
			result = iError;
			return false;
		}

		pArchive->SetUnpacked(true);

		m_Callback.EnqueAlert(new TAFilePatch(pArchive->GetFilePath(), "Completed", pArchive->GetTrueSize()));
	}

	return true;
}

inline int CPatcher::Patch(std::vector<CArchiveFile*>& files, CArchiveFile ** m_ppPatchFile)
{
	boost::atomic_int32_t iResult(kSuccess);
	size_t iThreadCount = m_PatchService.ThreadCount();

	if (m_PatchService.ThreadCount())
	{
		if (files.size() >= m_PatchService.ThreadCount() * m_PatchService.ThreadCount())
		{
			for (size_t i = 0; i < files.size(); i += iThreadCount)
			{
				m_PatchService.Work(boost::bind(&CPatcher::MTUnpackArchiveFiles, this,
					boost::make_iterator_range(files.begin() + i, files.begin() + i + std::min(iThreadCount, files.size() - i)), boost::ref(iResult)));
			}
		}
		else
		{
			for (TArchiveFiles::iterator it = files.begin(); it != files.end(); ++it)
			{
				m_PatchService.Work(boost::bind(&CPatcher::MTUnpackSingleArchiveFile, this, *it, boost::ref(iResult)));
			}
		}

		m_PatchService.JoinAll();
	}
	else
	{
		for (TArchiveFiles::iterator it = files.begin(); it != files.end(); ++it)
		{
			MTUnpackSingleArchiveFile(*it, boost::ref(iResult));
		}
	}
	
	return iResult;
}

bool LoadUnpackedFileOffsets(std::set<size_t>& set, std::string file)
{
	std::ifstream fFileData(file + ".dat", std::ios::binary);

	if (!fFileData.is_open())
	{
		fFileData.open(file + ".dat", std::ios::binary | std::ios::trunc);

		if (!fFileData.is_open())
		{
			return true;
		}
	}

	boost::archive::binary_iarchive archiveData(fFileData);
	archiveData & set;

	return true;
}

bool SaveUnpackedFileOffsets(CArchive& archive, std::set<size_t>& set, std::string file)
{
	std::ofstream fFileData(file + ".dat", std::ios::binary | std::ios::trunc);

	if (!fFileData.is_open())
	{
		return false;
	}

	TArchiveFiles& files = archive.GetArchiveFiles();

	for (TArchiveFiles::iterator it = files.begin(); it != files.end(); ++it)
	{
		CArchiveFile* pFile = *it;

		if (pFile->IsUnpacked())
		{
			set.insert(pFile->GetFileArchiveOffset());
		}
	}

	boost::archive::binary_oarchive archiveData(fFileData);
	archiveData & set;

	return true;
}

bool CPatcher::UnpackArchive(std::string file, std::set<size_t>& sUnpackedOffsets, CArchive& archive, CArchiveFile* pFilePart)
{
	boost::atomic_int32_t iResult(kSuccess);
	CArchiveFile* m_pVersionFile = NULL;
	size_t iThreadCount = m_PatchService.ThreadCount();

	int32_t iError;
	for (	iError = archive.LoadArchive(file.c_str(), &sUnpackedOffsets, pFilePart); 
			iError == kInsufficientMemory; 
			iError = archive.LoadArchive(file.c_str(), &sUnpackedOffsets, pFilePart))
	{
		if (m_bStop)
		{
			return false;
		}

		pFilePart = NULL;

		if (archive.GetArchiveFiles().empty())
		{
			if (iError == kInsufficientMemory)
			{
				alert(m_Callback, "Error: Not enough memory for single file: " 
					<< archive.GetLastFileRequiredSize() << "b. Allocated: " << GetAllocationSize() << "b." << endl);
 
				int32_t result = archive.UnpackNextArchiveFile(m_strClientPath, &sUnpackedOffsets, pFilePart);

				if (result != kSuccess)
				{
					alert(m_Callback, "Error: Impossible to unpack with current memory availability: " << ErrorMessage(result));
					return false;
				}

				continue;
			}

			if (CArchive::IsMainFile(file.c_str()))
			{
				return true;
			}

			alert(m_Callback,"Error: Could not find any files in archive: " << file << endl);
			return false;
		}

		TArchiveFiles& files = archive.GetArchiveFiles();
		{
			files.Sort();
		}

		alert(m_Callback, "Info: Could not allocate more memory. Trying to unpack " << files.size() << " files." << endl);

		int32_t iResult = Patch(files, &m_pVersionFile);

		for (TArchiveFiles::iterator it = files.begin(); it != files.end(); ++it)
		{
			CArchiveFile* pFile = *it;

			if(pFile->IsUnpacked())
			{
				sUnpackedOffsets.insert(pFile->GetFileArchiveOffset());
			}
		}

		if (iResult != kSuccess)
		{
			alert(m_Callback, "Error: Could not unpack archive: " << file << ". Error: " << ErrorMessage(iResult) << endl);
			return false;
		}

		archive.Clear();
	}

	if (iError != kInsufficientMemory && iError != kSuccess && iError != kFilePart)
	{
		alert(m_Callback, "Error: Could not load archive: " << file << ". Error: " << ErrorMessage(iError) << endl);
		return false;
	}

	TArchiveFiles& files = archive.GetArchiveFiles();
	{
		files.Sort();
	}

	if (files.empty())
	{
		if (iError == kFilePart)
		{
			return true;
		}

		if (CArchive::IsMainFile(file.c_str()))
		{
			return true;
		}

		// hack
		if (!sUnpackedOffsets.empty())
		{
			return true;
		}

		alert(m_Callback,"Empty archive: " << file << endl);
		return false;
	}

	alert(m_Callback,"Unpacking: " << files.size() << " files." << endl);

	if ((iError = Patch(files, &m_pVersionFile)) != kSuccess)
	{
		if (iError = kFilePart)
		{
			return true;
		}

		alert(m_Callback, "Error: Failed patching files: " << ErrorMessage(iError));
		return false;
	}

	if (m_pVersionFile)
	{
		if ((iError = m_pVersionFile->WriteDecompressed(m_strClientPath)) != kSuccess)
		{
			alert(m_Callback, "Error: Unable to unpack version file: " << ErrorMessage(iError));
			return false;
		}
	}

	return true;
}

bool CPatcher::UpdateToLatest()
{
	int32_t iError = kSuccess;

	std::vector<int> vReleasesNeeded;
	if (!m_Manifest.GetReleasesToDownloadFor(m_iMyVersion, m_Manifest.GetLatestVersion(), vReleasesNeeded))
	{
		fatal(m_Callback, "Error: " << m_Manifest.LastError());
		return false;
	}

	if (vReleasesNeeded.empty())
	{
		m_Callback.EnqueAlert(new TAPatchFinish);

		fatal(m_Callback,"Info: Your version is up to date: " << m_iMyVersion << endl);
		return true;
	}

	for (std::vector<int>::iterator it = vReleasesNeeded.begin(); it != vReleasesNeeded.end(); ++it)
	{
		std::string sMetaPath;
		
		if (!m_Manifest.GetDownloadPathForRelease(m_iMyVersion, *it, sMetaPath))
		{
			fatal(m_Callback, "Error: " << m_Manifest.LastError());
			return false;
		}

		if (sMetaPath.empty())
		{
			fatal(m_Callback,"Error: Empty meta path for release: " << *it << endl);
			return false;
		}

		std::string sPackage = sMetaPath.substr(sMetaPath.find_last_of("/") + 1);

		if (!HTTPDownloadFile(sMetaPath, sPackage, true, false))
		{
			fatal(m_Callback,"Error: HTTP download filed for metafile from: " << sMetaPath << endl);
			return false;
		}

		sPackage = TmpPath(sPackage);

		CArchive archive;

		if ((iError = archive.LoadArchive(sPackage.c_str())) != kSuccess)
		{
			fatal(m_Callback,"Error: Unable not load metafile: " << ": " << ErrorMessage(iError) << endl);
			return false;
		}

		if ((iError = archive.DecompressAll()) != kSuccess)
		{
			fatal(m_Callback,"Error: Unable not decompress metafile: " << sPackage << ": " << ErrorMessage(iError) << endl);
			return false;
		}

		archive.WriteFiles(false);

		TArchiveFiles files = archive.GetArchiveFiles();

		if (files.empty())
		{
			fatal(m_Callback,"Error: Invalid meta archive file: " << sPackage << endl);
			return false;
		}

		CTorrentData torrentData;

		if (!torrentData.DecodeMetadata(P(&files[0]->GetDecompressedData()[0]), files[0]->GetDecompressedData().size()))
		{
			fatal(m_Callback,"Error: Unable to BDecode metadata." << endl);
			return false;
		}

		m_Callback.EnqueStaticAlert(new TADownloadBegin(torrentData.TotalSize()));

		TFiles torrentFiles = torrentData.GetFileStorage();
		{
			std::rotate(torrentFiles.begin(), torrentFiles.begin() + torrentFiles.size() - 1, torrentFiles.begin() + torrentFiles.size());
		}

		for (TFiles::iterator it = torrentFiles.begin(); it != torrentFiles.end(); ++it)
		{
			alert(m_Callback,"Info: Queued file to download: " << it->name << endl);
		}

		for (TFiles::iterator it = torrentFiles.begin(); it != torrentFiles.end(); ++it)
		{
			size_t iPos = it->name.find_last_of(".");

			if (iPos == it->name.npos)
			{
				alert(m_Callback,"Warning: Invalid file format: " << it->name << endl);
				continue;
			}

			std::string sFile = it->name;

			if (sFile.empty())
			{
				alert(m_Callback,"Warning: Empty file." << endl);
				continue;
			}

			std::string sUrl = "http://" + m_strDownloadClient + sFile.substr(0, iPos) + "/" + sFile;

			int iResult = 0;

			if (!(iResult = HTTPDownloadFile(sUrl, it->name, true, false)))
			{
				fatal(m_Callback,"Error: Failed when downloading: " << sUrl << endl);
				return false;
			}

			// Already finished.
			if (iResult == 2)
			{
				m_Callback.EnqueAlert(new TADownload(it->size, -1));
			}

			alert(m_Callback,"Info: Finished downloading: " << it->name << endl);
		}

		CArchiveFile* pFilePart = NULL;

		for (TFiles::iterator it = torrentFiles.begin(); it != torrentFiles.end(); ++it)
		{
			std::string sTmpPath = TmpPath(it->name);

			if (m_bStop)
			{
				m_Callback << "Warning: Stopped unpacking." << endl;
				return false;
			}

			std::set<size_t> sUnpackedOffsets;

			if (!LoadUnpackedFileOffsets(sUnpackedOffsets, sTmpPath))
			{
				fatal(m_Callback, "Error: Unable to open: " << sTmpPath << ".dat" << endl);
				return false;
			}

			if (!sUnpackedOffsets.empty())
			{
				m_Callback.EnqueAlert(new TAMultiFilePatch(sUnpackedOffsets.size()));
			}

			CArchive archive;

			if (!UnpackArchive(sTmpPath, sUnpackedOffsets, archive, pFilePart))
			{
				fatal(m_Callback,"Error: Could not unpack archive file: " << it->name << endl);
				return false;
			}

			if (!SaveUnpackedFileOffsets(archive, sUnpackedOffsets, sTmpPath))
			{
				fatal(m_Callback, "Error: Unable to save: " << sTmpPath << ".dat" << endl);
				return false;
			}

			if (CArchive::IsMainFile(it->name.c_str()))
			{
				m_Callback.EnqueStaticAlert(new TAFilePatchBegin(archive.GetArchiveDirectoryFiles().size()));

				CArchiveDirectoryFile* pInfoFileDirectory = archive.GetArchiveDirectoryFileByName("pgc_v2.version", false);

				if (pInfoFileDirectory)
				{
					if (!FetchFileInfo(TmpPath(torrentFiles[torrentFiles.size() - 1].name), pInfoFileDirectory))
					{
						alert(m_Callback, "Error: Unable to fetch pgc_v2.version." << endl);
						return false;
					}
				}
				else
				{
					alert(m_Callback, "Error: Corrupted archive. No pgc_v2.version found in archive." << endl);
					return false;
				}
			}

			pFilePart = archive.GetPartFile();
			archive.Clear();
		}

		boost::system::error_code error;

		// hack
		if (boost::filesystem::exists(m_strClientPath + "\\version.ini.version", error) && !error)
		{
			boost::filesystem::remove(m_strClientPath + "\\version.ini", error);

			if (!error)
			{
				boost::filesystem::rename(m_strClientPath + "version.ini.version", "version.ini", error);
			}
		}

		if (error)
		{
			alert(m_Callback, "Error: Unable to find and rename: version.ini & version.ini.version");
			return false;
		}

		m_iMyVersion = *it;
	}

	m_Callback.EnqueAlert(new TAPatchFinish);

	return true;
}

void CPatcher::AsyncUpdateToLatest()
{
	m_bStop = false;
	m_PatchThread = boost::thread(boost::bind(&CPatcher::UpdateToLatest, this));
}

void CPatcher::Stop()
{
	m_bStop = true;

	if (m_HTTPSocket.is_open())
	{
		m_HTTPSocket.close();
	}

	m_HTTPService.stop();

	if (m_PatchThread.joinable())
	{
		m_PatchThread.join();
	}

	m_bStop = false;
}

bool CPatcher::IsStopping()
{
	return m_bStop;
}

void CPatcher::MakeLog()
{
	Callback::TAlertsCopy alerts;
	m_Callback.GetAlertsCopy(alerts);

	for (Callback::TAlertsCopy::iterator it = alerts.begin(); it != alerts.end(); ++it)
	{
		Callback::CAlert *alert = *it;
		std::cerr << alert->Message();
	}
}

bool CPatcher::HTTPDownloadManifest(std::string link)
{
	if (HTTPDownloadFile(m_strDownloadManifest, "manifest.patchmanifest", false, true))
	{
		int32_t iError = kSuccess;

		std::string strManifestPath(m_strTempPath + "\\manifest.patchmanifest");

		CArchive archive;

		if ((iError = archive.LoadArchive(strManifestPath.c_str())) != kSuccess)
		{
			alert(m_Callback,"Could not open manifest file as archive: " << ErrorMessage(iError) << endl);
			return false;
		}

		if ((iError = archive.DecompressAll()) != kSuccess)
		{
			alert(m_Callback,"Could not decompress manifest file: " << ErrorMessage(iError) << endl);
			return false;
		}

		if (!m_Manifest.LoadManifestFile((char*)archive.GetArchiveFiles()[0]->GetDecompressedData().data(), archive.GetArchiveFiles()[0]->GetDecompressedData().size()))
		{
			alert(m_Callback,"Could not parse manifest file." << endl);
			return false;
		}

		return true;
	}

	return false;
}

bool CPatcher::ReadConfiguration()
{
	boost::filesystem::path pCurrent = boost::filesystem::current_path();

	pCurrent += "\\config\\patcher.ini";

	if (!boost::filesystem::exists(pCurrent))
	{
		alert(m_Callback,"Error: Could not find patcher.ini: \\config\\patcher.ini" << endl);
		return false;
	}

	boost::filesystem::ifstream ifConfig(pCurrent, std::ios_base::in);

	if (!ifConfig.is_open())
	{
		alert(m_Callback,"Error: Could not open patcher.ini" << endl);
		return false;
	}

	for (std::string line; std::getline(ifConfig, line); )
	{
		boost::trim(line);

		if (boost::starts_with(line, "buffersize"))
		{
			size_t iPos = line.find_last_of("=");

			if(iPos != line.npos && line.size() > iPos + 1)
			{
				m_DownloadBufferSize = Cast<uint32_t>(line.substr(iPos + 1));

				if (m_DownloadBufferSize < 1024)
				{
					alert(m_Callback,"Error: Download buffer size may not be less than 1 kB" << endl);
					return false;
				}
			}
			else
			{
				alert(m_Callback,"Error: No download buffer size found" << endl);
				return false;
			}
		}
		else if (boost::starts_with(line, "tera_game_path"))
		{
			size_t iPos = line.find_last_of("=");

			if (iPos != line.npos && line.size() > iPos + 1)
			{
				m_strGamePath = line.substr(iPos + 1);

				if (!boost::filesystem::exists(m_strGamePath))
				{
					alert(m_Callback,"Error: Could not find game path" << endl);
					return false;
				}

				m_strClientPath = m_strGamePath + "\\Client";
			}
			else
			{
				alert(m_Callback,"Error: No game path given" << endl);
				return false;
			}
		}
		else if (boost::starts_with(line, "download_host_url"))
		{
			size_t iPos = line.find_last_of("=");

			if (iPos != line.npos && line.size() > iPos + 1)
			{
				m_strDownloadHost = line.substr(iPos + 1);
			}
			else
			{
				alert(m_Callback, "Error: Invalid download host url" << endl);
				return false;
			}
		}
		else if (boost::starts_with(line, "download_client_url"))
		{
			size_t iPos = line.find_last_of("=");

			if (iPos != line.npos && line.size() > iPos + 1)
			{
				m_strDownloadClient = line.substr(iPos + 1);
			}
			else
			{
				alert(m_Callback, "Error: Invalid download client url" << endl);
				return false;
			}
		}
		else if (boost::starts_with(line, "download_manifest_url"))
		{
			size_t iPos = line.find_last_of("=");

			if (iPos != line.npos && line.size() > iPos + 1)
			{
				m_strDownloadManifest = line.substr(iPos + 1);
			}
			else
			{
				alert(m_Callback, "Error: Invalid download manifest url" << endl);
				return false;
			}
		}
		else if (boost::starts_with(line, "download_client_url"))
		{
			size_t iPos = line.find_last_of("=");

			if (iPos != line.npos && line.size() > iPos + 1)
			{
				m_strDownloadClient = line.substr(iPos + 1);
			}
			else
			{
				alert(m_Callback, "Error: Invalid download client url" << endl);
				return false;
			}
		}
		else if (boost::starts_with(line, "temp_file_path"))
		{
			size_t iPos = line.find_last_of("=");

			if (iPos != line.npos && line.size() > iPos + 1)
			{
				m_strTempPath = line.substr(iPos + 1);

				if (!boost::filesystem::exists(m_strTempPath))
				{
					try
					{
						if (!boost::filesystem::create_directories(m_strTempPath))
						{
							alert(m_Callback,"Error: Unable to create temp file directory" << endl);
							return false;
						}
					}
					catch (const boost::system::error_code& ec)
					{
						alert(m_Callback, "Error: Unable to create temp file directory: " << ec.message() << endl);
						return false;
					}
				}
			}
			else
			{
				alert(m_Callback,"Error: No temp file path given" << endl);
				return false;
			}
		}
		else if(boost::starts_with(line, "worker_thread_count"))
		{
			size_t iPos = line.find_last_of("=");

			if (iPos != line.npos && line.size() > iPos + 1)
			{
				m_PatchService.SetupPatchThreads(Cast<int>(line.substr(iPos + 1)));
			}
		}
		else if (boost::starts_with(line, "memory_usage"))
		{
			size_t iPos = line.find_last_of("=");

			if (iPos != line.npos && line.size() > iPos + 1)
			{
				SetAllocationLimit(Cast<size_t>(line.substr(iPos + 1)));
			}

			if (GetAllocationLimit() < GIGABYTE)
			{
				alert(m_Callback, "Error: Allocation limit may not less than 1 Gb" << endl);
				return false;
			}
		}
	}

	if (m_DownloadBufferSize == 0)
	{
		alert(m_Callback,"Warning: No download buffer size given. Using 1 Mb" << endl);
		m_DownloadBufferSize = 1024 * 1024;
	}

	if (m_strDownloadClient.empty())
	{
		alert(m_Callback, "Info: No download_client_url specification" << endl);
		m_strDownloadClient = g_strDownloadClient;
	}

	if (m_strDownloadManifest.empty())
	{
		alert(m_Callback, "Info: No download_manifest_url specification" << endl);
		m_strDownloadManifest = g_strDownloadManifest;
	}

	if (m_strDownloadHost.empty())
	{
		alert(m_Callback, "Info: No download_host_url specification" << endl);
		m_strDownloadHost = g_strDownloadHost;
	}

	if (m_strGamePath.empty() || !boost::filesystem::exists(m_strGamePath))
	{
		alert(m_Callback,"Info: Invalid or no game path in config file (game_path). Choosing current directory as game path" << endl);
		m_strGamePath = boost::filesystem::current_path().string();

		if (!boost::filesystem::exists(m_strGamePath))
		{
			alert(m_Callback,"Error: Current path invalid" << endl);
			return false;
		}
	}

	if (m_strTempPath.empty() || !boost::filesystem::exists(m_strTempPath))
	{
		alert(m_Callback,"Info: Invalid or no temp path in config file. Choosing \temp as temp path" << endl);
		m_strTempPath = boost::filesystem::current_path().string() + "\\temp";

		if (!boost::filesystem::exists(m_strTempPath))
		{
			alert(m_Callback,"Error: Current path invalid" << endl);
			return false;
		}

		return false;
	}

	m_DownloadBuffer = (uint8_t*)malloc(m_DownloadBufferSize);

	if (!m_DownloadBuffer)
	{
		alert(m_Callback, "Error: Could not allocate " << m_DownloadBufferSize << " bytes for download buffer" << endl);
		return false;
	}

	if (!m_PatchService.ThreadCount())
	{
		m_PatchService.SetupPatchThreads(NUM_DEFAULT_PATCH_THREADS);
	}

	return GetCurrentVersion();
}

bool CPatcher::GetCurrentVersion()
{
	boost::filesystem::path pVersion(m_strGamePath);

	pVersion += "\\client\\version.ini";

	if (!boost::filesystem::exists(pVersion))
	{
		alert(m_Callback,"Warning: Could not find version file. Expecting a new client" << endl);
		return true;
	}

	boost::filesystem::ifstream ifConfig(pVersion, std::ios_base::in);

	if (!ifConfig.is_open())
	{
		alert(m_Callback,"Error: Could not open version file!" << endl);
		return false;
	}

	for (std::string line; std::getline(ifConfig, line); )
	{
		boost::trim(line);

		if (boost::starts_with(line, "value"))
		{
			size_t iPos = line.find_last_of("=");

			if (iPos != 0)
			{
				m_strVersion = line.substr(iPos + 2);
				m_strVersion.resize(m_strVersion.size() - 1);
			}
		}
	}

	if (m_strVersion.empty())
	{
		alert(m_Callback,"Error: Could not parse version from version file" << endl);
		return false;
	}

	return true;
}

static CPatcher* g_pPatcher = NULL;

CPatcher * GetPatcher()
{
	return g_pPatcher;
}

CPatcher * CreatePatcher(Callback::CPatchCallback& callback)
{
	g_pPatcher = new CPatcher(callback);

	if (!g_pPatcher)
	{
		return NULL;
	}

	if (!g_pPatcher->Initialize())
	{
		delete g_pPatcher;
		return g_pPatcher = NULL;
	}

	return g_pPatcher;
}