#ifndef _TORRENT_H_
#define _TORRENT_H_

#pragma once

#include "bdecode.h"

using namespace libtorrent;

struct SFile
{
	SFile(std::string Name, uint64_t Size) : name(Name), size(Size) {};
	SFile() {};

	uint64_t size;
	std::string name;
};

typedef std::vector<SFile> TFiles;

class CFileStorage
{
private:
	TFiles m_vFiles;
	uint64_t m_TotalSize;

public:
	uint64_t GetTotalSize()
	{
		return m_TotalSize;
	}

	void Reserve(size_t size)
	{
		m_vFiles.reserve(size);
	}

	void AddFile(std::string name, uint64_t size)
	{
		m_vFiles.push_back(SFile(name, size));
		m_TotalSize += size;
	}

	std::vector<SFile> const& GetFiles()
	{
		return m_vFiles;
	}

	void Clear()
	{
		m_vFiles.clear();
		m_TotalSize = 0;
	}
};

class CTorrentData
{
protected:
	CFileStorage m_Files;

private:
	libtorrent::bdecode_node m_Context;
	libtorrent::error_code m_ErrorCode;

public:
	CTorrentData()
	{
	}

	~CTorrentData()
	{
	}

	TFiles const& GetFileStorage()
	{
		return m_Files.GetFiles();
	}

	uint64_t TotalSize()
	{
		return m_Files.GetTotalSize();
	}

	bool DecodeMetadata(const char* szBDecoded, size_t size)
	{
		m_Files.Clear();

		int iResult = libtorrent::bdecode(szBDecoded, szBDecoded + size, m_Context, m_ErrorCode);

		if (m_ErrorCode)
		{
			return false;
		}

		if (iResult != 0)
		{
			return false;
		}

		bdecode_node info = m_Context.dict_find_dict("info");

		if (!info)
		{
			return false;
		}

		bdecode_node files = info.dict_find_list("files");

		if (!files)
		{
			if (!extract_single_file(info, m_Files, "", 0, true, m_ErrorCode))
			{
				return false;
			}

			if (m_ErrorCode)
			{
				return false;
			}

			return true;
		}

		return extract_files(files, m_Files, "", 0, m_ErrorCode);
	}

private: // (c) LIBTORRENT
	bool extract_files(bdecode_node const& list, CFileStorage& target
		, std::string const& root_dir, ptrdiff_t info_ptr_diff, error_code& ec)
	{
		if (list.type() != bdecode_node::list_t)
		{
			return false;
		}

		target.Reserve(list.list_size());

		for (int i = 0, end(list.list_size()); i < end; ++i)
		{
			if (!extract_single_file(list.list_at(i), target, root_dir
				, info_ptr_diff, false, ec))
				return false;
		}

		return true;
	}

	bool extract_single_file(bdecode_node const& dict, CFileStorage& files
		, std::string const& root_dir, ptrdiff_t info_ptr_diff, bool top_level
		, error_code& ec)
	{
		if (dict.type() != bdecode_node::dict_t) return false;

		boost::int64_t file_size = dict.dict_find_int_value("length", -1);

		if (file_size < 0)
		{
			return false;
		}

		boost::int64_t mtime = dict.dict_find_int_value("mtime", 0);

		std::string path = root_dir;
		std::string path_element;
		char const* filename = NULL;
		int filename_len = 0;

		if (top_level)
		{
			bdecode_node p = dict.dict_find_string("name.utf-8");
			if (!p) p = dict.dict_find_string("name");
			if (!p || p.string_length() == 0)
			{
				return false;
			}

			filename = p.string_ptr() + info_ptr_diff;
			filename_len = p.string_length();
		}
		else
		{
			bdecode_node p = dict.dict_find_list("path.utf-8");
			if (!p) p = dict.dict_find_list("path");
			if (!p || p.list_size() == 0)
			{
				return false;
			}

			int preallocate = int(path.size());
			for (int i = 0, end(p.list_size()); i < end; ++i)
			{
				bdecode_node e = p.list_at(i);
				if (e.type() != bdecode_node::string_t)
				{
					return false;
				}
				preallocate += e.string_length() + 1;
			}
			path.reserve(preallocate);

			for (int i = 0, end(p.list_size()); i < end; ++i)
			{
				bdecode_node e = p.list_at(i);
				if (i == end - 1)
				{
					filename = e.string_ptr() + info_ptr_diff;
					filename_len = e.string_length();
				}
			}
		}

		files.AddFile(std::string(filename, filename + filename_len), file_size);

		return true;
	}
};

#endif
