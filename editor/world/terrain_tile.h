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
#include "terrain_model.h"
#include "OgreMovableObject.h"
#include "OgreRenderable.h"

namespace wowpp
{
	namespace view
	{
		class TerrainPage;

		class TerrainTile final
			: public Ogre::MovableObject
			, public Ogre::Renderable
		{
		public:

			typedef size_t VertexID;

			explicit TerrainTile(
				Ogre::SceneManager &sceneMgr,
				Ogre::Camera &camera,
				const Ogre::String &name, 
				TerrainPage &page, 
				Ogre::MaterialPtr material,
				UInt32 tileX, 
				UInt32 tileY,
				terrain::model::Heightmap &tileHeights,
                terrain::model::Normalmap &tileNormals);
			~TerrainTile();

			Ogre::uint32 getTypeFlags() const override;
			const Ogre::MaterialPtr &getMaterial() const override;
			void getRenderOperation(Ogre::RenderOperation &op) override;
			void getWorldTransforms(Ogre::Matrix4 *m) const override;
			const Ogre::Quaternion &getWorldOrientation() const;
			const Ogre::Vector3 &getWorldPosition() const;
			Ogre::Real getSquaredViewDepth(const Ogre::Camera *cam) const override;
			const Ogre::LightList &getLights() const override;
			const Ogre::String &getMovableType() const override;
			const Ogre::AxisAlignedBox &getBoundingBox() const override;
			Ogre::Real getBoundingRadius() const override;
			void _updateRenderQueue(Ogre::RenderQueue *queue) override;
			void visitRenderables(Ogre::Renderable::Visitor *visitor, bool debugRenderables) override;
			bool getCastsShadows() const override;

		private:

			void createVertexData();
			void createIndexes();
			
		private:

			Ogre::SceneManager &m_sceneMgr;
			//Ogre::Camera &m_camera;
			//TerrainPage &m_page;
			Ogre::MaterialPtr m_material;
			const UInt32 m_tileX;
			const UInt32 m_tileY;
			Ogre::AxisAlignedBox m_bounds;
			float m_boundingRadius;
			Ogre::Vector3 m_center;
			mutable bool m_lightListDirty;
			mutable Ogre::LightList m_lightList;
			Ogre::HardwareVertexBufferSharedPtr m_mainBuffer;
			std::unique_ptr<Ogre::VertexData> m_vertexData;
			std::unique_ptr<Ogre::IndexData> m_indexData;
			terrain::model::Heightmap &m_tileHeights;
            terrain::model::Normalmap &m_tileNormals;
		};
	}
}
