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
#include "math/vector3.h"
#include "math/matrix4.h"
#include "math/bounding_box.h"

namespace wowpp
{
	// Chunk serialization constants

	/// Used as map header chunk signature.
	static constexpr UInt32 MapHeaderChunkCC = 0x50414D57;
	/// Used as map area chunk signature.
	static constexpr UInt32 MapAreaChunkCC = 0x52414D57;
	/// Used as map nav chunk signature.
	static constexpr UInt32 MapNavChunkCC = 0x564E4D57;
	/// Used as map wmo chunk signature.
	static constexpr UInt32 MapWMOChunkCC = 0x4D574D4F;
	/// Used as map doodad chunk signature.
	static constexpr UInt32 MapDoodadChunkCC = 0x4D57444F;

	// Helper types

	struct Triangle
	{
		UInt32 indexA, indexB, indexC;

		explicit Triangle()
			: indexA(0)
			, indexB(0)
			, indexC(0)
		{
		}
	};

	typedef math::Vector3 Vertex;

	enum NavTerrain
	{
		NAV_EMPTY = 0x00,
		NAV_GROUND = 0x01,
		NAV_MAGMA = 0x02,
		NAV_SLIME = 0x04,
		NAV_WATER = 0x08,
		NAV_UNUSED1 = 0x10,
		NAV_UNUSED2 = 0x20,
		NAV_UNUSED3 = 0x40,
		NAV_UNUSED4 = 0x80
	};

	struct MapChunkHeader
	{
		UInt32 fourCC;
		UInt32 size;

		MapChunkHeader()
			: fourCC(0)
			, size(0)
		{
		}
	};

	// Map chunks

	struct MapHeaderChunk
	{
		static constexpr UInt32 MapFormat = 0x150;

		MapChunkHeader header;
		UInt32 version;
		UInt32 offsAreaTable;
		UInt32 areaTableSize;
		UInt32 offsWmos;
		UInt32 wmoSize;
		UInt32 offsDoodads;
		UInt32 doodadSize;
		UInt32 offsNavigation;
		UInt32 navigationSize;

		MapHeaderChunk()
			: version(0)
			, offsAreaTable(0)
			, areaTableSize(0)
			, offsWmos(0)
			, wmoSize(0)
			, offsDoodads(0)
			, doodadSize(0)
			, offsNavigation(0)
			, navigationSize(0)
		{
		}
	};

	struct MapAreaChunk
	{
		MapChunkHeader header;
		struct AreaInfo
		{
			UInt32 areaId;
			UInt32 flags;

			///
			AreaInfo()
				: areaId(0)
				, flags(0)
			{
			}
		};
		std::array<AreaInfo, 16 * 16> cellAreas;
	};

	struct MapNavigationChunk
	{
		struct TileData
		{
			UInt32 size;
			std::vector<char> data;

			TileData()
				: size(0)
			{
			}
		};

		MapChunkHeader header;
		UInt32 tileCount;
		std::vector<TileData> tiles;

		explicit MapNavigationChunk()
			: tileCount(0)
		{
		}
	};

	struct MapWMOChunk
	{
		struct WMOEntry
		{
			UInt32 uniqueId;
			String fileName;
			math::Matrix4 transform;	// Not serialized!
			math::Matrix4 inverse;
			math::BoundingBox bounds;
		};

		MapChunkHeader header;
		std::vector<WMOEntry> entries;

		explicit MapWMOChunk()
		{
		}
	};

	struct MapDoodadChunk
	{
		struct DoodadEntry
		{
			UInt32 uniqueId;
			String fileName;
			math::Matrix4 transform;	// Not serialized!
			math::Matrix4 inverse;
			math::BoundingBox bounds;
		};

		MapChunkHeader header;
		std::vector<DoodadEntry> entries;

		explicit MapDoodadChunk()
		{
		}
	};

	// More helpers

	/// Stores map-specific tiled data informations like nav mesh data, height maps and such things.
	struct MapDataTile final
	{
		MapAreaChunk areas;
		MapNavigationChunk navigation;
		MapWMOChunk wmos;
		MapDoodadChunk doodads;

		~MapDataTile() {}
	};
}
