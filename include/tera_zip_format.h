#ifndef _TERA_ZIP_FORMAT_H_
#define _TERA_ZIP_FORMAT_H_

#pragma once
#pragma warning(disable: 4005)

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

extern "C"
{
#define XD3_WIN32 1
#define BREAK _asm int 3

#include <xdelta3.h>
#include <xdelta3-internal.h>

#define INCLUDECRYPTINGCODE_IFCRYPTALLOWED

#include "crypt.h"
}

#include <ktmw32.h>

#include "error_codes.h"
#include "allocation.h"
#include "mapped_file.h"

#pragma intrinsic( memset, memcpy, memcmp )

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma pack(1)

#define DYNAMIC_CRC_TABLE

#include <zlib.h>
#include <stdlib.h>

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)

#endif

#define MAX_FN_LEN _MAX_FNAME

#define ENCRYPT_HEADER_LENGTH 12
#define ZIP_VERSION 20

#define SAFE_ZIP
#define DIRECTORY_CHECK

// MAGIC
#define DELTA_ENCODED_FILE_SIGNATURE				0x00C4C3D6
#define ZIP_COMPRESSED_FILE_SIGNATURE				0x04034B50
#define ZIP_COMPRESSED_DIRECTORY_FILE_SIGNATURE		0x02014B50

#define DELTA_FILE(x)	*(uint32_t*)(x) == DELTA_ENCODED_FILE_SIGNATURE
#define ZIP_FILE(x)		*(uint32_t*)(x) == ZIP_COMPRESSED_FILE_SIGNATURE
#define ZIP_DIRE(x)		*(uint32_t*)(x) == ZIP_COMPRESSED_DIRECTORY_FILE_SIGNATURE

#define SEEK _fseeki64
#define TELL _ftelli64

typedef uint32_t file_size_t;

/*
*	https://users.cs.jmu.edu/buchhofp/forensics/formats/pkzip.html
*/
struct SArchiveFileHeader
{
	uint32_t signature;
	uint16_t version;
	uint16_t flags;
	uint16_t compression;
	uint16_t mod_time;
	uint16_t mod_date;
	uint32_t crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint16_t filename_len;
	uint16_t extra_field_len;
	uint8_t filename[MAX_FN_LEN];
	uint8_t extrafield[MAX_FN_LEN];
};

/*
*	https://users.cs.jmu.edu/buchhofp/forensics/formats/pkzip.html
*/
struct SArchiveDirectoryFileHeader
{
	uint32_t signature;
	uint16_t version;
	uint16_t version_needed;
	uint16_t flags;
	uint16_t compression;
	uint16_t mod_time;
	uint16_t mod_date;
	uint32_t crc32;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint16_t filename_len;
	uint16_t extra_field_len;
	uint16_t file_comment_len;
	uint16_t disk_start;
	uint16_t internal_addr;
	uint32_t external_addr;
	uint32_t offset_to_header;
	uint8_t filename[MAX_FN_LEN];
	uint8_t extrafield[MAX_FN_LEN];
};

#define ARC_DIRE_SIZE sizeof(SArchiveDirectoryFileHeader) - MAX_FN_LEN * 2
#define ARC_FILE_SIZE sizeof(SArchiveFileHeader) - MAX_FN_LEN * 2

constexpr uint8_t g_cPWXor = 0;

static int __fastcall GetPassword(char* szCrypted)
{
	unsigned int i = 0;
	unsigned char j = 0;

	if (!szCrypted || !szCrypted[0])
	{
		return 0;
	}

	do
	{
		if (i >= 119)
		{
			break;
		}

		int v = 8;

		for (j = g_cPWXor ^ szCrypted[i] + (1 << i); j > 126 && v > 0; v--)
		{
			j &= ~(1 << v);
		}

		if (j < 33)
		{
			j = (j | (1 << ((j % 3) + 5))) + 1;
		}

		szCrypted[i++] = j;
	} while (szCrypted[i]);

	szCrypted[i] = 0;

	return 1;
}

#define ARCHIVE_FILE_HEADER(x)				*(SArchiveFileHeader*)(x)
#define ARCHIVE_DIRECTORY_FILE_HEADER(x)	*(SArchiveDirectoryFileHeader*)(x)

#define MAX_PW_LENGTH 120

#define BUFFER_SIZE 65536

#define ALLOC(x)	Allocate(x)
#define FREE(x)		Deallocate(NULL, x)

#define MEM_NEW new (std::nothrow)

enum EFlags
{
	kEncrypted = 1 << 0,
	kLzmaEOS = 1 << 1,
	kDescriptorUsedMask = 1 << 3,
	kStrongEncrypted = 1 << 6,
	kUtf8 = 1 << 11,
	kImplodeDictionarySizeMask = 1 << 1,
	kImplodeLiteralsOnMask = 1 << 2,
	kDeflateTypeBitStart = 1,
	kNumDeflateTypeBits = 2,
	kNumDeflateTypes = (1 << kNumDeflateTypeBits),
	kDeflateTypeMask = (1 << kNumDeflateTypeBits) - 1
};

class CArchiveDirectoryFile
{
private:
	SArchiveDirectoryFileHeader m_Header;

protected:
	uint8_t m_Password[MAX_PW_LENGTH];

public:
	CArchiveDirectoryFile(uint8_t* data)
	{
		m_Header = ARCHIVE_DIRECTORY_FILE_HEADER(data);
	}

	CArchiveDirectoryFile()
	{
		memset(&m_Header, 0, sizeof(SArchiveDirectoryFileHeader));
		memset(m_Password, 0, MAX_PW_LENGTH);

		m_Header.signature = ZIP_COMPRESSED_DIRECTORY_FILE_SIGNATURE;
	}

