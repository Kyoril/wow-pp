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
#include "adt_file.h"
#include "log/default_log_levels.h"
#include "math/matrix4.h"

namespace wowpp
{
	ADTFile::ADTFile(String fileName)
		: MPQFile(std::move(fileName))
		, m_isValid(false)
		, m_offsetBase(0)
		, m_wmoFilenames(nullptr)
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

		// MWMO informations
		if (m_headerChunk.offsMapObejcts)
		{
			const size_t mwmoOffset = m_offsetBase + m_headerChunk.offsMapObejcts;

			// We have an MWMO chunk, read it
			m_source->seek(mwmoOffset);

			UInt32 cc = 0, size = 0;
			m_reader >> io::read<UInt32>(cc) >> io::read<UInt32>(size);
			if (cc == 0x4d574d4f)
			{
				m_wmoFilenames = m_buffer.data() + m_source->position();
			}
			else
			{
				WLOG("Invalid MWMO chunk in ADT (cc mismatch)");
			}
		}

		// MWID chunk
		if (m_headerChunk.offsMapObejctsIds)
		{
			const size_t mwidOffset = m_offsetBase + m_headerChunk.offsMapObejctsIds;
			m_source->seek(mwidOffset);

			UInt32 cc = 0, size = 0;
			m_reader >> io::read<UInt32>(cc) >> io::read<UInt32>(size);
			if (cc == 0x4d574944)
			{
				if (size % sizeof(UInt32) == 0)
				{
					if (m_wmoFilenames != nullptr)
					{
						m_wmoIndex.resize(size / sizeof(UInt32), 0);
						m_reader >> io::read_range(m_wmoIndex);
					}
					else
					{
						WLOG("Found MWID chunk but didn't find valid MWMO chunk in ADT!");
					}
				}
				else
				{
					WLOG("Invalid number of WMO indices in MWID chunk of ADT");
				}
			}
			else
			{
				WLOG("Invalid MWID chunk in ADT (cc mismatch)");
			}
		}

		// WMO definitions
		if (m_headerChunk.offsObjectsDef)
		{
			const size_t modfOffset = m_offsetBase + m_headerChunk.offsObjectsDef;
			m_source->seek(modfOffset);

			// Read as many MODF chunks as available
			m_reader >> io::read<UInt32>(m_modfChunk.fourcc) >> io::read<UInt32>(m_modfChunk.size);
			if (m_modfChunk.fourcc == 0x4d4f4446)
			{
				if (m_modfChunk.size % sizeof(MODFChunk::Entry) == 0)
				{
					m_modfChunk.entries.resize(m_modfChunk.size / sizeof(MODFChunk::Entry));
					for (size_t i = 0; i < m_modfChunk.entries.size(); ++i)
					{
						m_reader.readPOD(m_modfChunk.entries[i]);
					}
				}
				else
				{
					WLOG("Number of MODF entries mismatch!");
				}
			}
		}

		// Next one will be MCIN chunk
		if (m_headerChunk.offsMCIN)
		{
			const size_t mcinOffset = m_offsetBase + m_headerChunk.offsMCIN;

			// We have an MCIN chunk, read it
			m_source->seek(mcinOffset);
			m_reader.readPOD(m_mcinChunk);
			if (m_mcinChunk.fourcc != 0x4d43494e || m_mcinChunk.size != 0x1000)
			{
				return false;
			}

			// Load MCNK chunks
			size_t cellIndex = 0;
			for (const auto &cell : m_mcinChunk.cells)
			{
				// Read MCNK chunk
				m_source->seek(cell.offMCNK);
				m_reader.readPOD(m_mcnkChunks[cellIndex]);

				// Read MCVT sub chunk
				m_source->seek(cell.offMCNK + m_mcnkChunks[cellIndex].offsMCVT);
				m_reader.readPOD(m_heightChunks[cellIndex]);

				// Read MCLQ sub chunk
				if (m_mcnkChunks[cellIndex].offsMCLQ)
				{
					m_source->seek(cell.offMCNK + m_mcnkChunks[cellIndex].offsMCLQ);
					m_reader.readPOD(m_liquidChunks[cellIndex]);
				}

				cellIndex++;
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

	const String ADTFile::getWMO(UInt32 index) const
	{
		if (index > getWMOCount())
			return String();

		const char *filename = m_wmoFilenames + (m_wmoIndex[index]);
		return String(filename);
	}

}
