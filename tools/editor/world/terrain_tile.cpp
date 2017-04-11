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
#include "terrain_tile.h"
#include "terrain_page.h"
#include "common/constants.h"
#include "common/make_unique.h"
#include "game/constants.h"
#include "OgreSceneManager.h"
#include "OgreMath.h"
#include "OgreHardwareBufferManager.h"
#include "OgreHardwareBuffer.h"
#include <OgreNode.h>
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace view
	{
		static const UInt16 MAIN_BINDING = 0;

		TerrainTile::TerrainTile(
			Ogre::SceneManager &sceneMgr,
			Ogre::Camera &camera,
			const Ogre::String &name,
			TerrainPage &page,
			Ogre::MaterialPtr material,
			UInt32 tileX,
			UInt32 tileY,
			terrain::model::Heightmap &tileHeights,
			terrain::model::Normalmap &tileNormals,
			UInt16 holes
			)
			: Ogre::MovableObject(name)
			, Ogre::Renderable()
			, m_sceneMgr(sceneMgr)
			, m_material(material)
			, m_tileX(tileX)
			, m_tileY(tileY)
			, m_boundingRadius(0.0f)
			, m_lightListDirty(true)
			, m_tileHeights(tileHeights)
			, m_tileNormals(tileNormals)
			, m_holes(holes)
		{
			createVertexData();
			createIndexes();
		}

		TerrainTile::~TerrainTile()
		{
		}

		Ogre::uint32 TerrainTile::getTypeFlags() const
		{
			// This geometry is handled as world geometry
			return Ogre::SceneManager::WORLD_GEOMETRY_TYPE_MASK;
		}

		const Ogre::MaterialPtr & TerrainTile::getMaterial() const
		{
			return m_material;
		}

		void TerrainTile::getRenderOperation(Ogre::RenderOperation &op)
		{
			op.useIndexes = true;
			op.operationType = Ogre::RenderOperation::OT_TRIANGLE_LIST;
			op.vertexData = m_vertexData.get();
			op.indexData = m_indexData.get();
		}

		void TerrainTile::getWorldTransforms(Ogre::Matrix4 *m) const
		{
			*m = Ogre::MovableObject::_getParentNodeFullTransform();
		}

		const Ogre::Quaternion & TerrainTile::getWorldOrientation() const
		{
			Ogre::Node *n = getParentNode();
			ASSERT(n);

			return n->_getDerivedOrientation();
		}

		const Ogre::Vector3 & TerrainTile::getWorldPosition() const
		{
			Ogre::Node *n = getParentNode();
			ASSERT(n);

			return n->_getDerivedPosition();
		}

		Ogre::Real TerrainTile::getSquaredViewDepth(const Ogre::Camera *cam) const
		{
			Ogre::Node *n = getParentNode();
			ASSERT(n);

			return n->getSquaredViewDepth(cam);
		}

		const Ogre::LightList & TerrainTile::getLights() const
		{
			if (m_lightListDirty)
			{
				m_sceneMgr._populateLightList(m_center, m_boundingRadius, m_lightList);
				m_lightListDirty = false;
			}

			return m_lightList;
		}

		const Ogre::String & TerrainTile::getMovableType() const
		{
			static const Ogre::String movableType = "WoW++ Terrain Tile";
			return movableType;
		}

		const Ogre::AxisAlignedBox & TerrainTile::getBoundingBox() const
		{
			return m_bounds;
		}

		Ogre::Real TerrainTile::getBoundingRadius() const
		{
			return m_boundingRadius;
		}

		void TerrainTile::_updateRenderQueue(Ogre::RenderQueue *queue)
		{
			m_lightListDirty = true;
			queue->addRenderable(this, getRenderQueueGroup());
		}

		void TerrainTile::visitRenderables(Ogre::Renderable::Visitor *visitor, bool debugRenderables)
		{
			visitor->visit(this, 0, false, 0);
		}

		bool TerrainTile::getCastsShadows() const
		{
			return false;
		}

		static UInt16 holetab_h[4] = { 0x1111, 0x2222, 0x4444, 0x8888 };
		static UInt16 holetab_v[4] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };

		void TerrainTile::getVertexData(std::vector<Ogre::Vector3>& out_vertices, std::vector<UInt32>& out_indices)
		{
			const auto &worldPos = getParentNode()->_getDerivedPosition();

			const VertexID startX = m_tileX * 8;
			const VertexID startY = m_tileY * 8;
			const VertexID endX = startX + 9;
			const VertexID endY = startY + 9;
			const float scale = -((constants::MapWidth / static_cast<float>(constants::TilesPerPage)) / 8.0f);

			size_t index = 0;
			for (VertexID j = startY; j < endY - 1; ++j)
			{
				for (VertexID i = startX; i < endX; ++i)
				{
					float height = m_tileHeights[index++];
					out_vertices.push_back(
						Ogre::Vector3(scale * static_cast<Ogre::Real>(j), scale * static_cast<Ogre::Real>(i), height) + worldPos);
				}

				for (VertexID i = startX; i < endX - 1; ++i)
				{
					float height = m_tileHeights[index++];
					out_vertices.push_back(
						Ogre::Vector3(scale * static_cast<Ogre::Real>(j) + scale * 0.5, scale * static_cast<Ogre::Real>(i) + scale * 0.5, height) + worldPos);
				}
			}

			// One last row
			VertexID j = endY - 1;
			for (VertexID i = startX; i < endX; ++i)
			{
				float height = m_tileHeights[index++];
				out_vertices.push_back(
					Ogre::Vector3(scale * static_cast<Ogre::Real>(j), scale * static_cast<Ogre::Real>(i), height) + worldPos);
			}

			// Indices
			for (UInt32 i = 0; i < 8; ++i)
			{
				for (UInt32 j = 0; j < 8; ++j)
				{
					const bool isHole =
						(m_holes & holetab_h[i / 2] & holetab_v[j / 2]) != 0;
					if (isHole)
					{
						continue;
					}

					UInt16 topLeftInd = i + j * 9 + j * 8;
					UInt16 topRightInd = topLeftInd + 1;
					UInt16 centerInd = topLeftInd + 9;
					UInt16 bottomLeftInd = centerInd + 8;
					UInt16 bottomRightInd = bottomLeftInd + 1;

					// Top
					out_indices.push_back(topRightInd);
					out_indices.push_back(topLeftInd);
					out_indices.push_back(centerInd);

					// Right
					out_indices.push_back(bottomRightInd);
					out_indices.push_back(topRightInd);
					out_indices.push_back(centerInd);

					// Bottom
					out_indices.push_back(bottomLeftInd);
					out_indices.push_back(bottomRightInd);
					out_indices.push_back(centerInd);

					// Left
					out_indices.push_back(topLeftInd);
					out_indices.push_back(bottomLeftInd);
					out_indices.push_back(centerInd);
				}
			}
		}
		
		void TerrainTile::createVertexData()
		{
			m_vertexData = make_unique<Ogre::VertexData>();
			m_vertexData->vertexStart = 0;
			m_vertexData->vertexCount = constants::VertsPerTile;

			Ogre::VertexDeclaration *decl = m_vertexData->vertexDeclaration;
			Ogre::VertexBufferBinding *bind = m_vertexData->vertexBufferBinding;

			size_t offset = 0;
			decl->addElement(MAIN_BINDING, offset, Ogre::VET_FLOAT3, Ogre::VES_POSITION);
			offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);
            decl->addElement(MAIN_BINDING, offset, Ogre::VET_FLOAT3, Ogre::VES_NORMAL);
            offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);
			decl->addElement(MAIN_BINDING, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 0);
			offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT2);
						
			m_mainBuffer = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
				decl->getVertexSize(MAIN_BINDING),
				m_vertexData->vertexCount,
				Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY
				);

			bind->setBinding(MAIN_BINDING, m_mainBuffer);

			const VertexID startX = m_tileX * 8;
			const VertexID startY = m_tileY * 8;
			const VertexID endX = startX + 9;
			const VertexID endY = startY + 9;
			
			Ogre::Real minHeight = 9999999.0f;
			Ogre::Real maxHeight = -9999999.0f;

			const Ogre::VertexElement *posElem = decl->findElementBySemantic(Ogre::VES_POSITION);
            const Ogre::VertexElement *nrmElem = decl->findElementBySemantic(Ogre::VES_NORMAL);
			const Ogre::VertexElement *texElem = decl->findElementBySemantic(Ogre::VES_TEXTURE_COORDINATES, 0);
			unsigned char *base = static_cast<unsigned char*>(m_mainBuffer->lock(Ogre::HardwareBuffer::HBL_NORMAL));

			const float scale = -((constants::MapWidth / static_cast<float>(constants::TilesPerPage)) / 8.0f);
            const float uvScale2 = 1.0f;
            size_t index = 0;

			for (VertexID j = startY; j < endY - 1; ++j)
			{
				for (VertexID i = startX; i < endX; ++i)
				{
					Ogre::Real *pos = nullptr;
                    Ogre::Real *nrm = nullptr;
					Ogre::Real *tex = nullptr;

					posElem->baseVertexPointerToElement(base, &pos);
                    nrmElem->baseVertexPointerToElement(base, &nrm);
					texElem->baseVertexPointerToElement(base, &tex);

					float height = m_tileHeights[index];

					*pos++ = scale * static_cast<Ogre::Real>(j);
					*pos++ = scale * static_cast<Ogre::Real>(i);
					*pos++ = height;
                    
                    *nrm++ = m_tileNormals[index][0];
                    *nrm++ = m_tileNormals[index][1];
                    *nrm++ = m_tileNormals[index][2];

					*tex++ = i / 8.0f * uvScale2;
					*tex++ = j / 8.0f * uvScale2;

					if (height < minHeight) minHeight = height;
					if (height > maxHeight) maxHeight = height;

					base += m_mainBuffer->getVertexSize();
                    index++;
				}

				for (VertexID i = startX; i < endX - 1 ; ++i)
				{
					Ogre::Real *pos = nullptr;
					Ogre::Real *nrm = nullptr;
					Ogre::Real *tex = nullptr;

					posElem->baseVertexPointerToElement(base, &pos);
					nrmElem->baseVertexPointerToElement(base, &nrm);
					texElem->baseVertexPointerToElement(base, &tex);

					float height = m_tileHeights[index];

					*pos++ = scale * static_cast<Ogre::Real>(j)+scale * 0.5f;
					*pos++ = scale * static_cast<Ogre::Real>(i)+scale * 0.5f;
					*pos++ = height;
                    
                    *nrm++ = m_tileNormals[index][0];
                    *nrm++ = m_tileNormals[index][1];
                    *nrm++ = m_tileNormals[index][2];

					float uvScale = 1.0f / 8.0f;
					*tex++ = (i / 8.0f + (uvScale * 0.5f)) * uvScale2;
					*tex++ = (j / 8.0f + (uvScale * 0.5f)) * uvScale2;

					if (height < minHeight) minHeight = height;
					if (height > maxHeight) maxHeight = height;

					base += m_mainBuffer->getVertexSize();
                    index++;
				}
			}

			// One last row
			VertexID j = endY - 1;
			for (VertexID i = startX; i < endX; ++i)
			{
				Ogre::Real *pos = nullptr;
				Ogre::Real *nrm = nullptr;
				Ogre::Real *tex = nullptr;
				
				posElem->baseVertexPointerToElement(base, &pos);
				nrmElem->baseVertexPointerToElement(base, &nrm);
				texElem->baseVertexPointerToElement(base, &tex);

				float height = m_tileHeights[index];

				*pos++ = scale * static_cast<Ogre::Real>(j);
				*pos++ = scale * static_cast<Ogre::Real>(i);
				*pos++ = height;
                
                *nrm++ = m_tileNormals[index][0];
                *nrm++ = m_tileNormals[index][1];
                *nrm++ = m_tileNormals[index][2];

				*tex++ = i / 8.0f * uvScale2;
				*tex++ = 1.0f * uvScale2;

				if (height < minHeight) minHeight = height;
				if (height > maxHeight) maxHeight = height;

				base += m_mainBuffer->getVertexSize();
                index++;
			}

			m_mainBuffer->unlock();

			// Prevent minHeight == maxHeight for bounding box
			if (minHeight == maxHeight) maxHeight = minHeight + 0.1f;

			m_bounds.setExtents(
				scale * endX, scale * endY, minHeight,
				scale * startX, scale * startY, maxHeight
				);

			m_center = m_bounds.getCenter();
			m_boundingRadius = (m_bounds.getMaximum() - m_center).length();
		}

		void TerrainTile::createIndexes()
		{
			UInt16 indexCounter = 768;
			m_indexData = make_unique<Ogre::IndexData>();
			m_indexData->indexBuffer = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
				Ogre::HardwareIndexBuffer::IT_16BIT, indexCounter, Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY
				);

			UInt16 *indexPtr = static_cast<UInt16*>(m_indexData->indexBuffer->lock(
				0, m_indexData->indexBuffer->getSizeInBytes(), Ogre::HardwareBuffer::HBL_NORMAL
				));
			
			UInt16 numIndices = 0;
			for (UInt32 i = 0; i < 8; ++i)
			{
				for (UInt32 j = 0; j < 8; ++j)
				{
					// The terrain grid
					// 
					//  0------- 1...		i
					//  | \   /  |
					//	|	9    |
					//  | /   \  |
					// 17-------18...
					//  .        .
					//  .        .
					//  .        .
					//  j

					const bool isHole =
						(m_holes & holetab_h[i / 2] & holetab_v[j / 2]) != 0;
					if (isHole)
					{
						continue;
					}

					UInt16 topLeftInd = i + j * 9 + j * 8;
					UInt16 topRightInd = topLeftInd + 1;
					UInt16 centerInd = topLeftInd + 9;
					UInt16 bottomLeftInd = centerInd + 8;
					UInt16 bottomRightInd = bottomLeftInd + 1;

					// Top
					*indexPtr++ = topRightInd;
					*indexPtr++ = topLeftInd;
					*indexPtr++ = centerInd;

					// Right
					*indexPtr++ = bottomRightInd;
					*indexPtr++ = topRightInd;
					*indexPtr++ = centerInd;

					// Bottom
					*indexPtr++ = bottomLeftInd;
					*indexPtr++ = bottomRightInd;
					*indexPtr++ = centerInd;

					// Left
					*indexPtr++ = topLeftInd;
					*indexPtr++ = bottomLeftInd;
					*indexPtr++ = centerInd;

					numIndices += 12;
				}
			}

			m_indexData->indexBuffer->unlock();
			m_indexData->indexCount = numIndices;
			m_indexData->indexStart = 0;
		}
	}
}
