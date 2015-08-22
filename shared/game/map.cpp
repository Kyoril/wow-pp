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

#include "map.h"
#include "data/map_entry.h"
#include "detour_world_navigation.h"
#include "log/default_log_levels.h"
#include "common/make_unique.h"

namespace wowpp
{
	Map::Map(const MapEntry &entry, const boost::filesystem::path &dataPath)
		: m_entry(entry)
		, m_dataPath(dataPath)
		, m_navigation(make_unique<DetourWorldNavigation>(dataPath.string()))
	{
	}


}
