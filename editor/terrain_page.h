

#pragma once

#include "common/typedefs.h"
#include "common/grid.h"
#include "terrain_editing.h"
#include <functional>
#include <memory>
#include "OgreTexture.h"
#include "OgreMaterial.h"

namespace Ogre
{
	class SceneManager;
	class Camera;
	class SceneNode;
}

namespace wowpp
{
	namespace view
	{		
		class TerrainTile;

		class TerrainPage final
		{
		public:

			explicit TerrainPage(
				const terrain::editing::PositionedPage &data, 
				Ogre::SceneManager &sceneMgr,
				Ogre::Camera &camera,
				Ogre::SceneNode &worldNode);
			~TerrainPage();
			
		private:

			typedef Grid<std::unique_ptr<TerrainTile>> TileGrid;

			Ogre::SceneManager &m_sceneMgr;
			Ogre::MaterialPtr m_material;
			terrain::editing::PositionedPage m_data;
			Ogre::SceneNode *m_sceneNode;
			Ogre::Camera &m_camera;
			TileGrid m_tiles;
			Ogre::TexturePtr m_terrainTexture;
			std::vector<Ogre::TexturePtr> m_texturePtrs;
		};
	}
}