	inline bool HasPassword()
	{
		return m_Header.extra_field_len > 4 && *(uint16_t*)&m_Header.extrafield == 0x8810;
	}

	inline size_t PasswordLength()
	{
		if (!HasPassword())
		{
			return 0;
		}

		size_t pwLen = *(uint16_t*)(&m_Header.extrafield[2]);
		return pwLen;
	}

	inline std::string GetRelativePath()
	{
		return std::string(m_Header.filename, m_Header.filename + m_Header.filename_len);
	}

	inline uint64_t GetHeaderSize()
	{
		return ARC_DIRE_SIZE + m_Header.extra_field_len + m_Header.filename_len;
	}

	inline bool ReadHeaderFromFile(FILE* f)
	{
		void* pBuffer = &m_Header.version;

		if (f)
		{
			if (fread_s(pBuffer, sizeof(m_Header), ARC_DIRE_SIZE - 4, 1, f))
			{
				return true;
			}
		}

		return false;
	}

	inline void ReadHeaderFromData(const char* pData)
	{
		memcpy(&m_Header.version, pData, ARC_DIRE_SIZE - 4);
	}

	inline bool IsZip()
	{
		SArchiveDirectoryFileHeader* h = &m_Header;
		return ZIP_DIRE(h);
	}

	inline bool Valid()
	{
		if (!IsZip())
		{
			return false;
		}

		return m_Header.version_needed = ZIP_VERSION && m_Header.compression == Z_DEFLATED;
	}

	inline SArchiveDirectoryFileHeader& GetHeaderRef()
	{
		return m_Header;
	}

	inline uint8_t* GetPassword()
	{
		return m_Password;
	}
};

typedef MEMVector<uint8_t> data_container_t;

class CArchiveFile
{
private:
	SArchiveFileHeader m_Header;

	data_container_t m_vCompressedData;
	data_container_t m_vDecompressedData;
	data_container_t m_vSourceData;

	bool m_bDecompressed;
	bool m_bUnpacked;

	std::string m_strFilePath;
	std::string m_strTempPath;

	file_size_t m_CurrentSize;
	size_t m_FileArchiveOffset;

public:
	CArchiveFile(size_t fileArchiveOffset) : 
		m_bDecompressed(false),
		m_bUnpacked(false),
		m_CurrentSize(0),
		m_FileArchiveOffset(fileArchiveOffset)
	{
		memset(&m_Header, 0, sizeof(SArchiveFileHeader));
		m_Header.signature = ZIP_COMPRESSED_FILE_SIGNATURE;
	}

	CArchiveFile(uint8_t* data, size_t fileArchiveOffset) :
		m_bDecompressed(false),
		m_bUnpacked(false),
		m_CurrentSize(0),
		m_FileArchiveOffset(fileArchiveOffset)
	{
		memset(&m_Header, 0, sizeof(SArchiveFileHeader));
		m_Header = ARCHIVE_FILE_HEADER(data);
		{
			Resize();
		}
	}

	~CArchiveFile()
	{
#ifdef SAFE_ZIP
		if (!m_strTempPath.empty())
		{
			boost::system::error_code error;
			boost::filesystem::remove(m_strTempPath, error);
		}
#endif
	}

	inline void SetFilePath(const std::string& path)
	{
		m_strFilePath = path;
	}

	inline void SetTempPath(const std::string& path)
	{
		m_strTempPath = path;
	}

	inline void SetUnpacked(bool b)
	{
		m_bUnpacked = b;
	}

	inline bool IsUnpacked()
	{
		return m_bUnpacked;
	}

	inline std::string GetRelativePath()
	{
		return std::string(m_Header.filename, m_Header.filename + m_Header.filename_len);
	}

	inline std::string& GetFilePath()
	{
		return m_strFilePath;
	}

	inline std::string& GetTempPath()
	{
		return m_strTempPath;
	}

	inline file_size_t GetRequiredSize()
	{
		return m_vCompressedData.size() - m_CurrentSize;
	}

	inline file_size_t GetSize()
	{
		return m_CurrentSize;
	}

	inline file_size_t GetTrueSize()
	{
		return m_CurrentSize + ARC_FILE_SIZE + m_Header.filename_len + m_Header.extra_field_len;
	}

	inline file_size_t GetHeaderSize()
	{
		return ARC_FILE_SIZE + m_Header.filename_len + m_Header.extra_field_len;
	}

	inline size_t GetFileArchiveOffset()
	{
		return m_FileArchiveOffset;
	}

	inline void ClearAll()
	{
		m_vCompressedData.clear();
		m_vDecompressedData.clear();
		m_vSourceData.clear();
	}

	inline bool ReadHeaderFromFile(FILE* f)
	{
		void* pBuffer = &m_Header.version;

		if (f)
		{
			if (fread_s(pBuffer, sizeof(m_Header), 1, ARC_FILE_SIZE - 4, f))
			{
				return true;
			}
		}

		return false;
	}

	inline void ReadHeaderFromData(const char* pData)
	{
		memcpy(&m_Header.version, pData, ARC_FILE_SIZE - 4);
	}

	inline bool ReadDataFromFile(FILE* f)
	{
		if (f)
		{
			return (m_CurrentSize += fread_s(
				&m_vCompressedData[m_CurrentSize], 
				m_vCompressedData.size() - m_CurrentSize, 1, 
				m_vCompressedData.size() - m_CurrentSize, f)) == m_vCompressedData.size();
		}

		return false;
	}

	inline void ReadDataFromData(const char* pData, size_t iSize)
	{
		memcpy(&m_vCompressedData[m_CurrentSize], pData, iSize);
		{
			m_CurrentSize += iSize;
		}
	}

	inline bool Resize()
	{
		if (!m_vCompressedData.resize(m_Header.compressed_size))
		{
			return false;
		}

		if (!m_vDecompressedData.resize(m_Header.uncompressed_size))
		{
			return false;
		}

		return true;
	}

