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

#include "adt_file.h"
#include "log/default_log_levels.h"
#include <iomanip>

namespace wowpp
{
	ADTFile::ADTFile(String fileName)
		: MPQFile(std::move(fileName))
		, m_isValid(false)
		, m_offsetBase(0)
	{
	}

	bool ADTFile::load()
	{
		if (!m_source) return false;
		if (m_isValid) return true;

		// Read version chunk
		m_reader.readPOD(m_versionChunk);
		if (m_versionChunk.fourcc != 0x4d564552 || m_versionChunk.version != 0x12)
		{
			// ADT format 0x12 supported only
			return false;
		}

		// Calculate chunk offset base
		m_offsetBase = m_source->position() + 8;

		// Next chunk should be the MHDR chunk
		m_reader.readPOD(m_headerChunk);
		if (m_headerChunk.fourcc != 0x4d484452)
		{
			return false;
		}

		// Next one will be MCIN chunk
		if (m_headerChunk.offsMCIN)
		{
			// We have an MCIN chunk, read it
			m_source->seek(m_offsetBase + m_headerChunk.offsMCIN);
			m_reader.readPOD(m_mcinChunk);
			if (m_mcinChunk.fourcc != 0x4d43494e || m_mcinChunk.size != 0x1000)
			{
				return false;
			}
		}

		// MH2O chunk is next
		if (m_headerChunk.offsMH2O)
		{
			// We have an MH2O chunk, read it

		}

		m_isValid = true;
		return true;
	}

}
