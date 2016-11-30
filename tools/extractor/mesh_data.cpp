
#include "pch.h"
#include "mesh_data.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	void serializeMeshData(const std::string &suffix, UInt32 mapID, UInt32 tileX, UInt32 tileY, MeshData& meshData)
	{
		std::stringstream nameStrm;
		nameStrm << "meshes/map" << std::setw(3) << std::setfill('0') << mapID << std::setw(2) << tileY << tileX << suffix << ".obj";

		auto const fileName = nameStrm.str();

		std::ofstream objFile(fileName.c_str(), std::ios::out);
		if (!objFile)
		{
			ELOG("Failed to open " << fileName << " for writing");
			return;
		}

		float* verts = &meshData.solidVerts[0];
		int vertCount = meshData.solidVerts.size() / 3;
		int* tris = &meshData.solidTris[0];
		int triCount = meshData.solidTris.size() / 3;

		for (int i = 0; i < meshData.solidVerts.size() / 3; i++)
			objFile << "v " << verts[i * 3] << " " << verts[i * 3 + 1] << " " << verts[i * 3 + 2] << "\n";

		for (int i = 0; i < meshData.solidTris.size() / 3; i++)
			objFile << "f " << tris[i * 3] + 1 << " " << tris[i * 3 + 1] + 1 << " " << tris[i * 3 + 2] + 1 << "\n";
	}
}
