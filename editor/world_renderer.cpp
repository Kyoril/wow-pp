
#include "world_renderer.h"
#include "terrain_model.h"
#include "common/make_unique.h"
#include "OgreSceneManager.h"
#include "OgreCamera.h"
#include <cassert>

namespace wowpp
{
	namespace view
	{
		WorldRenderer::WorldRenderer(Ogre::SceneManager &sceneMgr, Ogre::Camera &camera)
			: m_sceneMgr(sceneMgr)
			, m_camera(camera)
		{
		}

		void WorldRenderer::handleEvent(const terrain::editing::TerrainChangeEvent &event)
		{
			if (const terrain::editing::PageAdded * addEvent = boost::get<terrain::editing::PageAdded>(&event))
			{
				auto it = m_terrainPages.find(addEvent->added.position);
				if (it == m_terrainPages.end())
				{
					auto page = std::make_shared<TerrainPage>(
						addEvent->added,
						m_sceneMgr,
						m_camera,
						*m_sceneMgr.getRootSceneNode()
						);
					m_terrainPages[addEvent->added.position] = page;
				}
			}
			else if (const terrain::editing::PageRemoved * removeEvent = boost::get<terrain::editing::PageRemoved>(&event))
			{
				// Page removed
				auto it = m_terrainPages.find(removeEvent->removed);
				if (it != m_terrainPages.end())
				{
					m_terrainPages.erase(it);
				}
			}
		}
		
		void WorldRenderer::update(float delta)
		{

		}
	}
}
