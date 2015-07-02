
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
			Ogre::Camera &m_camera;
			TerrainPage &m_page;
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
