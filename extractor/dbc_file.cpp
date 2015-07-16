//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "dbc_file.h"

namespace wowpp
{
	DBCFile::DBCFile(String fileName)
		: MPQFile(std::move(fileName))
		, m_isValid(false)
		, m_recordCount(0)
		, m_fieldCount(0)
		, m_recordSize(0)
		, m_stringSize(0)
		, m_recordOffset(0)
		, m_stringOffset(0)
	{
	}

	bool DBCFile::load()
	{
		if (!m_source) return false;
		if (m_isValid) return true;

		// Read four-cc code
		UInt32 fourcc = 0;
		m_reader >> io::read<UInt32>(fourcc);
		if (fourcc != 0x43424457)
		{
			// No need to parse any more - invalid CC
			return false;
		}

		if (!(m_reader
			>> io::read<UInt32>(m_recordCount)
			>> io::read<UInt32>(m_fieldCount)
			>> io::read<UInt32>(m_recordSize)
			>> io::read<UInt32>(m_stringSize)))
		{
			// Error reading these values
			return false;
		}

		if (m_fieldCount * 4 != m_recordSize)
		{
			// Invalid field count or record size...
			return false;
		}

		// Calculate data offsets
		m_recordOffset = m_source->position();
		m_stringOffset = m_recordOffset + (m_recordCount * m_recordSize);

		// File structure seems to be valid
		m_isValid = true;
		return true;
	}

}
