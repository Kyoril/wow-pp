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

#include "mpq_file.h"
#include "math/vector3.h"

namespace wowpp
{
	struct M2Header
	{
		UInt32 magic;
		UInt32 version;
		UInt32 nameLen;
		UInt32 nameOffset;
		UInt32 globalFlags;
		UInt32 nGlobalSequences;
		UInt32 ofsGlobalSequences;
		UInt32 nAnimations;
		UInt32 ofsAnimations;
		UInt32 nAnimationLookup;
		UInt32 ofsAnimationLookup;
		UInt32 nPlayableAnimationLookup;
		UInt32 ofsPlayableAnimationLookup;
		UInt32 nBones;
		UInt32 ofsBones;
		UInt32 nSkelBoneLookup;
		UInt32 ofsSkelBoneLookup;
		UInt32 nVertices;
		UInt32 ofsVertices;
		UInt32 nViews;
		UInt32 ofsViews;
		UInt32 nColors;
		UInt32 ofsColors;
		UInt32 nTextures;
		UInt32 ofsTextures;
		UInt32 nTransparency;
		UInt32 ofsTransparency;
		UInt32 nI;
		UInt32 ofsI;
		UInt32 nTexAnims;
		UInt32 ofsTexAnims;
		UInt32 nTexReplace;
		UInt32 ofsTexReplace;
		UInt32 nRenderFlags;
		UInt32 ofsRenderFlags;
		UInt32 nBoneLookupTable;
		UInt32 ofsBoneLookupTable;
		UInt32 nTexLookup;
		UInt32 ofsTexLookup;
		UInt32 nTexUnits;
		UInt32 ofsTexUnits;
		UInt32 nTransLookup;
		UInt32 ofsTransLookup;
		UInt32 nTexAnimLookup;
		UInt32 ofsTexAnimLookup;
		std::array<float, 14> values;
		UInt32 nBoundingTriangles;
		UInt32 ofsBoundingTriangles;
		UInt32 nBoundingVertices;
		UInt32 ofsBoundingVertices;
		UInt32 nBoundingNormals;
		UInt32 ofsBoundingNormals;
		UInt32 nAttachments;
		UInt32 ofsAttachments;
		UInt32 nAttachLookup;
		UInt32 ofsAttachLookup;
		UInt32 nAttachments_2;
		UInt32 ofsAttachments_2;
		UInt32 nLights;
		UInt32 ofsLights;
		UInt32 nCameras;
		UInt32 ofsCameras;
		UInt32 nCameraLookup;
		UInt32 ofsCameraLookup;
		UInt32 nRibbonEmitters;
		UInt32 ofsRibbonEmitters;
		UInt32 nParticleEmitters;
		UInt32 ofsParticleEmitters;
	};

	struct M2View
	{
		UInt32 nIndices;
		UInt32 ofsIndices;
		UInt32 nTriangles;
		UInt32 ofsTriangles;
		UInt32 nVertexProperties;
		UInt32 ofsVertexProperties;
		UInt32 nSubMeshs;
		UInt32 ofsSubMeshs;
		UInt32 nTextures;
		UInt32 ofsTextures;
	};

	struct M2SubMesh
	{
		UInt32 id;
		UInt16 startVert;
		UInt16 nVerts;
		UInt16 startTri;
		UInt16 nTris;
		UInt16 nBones;
		UInt16 startBone;
		UInt16 unkBone1;
		UInt16 rootBone;
		float vec1[3];
		float vec2[4];
	};

	typedef UInt16 M2Index;

	/// This class contains data from a m2 file.
	class M2File final : public MPQFile
	{
	public:

		/// 
		explicit M2File(String fileName);

		/// @copydoc MPQFile::load()
		bool load() override;

		const std::vector<math::Vector3> &getVertices() const { return m_vertices; }
		/*
		const std::vector<UInt16> &getIndices() const { return m_indices; }
		const std::vector<std::unique_ptr<WMOFile>> &getGroups() const { return m_wmoGroups; }
		const bool isCollisionTriangle(UInt32 triangleIndex) const;
		*/

	private:

		bool m_isValid;
		std::vector<math::Vector3> m_vertices;
		/*std::vector<UInt16> m_indices;
		std::vector<char> m_triangleProps;
		*/
	};
}
