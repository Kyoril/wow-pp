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
#include <OgreHardwareBuffer.h>
#include <OgreStringConverter.h>
#include <OgreDataStream.h>
#include <OgreMesh.h>
#include <OgreSubMesh.h>
#include <OgreResourceGroupManager.h>
#include <OgreHardwareVertexBuffer.h>
#include <OgreVertexIndexData.h>
#include <OgreHardwareBufferManager.h>
#include <OgreException.h>
#include <OgreMaterialManager.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>

namespace wowpp
{
	namespace editor
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

		struct M2Texture
		{
			UInt32 type;
			UInt16 unk1;
			UInt16 flags;
			UInt32 fileNameLen;
			UInt32 fileNameOffs;
		};

		class M2MeshLoader : public Ogre::ManualResourceLoader
		{
		private:

			Ogre::String m_fileName;

		public:

			M2MeshLoader(Ogre::String fileName)
				: m_fileName(std::move(fileName))
			{
			}

			virtual void loadResource(Ogre::Resource* resource) override;
		};
	}
}