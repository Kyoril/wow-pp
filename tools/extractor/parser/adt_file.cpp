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
		, m_m2Filenames(nullptr)
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
		if (m_headerChunk.offsMapObjects)
		{
			const size_t mwmoOffset = m_offsetBase + m_headerChunk.offsMapObjects;

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
		if (m_headerChunk.offsMapObjectsIds)
		{
			const size_t mwidOffset = m_offsetBase + m_headerChunk.offsMapObjectsIds;
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

		// MMDX informations
		if (m_headerChunk.offsModels)
		{
			const size_t mmdxOffset = m_offsetBase + m_headerChunk.offsModels;

			// We have an MMDX chunk, read it
			m_source->seek(mmdxOffset);

			UInt32 cc = 0, size = 0;
			m_reader >> io::read<UInt32>(cc) >> io::read<UInt32>(size);
			if (cc == 0x4d4d4458)
			{
				m_m2Filenames = m_buffer.data() + m_source->position();
			}
			else
			{
				WLOG("Invalid MMDX chunk in ADT (cc mismatch)");
			}
		}
		
		// MMID chunk
		if (m_headerChunk.offsModelsIds)
		{
			const size_t mmidOffset = m_offsetBase + m_headerChunk.offsModelsIds;
			m_source->seek(mmidOffset);

			UInt32 cc = 0, size = 0;
			m_reader >> io::read<UInt32>(cc) >> io::read<UInt32>(size);
			if (cc == 0x4d4d4944)
			{
				if (size % sizeof(UInt32) == 0)
				{
					if (m_m2Filenames != nullptr)
					{
						m_m2Index.resize(size / sizeof(UInt32), 0);
						m_reader >> io::read_range(m_m2Index);
					}
					else
					{
						WLOG("Found MMID chunk but didn't find valid MMDX chunk in ADT!");
					}
				}
				else
				{
					WLOG("Invalid number of MDX indices in MMID chunk of ADT");
				}
			}
			else
			{
				WLOG("Invalid MMID chunk in ADT (cc mismatch)");
			}
		}

		// M2 definitions
		if (m_headerChunk.offsDoodsDef)
		{
			const size_t mddfOffset = m_offsetBase + m_headerChunk.offsDoodsDef;
			m_source->seek(mddfOffset);

			// Read as many MODF chunks as available
			m_reader >> io::read<UInt32>(m_mddfChunk.fourcc) >> io::read<UInt32>(m_mddfChunk.size);
			if (m_mddfChunk.fourcc == 0x4d444446)
			{
				if (m_mddfChunk.size % sizeof(MDDFChunk::Entry) == 0)
				{
					m_mddfChunk.entries.resize(m_mddfChunk.size / sizeof(MDDFChunk::Entry));
					for (size_t i = 0; i < m_mddfChunk.entries.size(); ++i)
					{
						m_reader.readPOD(m_mddfChunk.entries[i]);
					}
				}
				else
				{
					WLOG("Number of MDDF entries mismatch!");
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

	static const UInt16 holetab_h[4] = { 0x1111, 0x2222, 0x4444, 0x8888 };
	static const UInt16 holetab_v[4] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };

	bool ADTFile::isHole(int square) const
	{
		int row = square / 128;
		int col = square % 128;
		int cellRow = row / 8;     // 8 squares per cell
		int cellCol = col / 8;
		int holeRow = row % 8 / 2;
		int holeCol = (square - (row * 128 + cellCol * 8)) / 2;

		const UInt16 &hole = getMCNKChunk(cellRow + cellCol * 16).holes;
		return (hole & holetab_v[holeCol] & holetab_h[holeRow]) != 0;
	}

	const String ADTFile::getWMO(UInt32 index) const
	{
		if (index > getWMOCount())
			return String();

		const char *filename = m_wmoFilenames + (m_wmoIndex[index]);
		return String(filename);
	}

	const String ADTFile::getMDX(UInt32 index) const
	{
		if (index > getMDXCount())
			return String();

		const char *filename = m_m2Filenames + (m_m2Index[index]);
		return String(filename);
	}

}