	inline bool ResizeCompressed()
	{
		if (!m_vCompressedData.resize(m_Header.compressed_size))
		{
			return false;
		}

		return true;
	}

	inline bool IsZip()
	{
		SArchiveFileHeader* h = &m_Header;
		return ZIP_FILE(h);
	}

	inline bool Valid()
	{
		if (!IsZip())
		{
			return false;
		}

		return m_Header.version == ZIP_VERSION && m_Header.compression == Z_DEFLATED;
	}

	inline SArchiveFileHeader& GetHeaderRef()
	{
		return m_Header;
	}

	inline data_container_t& GetDecompressedData()
	{
		return m_vDecompressedData;
	}

	inline data_container_t& GetCompressedData()
	{
		return m_vCompressedData;
	}

	inline bool IsDecompressed()
	{
		return m_bDecompressed;
	}

	inline bool IsCrypted()
	{
		return (m_Header.flags & (kEncrypted)) != 0;
	}

	inline void SetDecompressed(bool b)
	{
		m_bDecompressed = b;
	}

	int32_t Decompress()
	{
		if (m_bDecompressed)
		{
			return kSuccess;
		}

		if (m_vDecompressedData.empty())
		{
			return kSuccess;
		}

		z_stream stream;

		stream.zalloc	= Z_NULL;
		stream.zfree	= Z_NULL;
		stream.opaque	= Z_NULL;

		int ret = inflateInit2(&stream, -15);

		if (ret != S_OK)
		{
			return kInflateError;
		}

		while (stream.total_out != m_vDecompressedData.size())
		{
			stream.next_in = &m_vCompressedData[stream.total_in];
			stream.next_out = &m_vDecompressedData[stream.total_out];
			stream.avail_in = m_vCompressedData.size() - stream.total_in;
			stream.avail_out = m_vDecompressedData.size() - stream.total_out;

			ret = inflate(&stream, Z_SYNC_FLUSH);

			if (ret == Z_BUF_ERROR)
			{
				break;
			}

			switch (ret)
			{
			case Z_NEED_DICT:

				ret = Z_DATA_ERROR;

			case Z_DATA_ERROR:
			case Z_MEM_ERROR:

				inflateEnd(&stream);
				return kInflateError;
			};
		}

		inflateEnd(&stream);

		m_bDecompressed = true;

		return kSuccess;
	}

	int32_t DeltaDecode()
	{
		uint8_t dstBuff[BUFFER_SIZE];

		if (!m_bDecompressed)
		{
			return kNotDecompressed;
		}

		FILE* fSrc = NULL;
		fopen_s(&fSrc, m_strFilePath.c_str(), "rb");

		if (!fSrc)
		{
			return kInvalidSource;
		}

		FILE* fDst = NULL;
		fopen_s(&fDst, m_strTempPath.c_str(), "wb");

		if (!fDst)
		{
			fclose(fSrc);
			return kInvalidFile;
		}

		xd3_config config;
		memset(&config, 0, sizeof(config));

		config.opaque		= dstBuff;
		config.iopt_size	= XD3_DEFAULT_IOPT_SIZE;
		config.sprevsz		= XD3_DEFAULT_SPREVSZ;
		config.smatch_cfg	= XD3_SMATCH_FASTER;
		config.winsize		= m_vDecompressedData.size();

		xd3_stream stream;
		memset(&stream, 0, sizeof(xd3_stream));

		if (xd3_config_stream(&stream, &config))
		{
			fclose(fDst);
			fclose(fSrc);

			return kDeltaDecodeError;
		}

		stream.free	= Deallocate;
		stream.alloc= Allocate;

		xd3_source source;
		memset(&source, 0, sizeof(xd3_source));

		source.name		= m_strFilePath.c_str();
		source.blksize	= usize_t(BUFFER_SIZE);
		source.curblkno = -1;
		source.curblk	= dstBuff;

		int32_t ret = xd3_set_source(&stream, &source);

		if (ret != 0)
		{
			fclose(fDst);
			fclose(fSrc);

			return kInvalidSource;
		}

		while (stream.total_in < m_vDecompressedData.size())
		{
			stream.next_in = &m_vDecompressedData[size_t(stream.total_in)];
			stream.avail_in = xd3_min(m_vDecompressedData.size() - size_t(stream.total_in), BUFFER_SIZE);

			if (stream.avail_in < BUFFER_SIZE)
			{
				stream.flags |= XD3_FLUSH;
			}

			ret = 0;

			while (ret != XD3_INPUT)
			{
				ret = xd3_decode_input(&stream);

				switch (ret)
				{
				case XD3_INPUT:

					break;

				case XD3_OUTPUT:

					if (fwrite(stream.next_out, 1, stream.avail_out, fDst) != size_t(stream.avail_out))
					{
						fclose(fSrc);
						fclose(fDst);

						return kInsufficientMemory;
					}

					xd3_consume_output(&stream);

					continue;

				case XD3_GETSRCBLK:

					if (SEEK(fSrc, source.blksize * source.getblkno, SEEK_SET) == EOF)
					{
						fclose(fSrc);
						fclose(fDst);

						return kEndOfFile;
					}

					source.onblk	= usize_t(fread_s(dstBuff, BUFFER_SIZE, 1, source.blksize, fSrc));
					source.curblkno = source.getblkno;

					continue;

				case XD3_GOTHEADER:
				case XD3_WINSTART:
				case XD3_WINFINISH:

					continue;

				default:
					fclose(fSrc);
					fclose(fDst);

					if (stream.next_out == NULL)
					{
						xd3_close_stream(&stream);
						xd3_free_stream(&stream);
						return kInsufficientMemory;
					}

					xd3_close_stream(&stream);
					xd3_free_stream(&stream);
					return kDeltaDecodeError;
				};
			}

		}

		fclose(fSrc);
		fclose(fDst);

		xd3_close_stream(&stream);
		xd3_free_stream(&stream);
		return kSuccess;
	}

