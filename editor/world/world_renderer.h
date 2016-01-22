
#pragma once

#include "common/typedefs.h"
#include "common/vector.h"
#include "terrain_editing.h"
#include "terrain_page.h"
#include "ogre_wrappers/entity_ptr.h"
#include "ogre_wrappers/scene_node_ptr.h"
#include <boost/noncopyable.hpp>

namespace wowpp
{
	namespace view
	{
		class WorldRenderer : public boost::noncopyable
		{
		public:

			explicit WorldRenderer(Ogre::SceneManager &sceneMgr, Ogre::Camera &camera);
			
			void handleEvent(const terrain::editing::TerrainChangeEvent &event);
			void update(float delta);

		private:

			typedef std::shared_ptr<TerrainPage> TerrainPagePtr;
			typedef std::map<terrain::model::PagePosition, TerrainPagePtr> TerrainPages;

			Ogre::SceneManager &m_sceneMgr;
			Ogre::Camera &m_camera;
			TerrainPages m_terrainPages;
		};
	}
}
