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
#include "mesh_data.h"
#include "adt_file.h"
#include "mesh_settings.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace 
	{
		static const int V9_SIZE = 129;
		static const int V9_SIZE_SQ = V9_SIZE * V9_SIZE;
		static const int V8_SIZE = 128;
		static const int V8_SIZE_SQ = V8_SIZE * V8_SIZE;
		static const float GRID_SIZE = 533.33333f;
		static const float GRID_PART_SIZE = GRID_SIZE / V8_SIZE;

		enum GridType
		{
			GRID_V8,
			GRID_V9
		};

		/// This method gets the height of a given coordinate.
		static void getHeightCoord(UInt32 index, GridType grid, float xOffset, float yOffset, float* out_coord, const float* v)
		{
			// wow coords: x, y, height
			// coord is mirroed about the horizontal axes
			switch (grid)
			{
				case GRID_V9:
					out_coord[0] = (xOffset + index % (V9_SIZE)* GRID_PART_SIZE) * -1.f;
					out_coord[1] = (yOffset + (int)(index / (V9_SIZE)) * GRID_PART_SIZE) * -1.f;
					out_coord[2] = v[index];
					break;
				case GRID_V8:
					out_coord[0] = (xOffset + index % (V8_SIZE)* GRID_PART_SIZE + GRID_PART_SIZE / 2.f) * -1.f;
					out_coord[1] = (yOffset + (int)(index / (V8_SIZE)) * GRID_PART_SIZE + GRID_PART_SIZE / 2.f) * -1.f;
					out_coord[2] = v[index];
					break;
			}
		}

		/// This method gets the indices of a triangle depending on it's index.
		static void getHeightTriangle(UInt32 square, Spot triangle, int* indices)
		{
			int rowOffset = square / V8_SIZE;
			switch (triangle)
			{
				case TOP:
					indices[0] = square + rowOffset;                //           0-----1 .... 128
					indices[1] = square + 1 + rowOffset;            //           |\ T /|
					indices[2] = (V9_SIZE_SQ)+square;				//           | \ / |
					break;                                          //           |L 0 R| .. 127
				case LEFT:                                          //           | / \ |
					indices[0] = square + rowOffset;                //           |/ B \|
					indices[1] = (V9_SIZE_SQ)+square;				//          129---130 ... 386
					indices[2] = square + V9_SIZE + rowOffset;      //           |\   /|
					break;                                          //           | \ / |
				case RIGHT:                                         //           | 128 | .. 255
					indices[0] = square + 1 + rowOffset;            //           | / \ |
					indices[1] = square + V9_SIZE + 1 + rowOffset;  //           |/   \|
					indices[2] = (V9_SIZE_SQ)+square;				//          258---259 ... 515
					break;
				case BOTTOM:
					indices[0] = (V9_SIZE_SQ)+square;
					indices[1] = square + V9_SIZE + 1 + rowOffset;
					indices[2] = square + V9_SIZE + rowOffset;
					break;
				default: break;
			}
		}

		/// 
		static void getLoopVars(Spot portion, int& loopStart, int& loopEnd, int& loopInc)
		{
			switch (portion)
			{
				case ENTIRE:
					loopStart = 0;
					loopEnd = V8_SIZE_SQ;
					loopInc = 1;
					break;
				case TOP:
					loopStart = 0;
					loopEnd = V8_SIZE;
					loopInc = 1;
					break;
				case LEFT:
					loopStart = 0;
					loopEnd = V8_SIZE_SQ - V8_SIZE + 1;
					loopInc = V8_SIZE;
					break;
				case RIGHT:
					loopStart = V8_SIZE - 1;
					loopEnd = V8_SIZE_SQ;
					loopInc = V8_SIZE;
					break;
				case BOTTOM:
					loopStart = V8_SIZE_SQ - V8_SIZE;
					loopEnd = V8_SIZE_SQ;
					loopInc = 1;
					break;
			}
		}
	}

	void serializeMeshData(const std::string &suffix, UInt32 mapID, UInt32 tileX, UInt32 tileY, const MeshData& meshData)
	{
		if (meshData.solidTris.empty())
			return;

		std::stringstream nameStrm;
		nameStrm << "meshes/map" << std::setw(3) << std::setfill('0') << mapID << std::setw(2) << tileY << tileX << suffix << ".obj";

		auto const fileName = nameStrm.str();

		std::ofstream objFile(fileName.c_str(), std::ios::out);
		if (!objFile)
		{
			ELOG("Failed to open " << fileName << " for writing");
			return;
		}

		const float* verts = &meshData.solidVerts[0];
		int vertCount = meshData.solidVerts.size() / 3;
		const int* tris = &meshData.solidTris[0];
		int triCount = meshData.solidTris.size() / 3;

		for (int i = 0; i < meshData.solidVerts.size() / 3; i++)
			objFile << "v " << verts[i * 3] << " " << verts[i * 3 + 1] << " " << verts[i * 3 + 2] << "\n";

		for (int i = 0; i < meshData.solidTris.size() / 3; i++)
			objFile << "f " << tris[i * 3] + 1 << " " << tris[i * 3 + 1] + 1 << " " << tris[i * 3 + 2] + 1 << "\n";
	}

	bool addTerrainMesh(const ADTFile & adt, UInt32 tileX, UInt32 tileY, Spot spot, MeshData & mesh)
	{
		static_assert(V8_SIZE == 128, "V8_SIZE has to equal 128");
		std::array<float, 128 * 128> V8;
		V8.fill(0.0f);
		static_assert(V9_SIZE == 129, "V9_SIZE has to equal 129");
		std::array<float, 129 * 129> V9;
		V9.fill(0.0f);

		UInt32 chunkIndex = 0;
		for (UInt32 i = 0; i < 16; ++i)
		{
			for (UInt32 j = 0; j < 16; ++j)
			{
				auto &MCNK = adt.getMCNKChunk(j + i * 16);
				auto &MCVT = adt.getMCVTChunk(j + i * 16);

				// get V9 height map
				for (int y = 0; y <= 8; y++)
				{
					int cy = i * 8 + y;
					for (int x = 0; x <= 8; x++)
					{
						int cx = j * 8 + x;
						V9[cy + cx * V9_SIZE] = MCVT.heights[y * (8 * 2 + 1) + x] + MCNK.ypos;
					}
				}
				// get V8 height map
				for (int y = 0; y < 8; y++)
				{
					int cy = i * 8 + y;
					for (int x = 0; x < 8; x++)
					{
						int cx = j * 8 + x;
						V8[cy + cx * V8_SIZE] = MCVT.heights[y * (8 * 2 + 1) + 8 + 1 + x] + MCNK.ypos;
					}
				}
			}
		}

		int count = mesh.solidVerts.size() / 3;
		float xoffset = (float(tileX) - 32) * MeshSettings::AdtSize;
		float yoffset = (float(tileY) - 32) * MeshSettings::AdtSize;

		float coord[3];
		for (int i = 0; i < V9_SIZE_SQ; ++i)
		{
			getHeightCoord(i, GRID_V9, xoffset, yoffset, coord, V9.data());
			mesh.solidVerts.push_back(-coord[1]);
			mesh.solidVerts.push_back(coord[2]);
			mesh.solidVerts.push_back(-coord[0]);
		}
		for (int i = 0; i < V8_SIZE_SQ; ++i)
		{
			getHeightCoord(i, GRID_V8, xoffset, yoffset, coord, V8.data());
			mesh.solidVerts.push_back(-coord[1]);
			mesh.solidVerts.push_back(coord[2]);
			mesh.solidVerts.push_back(-coord[0]);
		}

		int loopStart, loopEnd, loopInc;
		int indices[3];

		getLoopVars(spot, loopStart, loopEnd, loopInc);
		for (int i = loopStart; i < loopEnd; i += loopInc)
		{
			for (int j = TOP; j <= BOTTOM; j += 1)
			{
				if (!adt.isHole(i))
				{
					getHeightTriangle(i, Spot(j), indices);
					mesh.solidTris.push_back(indices[0] + count);
					mesh.solidTris.push_back(indices[1] + count);
					mesh.solidTris.push_back(indices[2] + count);
					mesh.triangleFlags.push_back(1 << 1);
				}
			}
		}

		return true;
	}

}