	int32_t WriteDecompressed(
		boost::filesystem::path pCurrent = boost::filesystem::current_path())
	{
		boost::system::error_code error;

		if (!m_bDecompressed)
		{
			int32_t iError = Decompress();

			if (iError != kSuccess)
			{
				return iError;
			}
		}

		if (m_vDecompressedData.empty())
		{
			return kSuccess;
		}

		m_vCompressedData.clear();

		if (!pCurrent.is_absolute())
		{
			pCurrent = boost::filesystem::current_path().string() + "\\" + pCurrent.normalize().string();
		}

		if (boost::filesystem::is_directory(pCurrent, error))
		{
			if (error)
			{
				return kInvalidFile;
			}

			pCurrent += "\\";
			pCurrent += boost::filesystem::path((char*)m_Header.filename).normalize();
		}

		bool bDeltaDecode = false;

		if (boost::filesystem::exists(pCurrent, error))
		{
			bDeltaDecode = true;
		}

		m_strFilePath = pCurrent.string();
		m_strTempPath = pCurrent.string() + ".diff";

#ifdef DIRECTORY_CHECK
		if (error)
		{
			if (!boost::filesystem::exists(pCurrent.parent_path(), error))
			{
				if (!boost::filesystem::create_directories(pCurrent.parent_path(), error))
				{
					return kUnableToCreateDir;
				}
			}

			if (error)
			{
				return kUnableToCreateDir;
			}
		}
#endif

		if (DELTA_FILE(&m_vDecompressedData[0]))
		{
			if (!bDeltaDecode)
			{
				return kInvalidFile;
			}
		}
		else
		{
			bDeltaDecode = false;
		}

		if (bDeltaDecode)
		{
			int32_t iError;

			if ((iError = DeltaDecode()) != kSuccess)
			{
				boost::filesystem::remove(m_strTempPath, error);
				return iError;
			}

			if (!ReplaceFiles())
			{
				return kInvalidFile;
			}

			return kSuccess;
		}

		boost::filesystem::ofstream out(pCurrent, std::ios::binary);

		if (out.is_open())
		{
			out.clear();
			out.write((char*)&m_vDecompressedData[0], m_vDecompressedData.size());
			out.close();
		}
		else
		{
			return kWriteError;
		}

		return kSuccess;
	}

	bool ReplaceFiles()
	{
		if (GetRelativePath() != std::string("version.ini"))
		{
#ifdef BOOST_REPLACE_FILE
			boost::system::error_code error;

			boost::filesystem::copy_file(m_strTempPath, m_strFilePath,
				boost::filesystem::copy_option::overwrite_if_exists, error);

			if (error)
			{
				boost::filesystem::remove(m_strTempPath, error);
				return false;
			}

			boost::filesystem::remove(m_strTempPath, error);
#else
			HANDLE hTransaction = CreateTransaction(NULL, 0, 0, 0, 0, INFINITE, 0);

			if (!hTransaction)
			{
				return false;
			}

			if (!MoveFileTransactedA(m_strTempPath.c_str(), m_strFilePath.c_str(),
				0, 0, MOVEFILE_REPLACE_EXISTING, hTransaction))
			{
				RollbackTransaction(hTransaction);
				CloseHandle(hTransaction);

				return false;
			}

			return CommitTransaction(hTransaction);
#endif
		}
		else
		{
			boost::system::error_code error;
			boost::filesystem::rename(m_strTempPath, m_strFilePath + ".version", error);

			if (error)
			{
				return false;
			}
		}

		return true;
	}
};

template<typename T>
class CArchiveFileStorage : public std::vector<T*>
{
private:
	static inline bool SortBySize(T* p0, T* p1)
	{
		return p1->GetHeaderRef().uncompressed_size < p0->GetHeaderRef().uncompressed_size;
	}

public:
	void Sort()
	{
		std::sort(begin(), end(), CArchiveFileStorage::SortBySize);
	}
};

typedef CArchiveFileStorage<CArchiveFile>			TArchiveFiles;
typedef CArchiveFileStorage<CArchiveDirectoryFile>	TArchiveDirectoryFiles;

#define START_MAIN_ARCHIVE	0xA00
#define START_SPLIT_ARCHIVE	0x004

class CArchive
{
private:
	TArchiveFiles m_ArchiveFiles;
	TArchiveDirectoryFiles m_ArchiveDirectoryFiles;

	CArchiveFile* m_PartFile; ///< Last file incomplete
	CArchiveFile* m_File; ///< Last loaded file
	CArchiveDirectoryFile* m_DirectoryFile; ///< Last loaded directory file

	const char* m_szArchive;

	uint64_t m_CurrentOffset;
	uint64_t m_LastOffset;

	size_t m_FileOffset;

	uint64_t m_LastFileRequiredSize;

public:
	CArchiveFile* GetPartFile()
	{
		return m_PartFile;
	}

	void AddFile(CArchiveFile* pFile)
	{
		if (!pFile)
		{
			return;
		}

		m_File = pFile;
		{
			m_ArchiveFiles.push_back(pFile);
		}
	}

	void AddDirectoryFile(CArchiveDirectoryFile* pDirectoryFile)
	{
		if (!pDirectoryFile)
		{
			return;
		}

		m_DirectoryFile = pDirectoryFile;
		{
			m_ArchiveDirectoryFiles.push_back(pDirectoryFile);
		}
	}

