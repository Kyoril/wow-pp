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

#pragma once

#include "common/typedefs.h"

namespace wowpp
{
	namespace constants
	{
		/// Width or height of a ADT page in world units (Yards).
		static const float MapWidth = 533.33333f;
		/// Number of adt pages per map (one side).
		static const UInt32 PagesPerMap = 64;
		/// Number of tiles per adt page (one side).
		static const UInt32 TilesPerPage = 16;
		/// Total number of tiles per adt page (both sides).
		static const UInt32 TilesPerPageSquared = TilesPerPage * TilesPerPage;
		/// Total number of vertices per tile (both sides).
		static const UInt32 VertsPerTile = 9 * 9 + 8 * 8;
		/// Defined by client?
		static const UInt32 FriendListLimit = 50;
		/// Defined by client?
		static const UInt32 IgnoreListLimit = 25;
		/// Maximum number of action buttons the client knows.
		static const UInt32 ActionButtonLimit = 132;
	}
}
