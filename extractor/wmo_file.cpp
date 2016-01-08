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

#include "wmo_file.h"
#include "common/make_unique.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	WMOFile::WMOFile(String fileName)
		: MPQFile(std::move(fileName))
		, m_isValid(false)
		, m_isRoot(false)
	{
	}

	bool WMOFile::load()
	{
		if (!m_source) return false;
		if (m_isValid) return true;

		// First, we need the version chunk
		MVERChunk version;
		m_reader.readPOD(version);
		if (version.version != 11 && version.version != 17)
		{
			ELOG("Unsupported WMO file format version: Have " << version.version << ", expected 11 or 17");
			return false;
		}

		// Read next chunk. Based on this chunk, we decide what type of WMO we are dealing with
		UInt32 cc = 0, size = 0;
		m_reader >> io::read<UInt32>(cc) >> io::read<UInt32>(size);
		if (cc == 0x4D4F4844)	// MOHD - Root WMO
		{
			// Root wmo: Load header chunk
			m_isRoot = true;
			m_reader.readPOD(m_rootHeader);

			auto extensionPos = getFileName().find_last_of('.');
			assert(extensionPos != std::string::npos);

			const String baseFileName = getFileName().substr(0, extensionPos);
			const String extension = getFileName().substr(extensionPos);

			// Now we need to load all sub group files
			for (UInt32 i = 0; i < m_rootHeader.groupCount; ++i)
			{
				// Build group file name: BASEFILE_NNN.EXT
				std::ostringstream strm;
				strm << baseFileName << '_';
				strm << std::setfill('0') << std::setw(3) << i;
				strm << extension;

				// Load group file
				auto groupFile = make_unique<WMOFile>(strm.str());
				if (!groupFile->load())
				{
					ELOG("Could not load WMO group file: " << groupFile->getFileName());
					return false;
				}

				// Store group wmo for later use
				m_wmoGroups.emplace_back(std::move(groupFile));
			}
		}
		else if(cc == 0x4D4F4750)	// MOGP - Group WMO
		{
			// Not the root WMO
			m_isRoot = false;
			m_reader.readPOD(m_groupHeader);


		}

		// File structure seems to be valid
		m_isValid = true;
		return true;
	}

}