	CArchiveFile* GetArchiveFileByName(const std::string& strName, bool bReversed = false)
	{
		if (!bReversed)
		{
			for (auto it = m_ArchiveFiles.begin(); it != m_ArchiveFiles.end(); ++it)
			{
				CArchiveFile* pArchiveFile = *it;

				if (pArchiveFile->GetRelativePath() == strName)
				{
					return pArchiveFile;
				}
			}
		}
		else
		{
			for (auto it = m_ArchiveFiles.rbegin(); it != m_ArchiveFiles.rend(); ++it)
			{
				CArchiveFile* pArchiveFile = *it;

				if (pArchiveFile->GetRelativePath() == strName)
				{
					return pArchiveFile;
				}
			}
		}

		return NULL;
	}

	CArchiveDirectoryFile* GetArchiveDirectoryFileByName(const std::string& strName, bool bReversed = false)
	{
		if (!bReversed)
		{
			for (auto it = m_ArchiveDirectoryFiles.begin(); it != m_ArchiveDirectoryFiles.end(); ++it)
			{
				CArchiveDirectoryFile* pArchiveDirectoryFile = *it;

				if (pArchiveDirectoryFile->GetRelativePath() == strName)
				{
					return pArchiveDirectoryFile;
				}
			}
		}
		else
		{
			for (auto it = m_ArchiveDirectoryFiles.rbegin(); it != m_ArchiveDirectoryFiles.rend(); ++it)
			{
				CArchiveDirectoryFile* pArchiveDirectoryFile = *it;

				if (pArchiveDirectoryFile->GetRelativePath() == strName)
				{
					return pArchiveDirectoryFile;
				}
			}
		}

		return NULL;
	}

	inline TArchiveFiles& GetArchiveFiles()
	{
		return m_ArchiveFiles;
	}

	inline TArchiveDirectoryFiles& GetArchiveDirectoryFiles()
	{
		return m_ArchiveDirectoryFiles;
	}

	inline uint64_t GetLastFileRequiredSize()
	{
		return m_LastFileRequiredSize;
	}

public:
	CArchive() :
		m_CurrentOffset(0),
		m_LastOffset(0),
		m_FileOffset(0),
		m_LastFileRequiredSize(0),
		m_File(NULL),
		m_PartFile(NULL),
		m_DirectoryFile(NULL),
		m_szArchive(NULL)
	{
	}

	CArchive(uint64_t offset) :
		m_CurrentOffset(offset),
		m_LastOffset(0),
		m_FileOffset(0),
		m_LastFileRequiredSize(0),
		m_File(NULL),
		m_PartFile(NULL),
		m_DirectoryFile(NULL),
		m_szArchive(NULL)
	{
	}

	~CArchive()
	{
		Clear();
	}

	static bool ValidArchive(const char* szFile)
	{
		return szFile && strlen(szFile) > 4;
	}

	static int IsSplittedFile(const char* szFile)
	{
		if (!ValidArchive(szFile))
		{
			return -1;
		}

		return boost::regex_match(szFile, boost::regex(".*.z[0-9][0-9]"), boost::regex_constants::match_any);
	}

	static int IsManifestFile(const char* szFile)
	{
		if (!ValidArchive(szFile))
		{
			return -1;
		}

		return boost::ends_with(szFile, ".patchmanifest");
	}

	static int IsMetaFile(const char* szFile)
	{
		if (!ValidArchive(szFile))
		{
			return -1;
		}

		return boost::ends_with(szFile, ".solidpkg");
	}

	static int IsVersionFile(const char* szFile)
	{
		if (!ValidArchive(szFile))
		{
			return -1;
		}

		return boost::ends_with(szFile, ".version");
	}

	static int IsMainFile(const char* szFile)
	{
		if (!ValidArchive(szFile))
		{
			return -1;
		}

		return boost::ends_with(szFile, ".zip");
	}

	inline bool SkipNext(FILE* f)
	{
		SArchiveFileHeader header;

		if (fread_s(&header, sizeof(SArchiveFileHeader), ARC_FILE_SIZE, 1, f) != 1)
		{
			return false;
		}

		if (SEEK(f, header.extra_field_len + header.filename_len + header.compressed_size, SEEK_CUR) == EOF)
		{
			return false;
		}

		m_FileOffset++;
		m_CurrentOffset = TELL(f);

		return true;
	}

	static int DecompressFile(FILE* fSrc, FILE* fDst, size_t iSize)
	{
		unsigned char in[BUFFER_SIZE];
		unsigned char out[BUFFER_SIZE];

		z_stream stream;

		stream.zalloc	= Z_NULL;
		stream.zfree	= Z_NULL;
		stream.opaque	= Z_NULL;
		stream.avail_in = 0;
		stream.next_in	= Z_NULL;

		int ret = inflateInit(&stream);

		if (ret != Z_OK)
		{
			return kInflateError;
		}

		size_t have = 0;

		do {
			stream.avail_in = fread(in, 1, xd3_min(BUFFER_SIZE, iSize), fSrc);
			{
				iSize -= stream.avail_in;
			}

			if (ferror(fSrc))
			{
				inflateEnd(&stream);
				return kInvalidFile;
			}

			if (stream.avail_in == 0)
			{
				ret = Z_STREAM_END;
				break;
			}

			stream.next_in = in;

			do {
				stream.avail_out= BUFFER_SIZE;
				stream.next_out	= out;

				ret = inflate(&stream, Z_NO_FLUSH);
				assert(ret != Z_STREAM_ERROR);

				switch (ret) 
				{
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;

				case Z_DATA_ERROR:
				case Z_MEM_ERROR:

					inflateEnd(&stream);
					return kInflateError;
				}

				have = BUFFER_SIZE - stream.avail_out;

				if (stream.total_in <= BUFFER_SIZE)
				{
					if (*(uint32_t*)out == DELTA_ENCODED_FILE_SIGNATURE)
					{
						inflateEnd(&stream);
						return kInvalidFile;
					}
				}

				if (fwrite(out, 1, have, fDst) != have || ferror(fDst))
				{
					inflateEnd(&stream);
					return Z_ERRNO;
				}	
			} while (stream.avail_out == 0);
		} while (ret != Z_STREAM_END);
					
		inflateEnd(&stream);
		return ret == Z_STREAM_END ? kSuccess : kInflateError;
	}

