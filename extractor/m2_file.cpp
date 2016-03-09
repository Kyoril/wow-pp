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
#include "m2_file.h"
#include "common/make_unique.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	M2File::M2File(String fileName)
		: MPQFile(std::move(fileName))
		, m_isValid(false)
	{
	}

	bool M2File::load()
	{
		if (!m_source) return false;
		if (m_isValid) return true;

		// File structure seems to be valid
		m_isValid = true;
		return true;
	}
	/*
	const bool WMOFile::isCollisionTriangle(UInt32 triangleIndex) const
	{
		if (triangleIndex >= m_triangleProps.size())
		{
			WLOG("Index out of bounds!");
			return true;
		}

		const auto &flags = m_triangleProps[triangleIndex * 2];

		//if (!(flags & 0x40)) return false;					// Only wall surfaces

		if (flags & 0x02) return false;						// Detail
		else if (flags & 0x04) return false;				// No collision
		else if (!(flags & (0x08 | 0x20))) return false;

		return true;
	}
	*/
}
