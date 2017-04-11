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

#include "map_data.h"
#include "tile_index.h"
#include "common/grid.h"
#include "math/ray.h"
#include "detour/DetourCommon.h"
#include "detour/DetourNavMesh.h"
#include "detour/DetourNavMeshQuery.h"

namespace wowpp
{
	// Foward declarations

	namespace proto
	{
		class MapEntry;
	}

	namespace math
	{
		class AABBTree;
	}

	struct IShape;

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
		typedef std::shared_ptr<MapDataTile> MapDataTilePtr;

	public:

		/// Creates a new instance of the map class and initializes it.
		/// @entry The base entry of this map.
		explicit Map(const proto::MapEntry &entry, boost::filesystem::path dataPath);

		/// Constructs a new nav mesh for this map id.
		void setupNavMesh();
		/// Loads all tiles at once.
		void loadAllTiles();
		/// Unloads all loaded map tiles of this map. Note that they may be reloaded if they
		/// are required again after being unloaded.
		void unloadAllTiles();
		/// Gets the map entry data of this map.
		const proto::MapEntry &getEntry() const {
			return m_entry;
		}
		/// Tries to get a specific data tile if it's loaded.
		MapDataTile *getTile(const TileIndex2D &position);
		/// Determines the height value at a given coordinate.
		/// @param pos The position in wow's coordinate system.
		/// @param out_height The height value will be stored here in wow's coordinate space.
		/// @param sampleADT If true, the terrain will be sampled.
		/// @param sampleWMO If true, WMOs will be sampled.
		/// @returns True if the height value could be sampled.
		bool getHeightAt(const math::Vector3 &pos, float &out_height, bool sampleADT, bool sampleWMO);
		/// Determines whether position B is in line of sight from position A.
		/// @param posA The source position the raycast is fired off.
		/// @param posB The destination position where the raycast is fired to.
		/// @returns true, if nothing prevents the line of sight, false otherwise.
		bool isInLineOfSight(const math::Vector3 &posA, const math::Vector3 &posB);
		/// Calculates a path from start point to the destination point.
		bool calculatePath(const math::Vector3 &source, math::Vector3 dest, std::vector<math::Vector3> &out_path, bool ignoreAdtSlope = true, const IShape *clipping = nullptr);
		/// 
		dtPolyRef getPolyByLocation(const math::Vector3 &point, float &out_distance) const;
		/// 
		bool getRandomPointOnGround(const math::Vector3 &center, float radius, math::Vector3 &out_point);
		/// Gets the nav mesh of this map id.
		const dtNavMesh *getNavMesh() const {
			return m_navMesh;
		}

		/// 
		std::shared_ptr<math::AABBTree> getWMOTree(const String &filename) const;

	private:

		/// Loads the given data tile.
		/// @param tileIndex Tile coordinates in the grid. Matches ADT cell coordinate system.
		/// @returns nullptr if loading failed.
		MapDataTilePtr loadTile(const TileIndex2D &tileIndex);
		/// Build a smooth path with corrected height values based on the detail mesh. This method
		/// operates in the recast coordinate system, so all inputs and outputs are in this system.
		/// @param dtStart Start position of the path.
		/// @param dtEnd End position of the path.
		/// @param polyPath List of polygons determined already for this path.
		/// @param out_smoothPath The calculated waypoints in recast space on success.
		/// @param maxPathSize Can be used to limit the amount of waypoints to generate. [1 < maxPathSize <= 74]
		dtStatus getSmoothPath(
			const math::Vector3 &dtStart, 
			const math::Vector3 &dtEnd, 
			std::vector<dtPolyRef> &polyPath, 
			std::vector<math::Vector3> &out_smoothPath, 
			UInt32 maxPathSize);
		
	private:

		const proto::MapEntry &m_entry;
		const boost::filesystem::path m_dataPath;
		// Note: We use a pointer here, because we don't need to load ALL height data
		// of all tiles, and Grid allocates them immediatly.
		Grid<MapDataTilePtr> m_tiles;
		/// Navigation mesh of this map. Note that this is shared between all map instanecs with the same map id.
		dtNavMesh *m_navMesh;
		/// Navigation mesh query for the current nav mesh (if any).
		std::unique_ptr<dtNavMeshQuery, NavQueryDeleter> m_navQuery;
		/// Filter to determine what kind of navigation polygons to use.
		dtQueryFilter m_filter;
		/// This filter avoids unwalkable adt areas.
		dtQueryFilter m_adtSlopeFilter;

		// Holds all loaded navigation meshes, keyed by map id.
		static std::map<UInt32, std::unique_ptr<dtNavMesh, NavMeshDeleter>> navMeshsPerMap;
	};
}