	// If a archive file is really big
	int UnpackNextArchiveFile(
		boost::filesystem::path path, 
		std::set<size_t>* pFilesToSkip = NULL, 
		CArchiveFile* pPartFile = NULL)
	{
		int error = kSuccess;

		if (!m_szArchive)
		{
			return kInvalidFile;
		}

		FILE* f = NULL;
		{
			fopen_s(&f, m_szArchive, "rb");
		}

		if (!f)
		{
			return kInvalidFile;
		}

		if (pPartFile)
		{
			m_CurrentOffset = pPartFile->GetRequiredSize();

			if (!pPartFile->ReadDataFromFile(f))
			{
				m_PartFile = pPartFile;
				return kFilePart;
			}

			m_ArchiveFiles.push_back(pPartFile);
		}
		else
		{
			if (m_CurrentOffset == 0)
			{
				m_CurrentOffset = IsSplittedFile(m_szArchive) ? START_SPLIT_ARCHIVE : START_MAIN_ARCHIVE;
			}
			else
			{
				m_CurrentOffset = m_LastOffset;
			}

			if (SEEK(f, m_CurrentOffset, SEEK_SET) == EOF)
			{
				fclose(f);
				return kEndOfFile;
			}
		}

		while (m_CurrentOffset != EOF)
		{
			m_LastOffset = m_CurrentOffset;

			if (pFilesToSkip != NULL && pFilesToSkip->find(m_FileOffset) != pFilesToSkip->end())
			{
				if (!SkipNext(f))
				{
					error = kEndOfFile;
					break;
				}

				continue;
			}

			uint32_t iSignature = 0;

			if (fread(&iSignature, 4, 1, f) == EOF)
			{
				break;
			}

			if (iSignature != ZIP_COMPRESSED_FILE_SIGNATURE)
			{
				break;
			}

			m_File = MEM_NEW CArchiveFile(m_FileOffset);

			if ((error = LoadFile(f, m_File, false)) != kSuccess)
			{
				if (error != kFilePart)
				{
					delete m_File;
				}
				else
				{
					m_PartFile = m_File;
				}

				break;
			}

			if (!path.is_absolute())
			{
				path = boost::filesystem::current_path().string() + "\\" + path.normalize().string();
			}

			boost::system::error_code ec;

			if (boost::filesystem::is_directory(path, ec))
			{
				if (ec)
				{
					return kInvalidFile;
				}

				path += "\\";
				path += boost::filesystem::path(m_File->GetRelativePath()).normalize();
			}

			FILE* fDst;
			{
				fopen_s(&fDst, path.string().c_str(), "rb");
			}

			if (!fDst)
			{
				fclose(f);
				return kInvalidSource;
			}

			if ((error = DecompressFile(f, fDst, m_File->GetHeaderRef().compressed_size)) != kSuccess)
			{
				fclose(f);
				fclose(fDst);

				return error;
			}

			fclose(fDst);

			m_FileOffset++;
			m_CurrentOffset = TELL(f);

			break;
		};

		fclose(f);

		return kSuccess;
	}

	static inline int LoadDirectoryFile(FILE *f, CArchiveDirectoryFile* pDirectoryFile)
	{
		size_t read;

		if (!pDirectoryFile)
		{
			return kInsufficientMemory;
		}

		if (!pDirectoryFile->ReadHeaderFromFile(f))
		{
			return kInvalidFile;
		}

		if (!pDirectoryFile->Valid())
		{
			return kInvalidFile;
		}

		read = fread_s(&pDirectoryFile->GetHeaderRef().filename, 
			MAX_FN_LEN, pDirectoryFile->GetHeaderRef().filename_len, 1, f);

		if (!read)
		{
			return kInvalidFile;
		}

		read = fread_s(&pDirectoryFile->GetHeaderRef().extrafield, 
			MAX_FN_LEN, pDirectoryFile->GetHeaderRef().extra_field_len, 1, f);

		if (!read)
		{
			return kInvalidFile;
		}

		return kSuccess;
	}

	static inline int LoadFile(FILE* f, CArchiveFile* pFile, bool bReadData = true)
	{
		size_t read;

		if (!pFile)
		{
			return kInsufficientMemory;
		}

		if (!pFile->ReadHeaderFromFile(f))
		{
			return kInvalidFile;
		}

		if (!pFile->Valid())
		{
			return kInvalidFile;
		}

		read = fread_s(&pFile->GetHeaderRef().filename, 
			MAX_FN_LEN, pFile->GetHeaderRef().filename_len, 1, f);

		if (!read)
		{
			return kInvalidFile;
		}

		if (pFile->GetHeaderRef().extra_field_len)
		{
			read = fread_s(&pFile->GetHeaderRef().extrafield, 
				MAX_FN_LEN, pFile->GetHeaderRef().extra_field_len, 1, f);

			if (!read)
			{
				return kInvalidFile;
			}
		}

		if (bReadData)
		{
			if (!pFile->Resize())
			{
				return kInsufficientMemory;
			}

			size_t size = pFile->GetCompressedData().size();

			if (!pFile->ReadDataFromFile(f))
			{
				return kFilePart;
			}
		}

		return kSuccess;
	}

