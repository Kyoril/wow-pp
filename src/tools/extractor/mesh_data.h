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
	class ADTFile;

	/// Represents mesh data used for navigation mesh generation
	struct MeshData final
	{
		/// Three coordinates represent one vertex (x, y, z)
		std::vector<float> solidVerts;
		/// Three indices represent one triangle (v1, v2, v3)
		std::vector<int> solidTris;
		/// Triangle flags
		std::vector<unsigned char> triangleFlags;
	};

	/// Serializes a given mesh into the obj file format for debugging purposes.
	/// @param suffix String which is appended to the filename just before the file extension.
	/// @param mapID Id of the map of this mesh. Used to name the output file.
	/// @param tileX X coordinate of the tile in the grid. Used to name the output file.
	/// @param tileY Y coordinate of the tile in the grid. Used to name the output file.
	/// @param meshData Mesh which should be serialized.
	void serializeMeshData(const std::string &suffix, UInt32 mapID, UInt32 tileX, UInt32 tileY, const MeshData& meshData);

	enum Spot
	{
		TOP = 1,
		RIGHT = 2,
		LEFT = 3,
		BOTTOM = 4,
		ENTIRE = 5
	};

	/// Serializes the terrain mesh of a given ADT file and adds it's vertices to the provided mesh object.
	bool addTerrainMesh(const ADTFile &adt, UInt32 tileX, UInt32 tileY, Spot spot, MeshData &mesh);
}
