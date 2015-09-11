
#include "world_renderer.h"
#include "terrain_model.h"
#include "common/make_unique.h"
#include "OgreSceneManager.h"
#include "OgreCamera.h"
#include "OgreTextureManager.h"
#include "log/default_log_levels.h"
#include "ogre_m2_loader.h"
#include <OgreMeshManager.h>
#include <OgreEntity.h>
#include <OgreSceneNode.h>
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

					for (const auto &placement : addEvent->added.page->m2Placements)
					{
						if (placement.mmidEntry >= addEvent->added.page->m2Ids.size())
						{
							ELOG("INVALID M2 ID");
							continue;
						}

						const Ogre::String &filename = addEvent->added.page->m2Ids[placement.mmidEntry];
						
						Ogre::String base, ext;
						Ogre::StringUtil::splitBaseFilename(filename, base, ext);
						
						Ogre::String realFileName = base + ".m2";

						Ogre::MeshPtr mesh;
						if (!Ogre::MeshManager::getSingleton().resourceExists(realFileName))
						{
							mesh = Ogre::MeshManager::getSingleton().createManual(realFileName, "WoW", new editor::M2MeshLoader(realFileName));
							mesh->load();
						}
						else
						{
							mesh = Ogre::MeshManager::getSingleton().getByName(realFileName, "WoW");
						}

						if (mesh.isNull())
						{
							ELOG("Could not find mesh file");
							continue;
						}

						Ogre::Vector3 pos(placement.position[2], placement.position[0], placement.position[1]);
						
						static size_t index = 0;
						Ogre::Entity *ent = m_sceneMgr.createEntity("Doodad_" + Ogre::StringConverter::toString(index++), mesh);
						Ogre::SceneNode *node = m_sceneMgr.getRootSceneNode()->createChildSceneNode();
						node->attachObject(ent);
						node->setPosition(pos);
						
						/*
						Ogre::Quaternion rotX(Ogre::Degree(placement.rotation[1] - 90.0f), Ogre::Vector3::UNIT_Y);
						Ogre::Quaternion rotY(Ogre::Degree(-placement.rotation[0]), Ogre::Vector3::UNIT_Z);
						Ogre::Quaternion rotZ(Ogre::Degree(placement.rotation[2]), Ogre::Vector3::UNIT_X);
						node->setOrientation(rotY * rotX * rotZ);
						*/
						DLOG("Position: " << Ogre::StringConverter::toString(pos) << "; Page: " << addEvent->added.position[0] << " " << addEvent->added.position[1]);

						node->setScale(Ogre::Vector3(placement.scale / 1024.0f));
					}

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

					auto list = Ogre::ResourceGroupManager::getSingleton().getResourceGroups();
					for (auto &name : list)
					{
						Ogre::ResourceGroupManager::getSingleton().unloadUnreferencedResourcesInGroup(name, false);
					}
				}
			}
		}
		
		void WorldRenderer::update(float delta)
		{
			for (auto &page : m_terrainPages)
			{
				page.second->update();
			}
		}
	}
}