	int LoadArchive(
		const char* szFile, 
		std::set<size_t>* pFilesToSkip = NULL, 
		CArchiveFile* pPartFile = NULL)
	{
		int error = kSuccess;

		if (!ValidArchive(szFile))
		{
			return kInvalidFile;
		}

		FILE* f = NULL;
		{
			fopen_s(&f, szFile, "rb");
		}

		if (!f)
		{
			return kInvalidFile;
		}

		m_szArchive = szFile;

		if (pPartFile)
		{
			m_CurrentOffset = pPartFile->GetRequiredSize();

			if (!pPartFile->ReadDataFromFile(f))
			{
				m_PartFile = pPartFile;
				return kFilePart;
			}

			m_ArchiveFiles.push_back(pPartFile);
		}
		else
		{
			if (m_CurrentOffset == 0)
			{
				m_CurrentOffset = IsSplittedFile(szFile) ? START_SPLIT_ARCHIVE : START_MAIN_ARCHIVE;
			}
			else
			{
				m_CurrentOffset = m_LastOffset;
			}

			if (SEEK(f, m_CurrentOffset, SEEK_SET) == EOF)
			{
				fclose(f);
				return kEndOfFile;
			}
		}

		while(m_CurrentOffset != EOF)
		{
			m_LastOffset = m_CurrentOffset;;

			if (pFilesToSkip != NULL && pFilesToSkip->find(m_FileOffset) != pFilesToSkip->end())
			{
				if (!SkipNext(f))
				{
					error = kEndOfFile;
					break;
				}

				continue;
			}

			uint32_t iSignature = 0;

			if (fread(&iSignature, 4, 1, f) == EOF)
			{
				break;
			}

			if (iSignature == ZIP_COMPRESSED_DIRECTORY_FILE_SIGNATURE)
			{
				m_DirectoryFile = MEM_NEW CArchiveDirectoryFile;

				if ((error = LoadDirectoryFile(f, m_DirectoryFile)) != kSuccess)
				{
					delete m_DirectoryFile;
					break;
				}

				if (m_DirectoryFile->GetHeaderRef().uncompressed_size != 0)
				{
					m_ArchiveDirectoryFiles.push_back(m_DirectoryFile);
				}
				else
				{
					delete m_DirectoryFile;
				}
			}
			else if(iSignature == ZIP_COMPRESSED_FILE_SIGNATURE)
			{
				m_File = MEM_NEW CArchiveFile(m_FileOffset);

				if ((error = LoadFile(f, m_File)) != kSuccess)
				{
					m_LastFileRequiredSize = 
						m_File->GetHeaderRef().compressed_size + 
						m_File->GetHeaderRef().uncompressed_size;

					if (error != kFilePart)
					{
						delete m_File;
					}
					else
					{
						m_PartFile = m_File;
					}

					break;
				}

				m_FileOffset++;

				if (m_File->GetDecompressedData().size() != 0)
				{
					m_ArchiveFiles.push_back(m_File);
				}
				else
				{
					delete m_File;
				}
			}
			else
			{
				break;
			}

			m_CurrentOffset = TELL(f);
		};

		fclose(f);

		return error;
	}

	inline int MEMLoadDirectoryFile(const char* pData, CArchiveDirectoryFile* pDirectoryFile)
	{
		if (!pDirectoryFile)
		{
			return kInsufficientMemory;
		}

		pDirectoryFile->ReadHeaderFromData(pData + m_CurrentOffset);

		if (!pDirectoryFile->Valid())
		{
			return kInvalidFile;
		}

		if (pDirectoryFile->GetHeaderRef().filename_len)
		{
			memcpy(pDirectoryFile->GetHeaderRef().filename, 
				pData + m_CurrentOffset + ARC_DIRE_SIZE - 4, 
				pDirectoryFile->GetHeaderRef().filename_len);
		}

		if (pDirectoryFile->GetHeaderRef().extra_field_len)
		{
			memcpy(pDirectoryFile->GetHeaderRef().extrafield, 
				pData + m_CurrentOffset + ARC_DIRE_SIZE + pDirectoryFile->GetHeaderRef().filename_len - 4, 
				pDirectoryFile->GetHeaderRef().extra_field_len);
		}

		m_CurrentOffset += pDirectoryFile->GetHeaderSize();

		return kSuccess;
	}

	inline int MEMLoadFile(const char* pData, CArchiveFile* pFile)
	{
		if (!pFile)
		{
			return kInsufficientMemory;
		}

		pFile->ReadHeaderFromData(pData + m_CurrentOffset);

		if (!pFile->Valid())
		{
			return kInvalidFile;
		}

		if (pFile->GetHeaderRef().filename_len)
		{
			memcpy(pFile->GetHeaderRef().filename, 
				pData + m_CurrentOffset + ARC_FILE_SIZE - 4, 
				pFile->GetHeaderRef().filename_len);
		}

		if (pFile->GetHeaderRef().extra_field_len)
		{
			memcpy(pFile->GetHeaderRef().extrafield, pData + m_CurrentOffset +ARC_FILE_SIZE + 
				pFile->GetHeaderRef().filename_len - 4, 
				pFile->GetHeaderRef().extra_field_len);
		}

		if (!pFile->Resize())
		{
			return kInsufficientMemory;
		}

		pFile->ReadDataFromData(pData + m_CurrentOffset + pFile->GetHeaderSize() - 4, pFile->GetCompressedData().size());

		return kSuccess;
	}

