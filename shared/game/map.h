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
#include "tile_index.h"
#include "common/grid.h"
#include "math/ray.h"
#include "detour/DetourCommon.h"
#include "detour/DetourNavMesh.h"
#include "detour/DetourNavMeshQuery.h"

namespace wowpp
{
	/// Header
	struct MapHeaderChunk
	{
		static constexpr UInt32 MapFormat = 0x140;

		UInt32 fourCC;
		UInt32 size;
		UInt32 version;
		UInt32 offsAreaTable;
		UInt32 areaTableSize;
		UInt32 offsWMOs;
		UInt32 wmoSize;
		UInt32 offsDoodads;
		UInt32 doodadSize;
		//UInt32 offsCollision;
		//UInt32 collisionSize;
		UInt32 offsNavigation;
		UInt32 navigationSize;

		MapHeaderChunk()
			: fourCC(0)
			, size(0)
			, version(0)
			, offsAreaTable(0)
			, areaTableSize(0)
			, offsWMOs(0)
			, wmoSize(0)
			, offsDoodads(0)
			, doodadSize(0)
			, offsCollision(0)
			, collisionSize(0)
			, offsNavigation(0)
			, navigationSize(0)
		{
		}
	};

	/// Map area chunk.
	struct MapAreaChunk
	{
		UInt32 fourCC;
		UInt32 size;
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

	struct MapCollisionChunk
	{
		UInt32 fourCC;
		UInt32 size;
		UInt32 vertexCount;
		UInt32 triangleCount;
		std::vector<Vertex> vertices;
		std::vector<Triangle> triangles;

		explicit MapCollisionChunk()
			: fourCC(0)
			, size(0)
			, vertexCount(0)
			, triangleCount(0)
		{
		}
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

		UInt32 fourCC;
		UInt32 size;
		UInt32 tileCount;
		std::vector<TileData> tiles;

		explicit MapNavigationChunk()
			: fourCC(0)
			, size(0)
			, tileCount(0)
		{
		}
	};

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

	namespace proto
	{
		class MapEntry;
	}

	/// Stores map-specific tiled data informations like nav mesh data, height maps and such things.
	struct MapDataTile final
	{
		MapAreaChunk areas;
		//MapHeightChunk heights;
		MapCollisionChunk collision;
		MapNavigationChunk navigation;

		~MapDataTile() {}
	};

	struct NavMeshDeleter final
	{
		virtual void operator()(dtNavMesh *ptr) const
		{
			if (ptr) dtFreeNavMesh(ptr);
		}
	};

	struct NavQueryDeleter final
	{
		virtual void operator()(dtNavMeshQuery *ptr) const
		{
			if (ptr) dtFreeNavMeshQuery(ptr);
		}
	};

	/// Converts a vertex from the recast coordinate system into WoW's coordinate system.
	Vertex recastToWoWCoord(const Vertex &in_recastCoord);
	/// Converts a vertex from the WoW coordinate system into recasts coordinate system.
	Vertex wowToRecastCoord(const Vertex &in_wowCoord);

	/// This class represents a map with additional geometry and navigation data.
	class Map final
	{
	public:

		/// Creates a new instance of the map class and initializes it.
		/// @entry The base entry of this map.
		explicit Map(const proto::MapEntry &entry, boost::filesystem::path dataPath);
		/// Loads all tiles at once.
		void loadAllTiles();
		/// Gets the map entry data of this map.
		const proto::MapEntry &getEntry() const {
			return m_entry;
		}
		/// Tries to get a specific data tile if it's loaded.
		MapDataTile *getTile(const TileIndex2D &position);
		/// Determines the height value at a given coordinate.
		/// @param x The x coordinate to check.
		/// @param y The y coordinate to check.
		/// @returns The height value at the given coordinate, or quiet NaN if no valid height.
		float getHeightAt(float x, float y);
		/// Determines whether position B is in line of sight from position A.
		/// @param posA The source position the raycast is fired off.
		/// @param posB The destination position where the raycast is fired to.
		/// @returns true, if nothing prevents the line of sight, false otherwise.
		bool isInLineOfSight(const math::Vector3 &posA, const math::Vector3 &posB);
		/// Calculates a path from start point to the destination point.
		bool calculatePath(const math::Vector3 &source, math::Vector3 dest, std::vector<math::Vector3> &out_path);
		/// 
		dtPolyRef getPolyByLocation(const math::Vector3 &point, float &out_distance) const;
		/// 
		bool getRandomPointOnGround(const math::Vector3 &center, float radius, math::Vector3 &out_point);
		/// Gets the nav mesh of this map id.
		const dtNavMesh *getNavMesh() const {
			return m_navMesh;
		}

	private:

		/// Build a smooth path with corrected height values based on the detail mesh. This method
		/// operates in the recast coordinate system, so all inputs and outputs are in this system.
		/// @param dtStart Start position of the path.
		/// @param dtEnd End position of the path.
		/// @param polyPath List of polygons determined already for this path.
		/// @param out_smoothPath The calculated waypoints in recast space on success.
		/// @param maxPathSize Can be used to limit the amount of waypoints to generate. [1 < maxPathSize <= 74]
		dtStatus getSmoothPath(const math::Vector3 &dtStart, const math::Vector3 &dtEnd, std::vector<dtPolyRef> &polyPath, std::vector<math::Vector3> &out_smoothPath, UInt32 maxPathSize);
		
	private:

		const proto::MapEntry &m_entry;
		const boost::filesystem::path m_dataPath;
		// Note: We use a pointer here, because we don't need to load ALL height data
		// of all tiles, and Grid allocates them immediatly.
		Grid<std::shared_ptr<MapDataTile>> m_tiles;
		/// Navigation mesh of this map. Note that this is shared between all map instanecs with the same map id.
		dtNavMesh *m_navMesh;
		/// Navigation mesh query for the current nav mesh (if any).
		std::unique_ptr<dtNavMeshQuery, NavQueryDeleter> m_navQuery;
		/// Filter to determine what kind of navigation polygons to use.
		dtQueryFilter m_filter;

		// Holds all loaded navigation meshes, keyed by map id.
		static std::map<UInt32, std::unique_ptr<dtNavMesh, NavMeshDeleter>> navMeshsPerMap;
	};
}
