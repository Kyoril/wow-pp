//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software 
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#include "pch.h"
#include "mpq_file.h"
#include "common/make_unique.h"
#include "log/default_log_levels.h"

// Define MPQ lock mutex
static std::mutex MPQMutex;

namespace mpq
{
	/// This deleter struct will make sure that 
	struct HandleDeleter final
	{
		virtual void operator ()(HANDLE* handle)
		{
			SFileCloseArchive(*handle);
		}
	};

	std::deque<std::unique_ptr<HANDLE, HandleDeleter>> openedArchives;

	bool loadMPQFile(const std::string &file)
	{
		std::lock_guard<std::mutex> lock(MPQMutex);

		std::unique_ptr<HANDLE, HandleDeleter> mpqHandle(new HANDLE(nullptr));
		if (!SFileOpenArchive(file.c_str(), 0, 0, mpqHandle.get()))
		{
			// Could not open MPQ archive file
			return false;
		}

		// Push to open archives
		mpq::openedArchives.push_front(std::move(mpqHandle));
		return true;
	}

	static bool openFile(const std::string &file, std::vector<char> &out_buffer)
	{
		std::lock_guard<std::mutex> lock(MPQMutex);
		for (auto &handle : openedArchives)
		{
			// Open the file
			HANDLE hFile = nullptr;
			if (!SFileOpenFileEx(*handle, file.c_str(), 0, &hFile) || !hFile)
			{
				// Could not find file in this archive, next one
				continue;
			}

			// Determine the file size in bytes
			DWORD fileSize = 0;
			fileSize = SFileGetFileSize(hFile, &fileSize);
			if (fileSize == 0)
			{
				// It seems as if this file does not exist
				SFileCloseFile(hFile);
				continue;
			}

			// Allocate memory
			out_buffer.resize(fileSize, 0);
			if (!SFileReadFile(hFile, &out_buffer[0], fileSize, nullptr, nullptr))
			{
				// Error reading the file
				SFileCloseFile(hFile);
				return false;
			}

			// Close the file for reading
			SFileCloseFile(hFile);
			return true;
		}

		return false;
	}
}

namespace wowpp
{
	MPQFile::MPQFile(String fileName)
		: m_fileName(fileName)
	{
		boost::filesystem::path p(fileName);
		m_baseName = p.stem().string();

		// Try to open the file
		if (!mpq::openFile(m_fileName, m_buffer))
		{
			// TODO: There was an error opening this file
			return;
		}

		// Setup the source
		m_source = make_unique<io::ContainerSource<std::vector<char>>>(m_buffer);
		m_reader.setSource(m_source.get());
	}
}
