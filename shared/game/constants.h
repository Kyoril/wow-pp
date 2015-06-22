
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
	}
}
