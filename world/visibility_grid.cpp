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

#include "visibility_grid.h"
#include <memory>
#include <cassert>

namespace wowpp
{
	VisibilityGrid::VisibilityGrid()
	{
	}

	VisibilityGrid::~VisibilityGrid()
	{
	}

	bool VisibilityGrid::getTilePosition(float x, float y, float z, Int32 &outX, Int32 &outY) const
	{
		// Calculate grid x coordinates
		outX  = static_cast<Int32>(floor((512.0 - (static_cast<double>(x) / 33.3333))));
		if (outX < 0 || outX >= 1024)
			return false;

		// Calculate grid y coordinates
		outY = static_cast<Int32>(floor((512.0 - (static_cast<double>(y) / 33.3333))));
		if (outY < 0 || outY >= 1024)
			return false;

		return true;
	}
}
