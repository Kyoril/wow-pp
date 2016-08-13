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
#include "world_renderer.h"
#include "terrain_model.h"
#include "common/make_unique.h"
#include "OgreSceneManager.h"
#include "OgreCamera.h"
#include "OgreTextureManager.h"
#include "log/default_log_levels.h"
#include "ogre_wrappers/ogre_m2_loader.h"
#include <OgreMeshManager.h>
#include <OgreEntity.h>
#include <OgreSceneNode.h>

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
						// Code below not yet ready
						break;

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

						Ogre::Vector3 position(
							placement.position[0],
							placement.position[1],
							placement.position[2]);
						position.x = 32.0f * 533.33333f - position.x;
						position.z = 32.0f * 533.33333f - position.z;

						node->setPosition(0.0f, 0.0f, 0.0f);
						node->setOrientation(Ogre::Quaternion::IDENTITY);
						node->setScale(1.0f, 1.0f, 1.0f);

						Ogre::Quaternion rot = Ogre::Quaternion::IDENTITY;
						rot = rot * Ogre::Quaternion(Ogre::Degree(90), Ogre::Vector3::UNIT_X);
						rot = rot * Ogre::Quaternion(Ogre::Degree(90), Ogre::Vector3::UNIT_Y);
						node->setOrientation(rot);

						node->translate(position, Ogre::Node::TS_LOCAL);

						Ogre::Quaternion ownRot = Ogre::Quaternion::IDENTITY;
						ownRot = ownRot * Ogre::Quaternion(Ogre::Degree(placement.rotation[1] - 270), Ogre::Vector3::UNIT_Y);
						ownRot = ownRot * Ogre::Quaternion(Ogre::Degree(placement.position[0]), Ogre::Vector3::UNIT_Z);
						ownRot = ownRot * Ogre::Quaternion(Ogre::Degree(placement.position[2] - 90), Ogre::Vector3::UNIT_X);
						node->rotate(ownRot, Ogre::Node::TS_PARENT);

						/*node->rotate(Ogre::Quaternion(Ogre::Degree(placement.rotation[1] - 270), Ogre::Vector3::UNIT_Y), Ogre::Node::TS_PARENT);
						node->rotate(Ogre::Quaternion(Ogre::Degree(-placement.position[0]), Ogre::Vector3::UNIT_Z), Ogre::Node::TS_PARENT);
						node->rotate(Ogre::Quaternion(Ogre::Degree(placement.position[2] - 90), Ogre::Vector3::UNIT_X), Ogre::Node::TS_PARENT);*/

						node->scale(Ogre::Vector3(placement.scale / 1024.0f));
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
