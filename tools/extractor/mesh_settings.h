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

namespace wowpp
{
	/// Contains NavMesh settings and checks.
	class MeshSettings
	{
	public:
		static constexpr int ChunksPerTile = 4;
		static constexpr int TileVoxelSize = 450;

		static constexpr float CellHeight = 0.5f;
		static constexpr float WalkableHeight = 1.6f;           // agent height in world units (yards)
		static constexpr float WalkableRadius = 0.3f;           // narrowest allowable hallway in world units (yards)
		static constexpr float WalkableSlope = 50.f;            // maximum walkable slope, in degrees
		static constexpr float WalkableClimb = 1.f;             // maximum 'step' height for which slope is ignored (yards)
		static constexpr float DetailSampleDistance = 3.f;      // heightfield detail mesh sample distance (yards)
		static constexpr float DetailSampleMaxError = 0.75f;    // maximum distance detail mesh surface should deviate from heightfield (yards)

																// NOTE: If Recast warns "Walk towards polygon center failed to reach center", try lowering this value
		static constexpr float MaxSimplificationError = 0.5f;

		static constexpr int MinRegionSize = 1600;
		static constexpr int MergeRegionSize = 400;
		static constexpr int VerticesPerPolygon = 6;

		// Nothing below here should ever have to change

		static constexpr int Adts = 64;
		static constexpr int ChunksPerAdt = 16;
		static constexpr int TilesPerADT = ChunksPerAdt / ChunksPerTile;
		static constexpr int TileCount = Adts * TilesPerADT;
		static constexpr int ChunkCount = Adts * ChunksPerAdt;

		static constexpr float AdtSize = 533.f + (1.f / 3.f);
		static constexpr float AdtChunkSize = AdtSize / ChunksPerAdt;

		static constexpr float TileSize = AdtChunkSize * ChunksPerTile;
		static constexpr float CellSize = TileSize / TileVoxelSize;
		static constexpr int VoxelWalkableRadius = static_cast<int>(WalkableRadius / CellSize);
		static constexpr int VoxelWalkableHeight = static_cast<int>(WalkableHeight / CellHeight);
		static constexpr int VoxelWalkableClimb = static_cast<int>(WalkableClimb / CellHeight);

		static_assert(WalkableRadius > CellSize, "CellSize must be able to approximate walkable radius");
		static_assert(WalkableHeight > CellSize, "CellSize must be able to approximate walkable height");
		static_assert(ChunksPerAdt % ChunksPerTile == 0, "Chunks per tile must divide chunks per ADT (16)");
		static_assert(VoxelWalkableRadius > 0, "VoxelWalkableRadius must be a positive integer");
		static_assert(VoxelWalkableHeight > 0, "VoxelWalkableHeight must be a positive integer");
		static_assert(VoxelWalkableClimb >= 0, "VoxelWalkableClimb must be non-negative integer");
		static_assert(CellSize > 0.f, "CellSize must be positive");
	};
}
