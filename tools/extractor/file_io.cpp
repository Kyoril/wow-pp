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
#include "file_io.h"

namespace wowpp
{
	FileIO::FileIO()
		: m_fp(0)
		, m_mode(-1)
	{
	}

	FileIO::~FileIO()
	{
		if (m_fp) 
			fclose(m_fp);
	}

	bool FileIO::openForWrite(const char * path)
	{
		if (m_fp) return false;
		m_fp = fopen(path, "wb");
		if (!m_fp) return false;
		m_mode = 1;
		return true;
	}

	bool FileIO::openForRead(const char * path)
	{
		if (m_fp) return false;
		m_fp = fopen(path, "rb");
		if (!m_fp) return false;
		m_mode = 2;
		return true;
	}

	bool FileIO::isWriting() const
	{
		return m_mode == 1;
	}

	bool FileIO::isReading() const
	{
		return m_mode == 2;
	}

	bool FileIO::write(const void * ptr, const size_t size)
	{
		if (!m_fp || m_mode != 1) return false;
		fwrite(ptr, size, 1, m_fp);
		return true;
	}

	bool FileIO::read(void * ptr, const size_t size)
	{
		if (!m_fp || m_mode != 2) return false;
		size_t readLen = fread(ptr, size, 1, m_fp);
		return readLen == 1;
	}
}
