
#include "terrain_tile.h"
#include "terrain_page.h"
#include "common/constants.h"
#include "common/make_unique.h"
#include "game/constants.h"
#include "OgreSceneManager.h"
#include "OgreMath.h"
#include "OgreHardwareBufferManager.h"
#include "OgreHardwareBuffer.h"
#include <cassert>

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
			terrain::model::Heightmap &tileHeights
			)
			: Ogre::MovableObject(name)
			, Ogre::Renderable()
			, m_sceneMgr(sceneMgr)
			, m_camera(camera)
			, m_page(page)
			, m_material(material)
			, m_tileX(tileX)
			, m_tileY(tileY)
			, m_boundingRadius(0.0f)
			, m_lightListDirty(true)
			, m_tileHeights(tileHeights)
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
			assert(n);

			return n->_getDerivedOrientation();
		}

		const Ogre::Vector3 & TerrainTile::getWorldPosition() const
		{
			Ogre::Node *n = getParentNode();
			assert(n);

			return n->_getDerivedPosition();
		}

		Ogre::Real TerrainTile::getSquaredViewDepth(const Ogre::Camera *cam) const
		{
			Ogre::Node *n = getParentNode();
			assert(n);

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
			
			Ogre::Real minHeight = std::numeric_limits<Ogre::Real>::max();
			Ogre::Real maxHeight = std::numeric_limits<Ogre::Real>::min();

			const Ogre::VertexElement *posElem = decl->findElementBySemantic(Ogre::VES_POSITION);
			unsigned char *base = static_cast<unsigned char*>(m_mainBuffer->lock(Ogre::HardwareBuffer::HBL_NORMAL));

			const float scale = (constants::MapWidth / static_cast<float>(constants::TilesPerPage)) / 8.0f;
			
			for (VertexID j = startY; j < endY - 1; ++j)
			{
				for (VertexID i = startX; i < endX; ++i)
				{
					Ogre::Real *pos = nullptr;

					posElem->baseVertexPointerToElement(base, &pos);

					int relX = i - startX;
					int relY = j - startY;
					int heightInd = relX + relY * 9;
					heightInd += relY * 8;

					float height = m_tileHeights[heightInd];

					*pos++ = scale * static_cast<Ogre::Real>(j);
					*pos++ = height;
					*pos++ = scale * static_cast<Ogre::Real>(i);

					if (height < minHeight) minHeight = height;
					if (height > maxHeight) maxHeight = height;

					base += m_mainBuffer->getVertexSize();
				}

				for (VertexID i = startX; i < endX - 1 ; ++i)
				{
					Ogre::Real *pos = nullptr;

					posElem->baseVertexPointerToElement(base, &pos);

					int relX = i - startX;
					int relY = j - startY;
					int heightInd = relX + relY * 9;
					heightInd += relY * 8;

					float height = m_tileHeights[heightInd];

					*pos++ = scale * static_cast<Ogre::Real>(j)+scale * 0.5f;
					*pos++ = height;
					*pos++ = scale * static_cast<Ogre::Real>(i)+scale * 0.5f;

					if (height < minHeight) minHeight = height;
					if (height > maxHeight) maxHeight = height;

					base += m_mainBuffer->getVertexSize();
				}
			}

			// One last row
			VertexID j = endY - 1;
			for (VertexID i = startX; i < endX; ++i)
			{
				Ogre::Real *pos = nullptr;

				posElem->baseVertexPointerToElement(base, &pos);

				int relX = i - startX;
				int relY = j - startY;
				int heightInd = relX + relY * 9;
				heightInd += relY * 8;

				float height = m_tileHeights[heightInd];

				*pos++ = scale * static_cast<Ogre::Real>(j);
				*pos++ = height;
				*pos++ = scale * static_cast<Ogre::Real>(i);

				if (height < minHeight) minHeight = height;
				if (height > maxHeight) maxHeight = height;

				base += m_mainBuffer->getVertexSize();
			}

			m_mainBuffer->unlock();

			// Prevent minHeight == maxHeight for bounding box
			if (minHeight == maxHeight) maxHeight = minHeight + 0.1f;

			m_bounds.setExtents(
				scale * startX,	minHeight, scale * startY,
				scale * endX,			maxHeight, scale * endY
				);

			m_center = m_bounds.getCenter();
			m_boundingRadius = (m_bounds.getMaximum() - m_center).length();
		}

		void TerrainTile::createIndexes()
		{
			m_indexData = make_unique<Ogre::IndexData>();
			m_indexData->indexBuffer = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
				Ogre::HardwareIndexBuffer::IT_16BIT, 768, Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY
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

					UInt16 topLeftInd = i + j * 9 + j * 8;
					UInt16 topRightInd = topLeftInd + 1;
					UInt16 centerInd = topLeftInd + 9;
					UInt16 bottomLeftInd = centerInd + 8;
					UInt16 bottomRightInd = bottomLeftInd + 1;

					// Top
					*indexPtr++ = topLeftInd;
					*indexPtr++ = topRightInd;
					*indexPtr++ = centerInd;

					// Right
					*indexPtr++ = topRightInd;
					*indexPtr++ = bottomRightInd;
					*indexPtr++ = centerInd;

					// Bottom
					*indexPtr++ = bottomRightInd;
					*indexPtr++ = bottomLeftInd;
					*indexPtr++ = centerInd;

					// Left
					*indexPtr++ = bottomLeftInd;
					*indexPtr++ = topLeftInd;
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