	int MEMLoadArchive(const char* pData, size_t iSize, bool bSplitted, CArchiveFile* pPartFile = NULL)
	{
		int error = kSuccess;

		m_szArchive = NULL;

		if (pPartFile)
		{
			pPartFile->ReadDataFromData(pData, pPartFile->GetRequiredSize());
			m_ArchiveFiles.push_back(pPartFile);

			m_CurrentOffset = pPartFile->GetRequiredSize();
		}
		else
		{
			m_CurrentOffset = bSplitted ? START_SPLIT_ARCHIVE : START_MAIN_ARCHIVE;
		}

		for (; m_CurrentOffset < iSize; m_LastOffset = m_CurrentOffset)
		{
			uint32_t iSignature = *(uint32_t*)(pData + m_CurrentOffset);
			{
				m_CurrentOffset += sizeof(uint32_t);
			}

			if (iSignature == ZIP_COMPRESSED_DIRECTORY_FILE_SIGNATURE)
			{
				m_DirectoryFile = MEM_NEW CArchiveDirectoryFile;

				if ((error = MEMLoadDirectoryFile(pData, m_DirectoryFile)) != kSuccess)
				{
					delete m_DirectoryFile;
					break;
				}

				m_ArchiveDirectoryFiles.push_back(m_DirectoryFile);
			}
			else if (iSignature == ZIP_COMPRESSED_FILE_SIGNATURE)
			{
				m_File = MEM_NEW CArchiveFile(m_FileOffset);

				if ((error = MEMLoadFile(pData, m_File)) != kSuccess)
				{
					delete m_File;
					break;
				}

				m_CurrentOffset += m_File->GetTrueSize() - sizeof(uint32_t);
				m_ArchiveFiles.push_back(m_File);

				m_FileOffset++;
			}
			else
			{
				break;
			}
		};

		return error;
	}

	void Clear()
	{
		for (auto it = m_ArchiveFiles.begin(); it != m_ArchiveFiles.end(); ++it)
		{
			delete *it;
		}

		m_ArchiveFiles.clear();

		for (auto it = m_ArchiveDirectoryFiles.begin(); it != m_ArchiveDirectoryFiles.end(); ++it)
		{
			delete *it;
		}

		m_ArchiveDirectoryFiles.clear();
	}

	void WriteFiles(bool bFreeIfDecompressed, const char* szPath = 0)
	{
		for (auto it = m_ArchiveFiles.begin(); it != m_ArchiveFiles.end(); ++it)
		{
			CArchiveFile* pArchiveFile = *it;
			{
				pArchiveFile->WriteDecompressed(szPath ? szPath : 
					(char*)pArchiveFile->GetHeaderRef().filename);

				if (bFreeIfDecompressed)
				{
					delete pArchiveFile;
				}
			}
		}

		if (bFreeIfDecompressed)
		{
			m_ArchiveFiles.clear();
		}
	}

	inline CArchiveDirectoryFile* GetDirectoryFile()
	{
		return m_DirectoryFile;
	}

	int32_t DecompressAll()
	{
		uLong keys[3] = { 0 };
		uLong *crcTab = (uLong*)get_crc_table();

		for (auto it = m_ArchiveFiles.begin(); it != m_ArchiveFiles.end(); ++it)
		{
			CArchiveFile *pFile = *it;

			if (pFile->IsDecompressed())
			{
				continue;
			}

			if (pFile->GetDecompressedData().empty())
			{
				continue;
			}

			z_stream stream;

			stream.zalloc = Z_NULL;
			stream.zfree = Z_NULL;
			stream.opaque = Z_NULL;

			int ret = inflateInit2(&stream, -15);

			if (ret != S_OK)
			{
				return kInflateError;
			}

			if (pFile->IsCrypted() && m_DirectoryFile && m_DirectoryFile->HasPassword())
			{
				char* szPassword = (char*)m_DirectoryFile->GetPassword();

				if (true /* m_DirectoryFile->PasswordLength() >= ENCRYPT_HEADER_LENGTH) */)
				{
					memcpy(m_DirectoryFile->GetPassword(), 
						&m_DirectoryFile->GetHeaderRef().extrafield[4], 
						m_DirectoryFile->PasswordLength());
				}
				else
				{
					for (size_t len = m_DirectoryFile->PasswordLength(); len != -1; --len)
					{
						szPassword[m_DirectoryFile->PasswordLength() - len] =
							m_DirectoryFile->GetHeaderRef().extrafield[4 +
							m_DirectoryFile->PasswordLength() - len];
					}
				}

				if (!GetPassword(szPassword))
				{
					return kInvalidFile;
				}

				init_keys(szPassword, keys, crcTab);

				data_container_t &container = pFile->GetDecompressedData();

				char head[ENCRYPT_HEADER_LENGTH] = { 0 };
				{
					memcpy(head, &pFile->GetCompressedData()[0], ENCRYPT_HEADER_LENGTH);
				}

				for (size_t i = 0; i < ENCRYPT_HEADER_LENGTH; ++i)
				{
					zdecode(keys, crcTab, head[i]);
				}

				for (size_t i = ENCRYPT_HEADER_LENGTH; i < pFile->GetCompressedData().size(); ++i)
				{
					pFile->GetCompressedData()[i] = zdecode(keys, crcTab, 
						pFile->GetCompressedData()[i]);
				}
			}

			file_size_t iBegin = pFile->IsCrypted() ? ENCRYPT_HEADER_LENGTH : 0;
			file_size_t iRequired = pFile->GetCompressedData().size() - iBegin;

			stream.next_in = &pFile->GetCompressedData()[iBegin + size_t(stream.total_in)];
			stream.next_out = &pFile->GetDecompressedData()[stream.total_out];
			stream.avail_out = pFile->GetDecompressedData().size();
			stream.avail_in = iRequired;

			ret = inflate(&stream, Z_FINISH);

			if (ret == Z_BUF_ERROR)
			{
				break;
			}

			switch (ret)
			{
			case Z_NEED_DICT:

				ret = Z_DATA_ERROR;

			case Z_DATA_ERROR:
			case Z_MEM_ERROR:

				inflateEnd(&stream);

				return kInflateError;
			};

			inflateEnd(&stream);

			pFile->SetDecompressed(true);
		}

		return kSuccess;
	}
};


#endif