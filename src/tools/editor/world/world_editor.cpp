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
#include "world_editor.h"
#include "proto_data/project.h"
#include "common/typedefs.h"
#include "common/constants.h"
#include "common/make_unique.h"
#include "game/constants.h"
#include "log/default_log_levels.h"
#include "OgreCamera.h"
#include "OgreLight.h"
#include "OgreSceneManager.h"
#include "OgreSceneNode.h"
#include "ogre_wrappers/ogre_m2_loader.h"
#include "game/map.h"
#include "editor_application.h"
#include "selected_creature_spawn.h"
#include "selected_object_spawn.h"
#include "windows/spawn_dialog.h"
#include "deps/debug_utils/DetourDebugDraw.h"
#include "math/aabb_tree.h"

namespace wowpp
{
	namespace editor
	{
		WorldEditor::WorldEditor(EditorApplication &app, Ogre::SceneManager &sceneMgr, Ogre::Camera &camera, proto::MapEntry &map, proto::Project &project)
			: m_app(app)
			, m_sceneMgr(sceneMgr)
			, m_camera(camera)
			, m_map(map)
			, m_work(new boost::asio::io_service::work(m_workQueue))
            , m_light(nullptr)
			, m_project(project)
			, m_previousPage(std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max())
			, m_startSet(false)
			, m_nextUnitSpawn(0)
		{
			// Create worker thread
			boost::asio::io_service &workQueue = m_workQueue;
			boost::asio::io_service &dispatcher = m_dispatcher;
			m_worker = std::thread([&workQueue]()
			{
				workQueue.run();
			});

			m_debugDraw.reset(new OgreDebugDraw(m_sceneMgr));

			// Move camera to first spawn position (if any)
			if (!m_map.unitspawns().empty())
			{
				const auto &spawn = m_map.unitspawns().begin();
				m_camera.setPosition(spawn->positionx(), spawn->positiony(), spawn->positionz() + 5.0f);
			}

			m_displayDbc = OgreDBCFileManager::getSingleton().load("DBFilesClient\\CreatureDisplayInfo.dbc", "WoW");
			m_modelDbc = OgreDBCFileManager::getSingleton().load("DBFilesClient\\CreatureModelData.dbc", "WoW");
			m_objDisplayDbc = OgreDBCFileManager::getSingleton().load("DBFilesClient\\GameObjectDisplayInfo.dbc", "WoW");

			// Spawn all entities
			UInt32 i = 0;
			for (auto &spawn : *m_map.mutable_unitspawns())
			{
				addUnitSpawn(spawn, false);
			}

			i = 0;
			for (auto &spawn : *m_map.mutable_objectspawns())
			{
				const auto *object = m_project.objects.getById(spawn.objectentry());
				ASSERT(object);

				UInt32 row = m_objDisplayDbc->getRowByIndex(object->displayid());
				if (row == UInt32(-1))
				{
					//WLOG("Could not find object display id " << object->displayid());
					continue;
				}

				String objectFileName = m_objDisplayDbc->getField(row, 1);
				if (objectFileName.empty())
				{
					WLOG("Could not find object display ID file!");
					continue;
				}

				std::replace(objectFileName.begin(), objectFileName.end(), '/', '\\');

				String base, ext;
				Ogre::StringUtil::splitBaseFilename(objectFileName, base, ext);
				objectFileName = base + ".m2";

				Ogre::MeshPtr mesh;
				if (!Ogre::MeshManager::getSingleton().resourceExists(objectFileName))
				{
					try
					{
						mesh = Ogre::MeshManager::getSingleton().createManual(objectFileName, "WoW", new editor::M2MeshLoader(objectFileName));
						mesh->load();
					}
					catch (const Ogre::Exception &)
					{
						continue;
					}
				}
				else
				{
					mesh = Ogre::MeshManager::getSingleton().getByName(objectFileName, "WoW");
				}

				try
				{
					Ogre::Entity *ent = m_sceneMgr.createEntity("ObjSpawn_" + Ogre::StringConverter::toString(i++), mesh);
					ent->setUserAny(Ogre::Any(&spawn));

					m_spawnEntities.push_back(ogre_utils::EntityPtr(ent));
					Ogre::SceneNode *node = m_sceneMgr.getRootSceneNode()->createChildSceneNode();
					node->attachObject(ent);
					node->setPosition(spawn.positionx(), spawn.positiony(), spawn.positionz());
					node->setOrientation(Ogre::Quaternion(spawn.rotationz(), spawn.rotationw(), spawn.rotationx(), spawn.rotationy()));
					m_spawnNodes.push_back(ogre_utils::SceneNodePtr(node));
				}
				catch (const Ogre::Exception &)
				{
					continue;
				}
			}

			const Ogre::Vector3 &camPos = m_camera.getDerivedPosition();
			float convertedX = (constants::MapWidth * 32.0f) + camPos.x;
			float convertedY = (constants::MapWidth * 32.0f) + camPos.y;

			paging::PagePosition pos(63 - static_cast<size_t>(convertedX / constants::MapWidth), 63 - static_cast<size_t>(convertedY / constants::MapWidth));
			const auto addWork = [&workQueue](const WorldPageLoader::Work &work)
			{
                workQueue.post(work);
			};

			const auto synchronize = [&dispatcher](const WorldPageLoader::Work &work)
			{
				dispatcher.post(work);
			};

			m_visibleSection = make_unique<paging::LoadedPageSection>(pos, 1, *this);
			m_pageLoader = make_unique<WorldPageLoader>(
				*m_visibleSection.get(),
				addWork,
				synchronize
				);

			paging::PagePosition worldSize(constants::PagesPerMap, constants::PagesPerMap);
			m_memoryPointOfView = make_unique<paging::PagePOVPartitioner>(
				worldSize,
				1,
				pos,
				*m_pageLoader.get());

			// Create the world renderer
			m_worldRenderer.reset(new view::WorldRenderer(
				m_sceneMgr,
				m_camera));
            
			m_light = m_sceneMgr.createLight("Sun");
            m_light->setType(Ogre::Light::LT_DIRECTIONAL);
            m_light->setDirection(Ogre::Vector3(0.0f, 0.5f, -0.5f).normalisedCopy());
			m_sceneMgr.getRootSceneNode()->attachObject(m_light);
			m_camera.setFarClipDistance(533.3333f);
			m_sceneMgr.setFog(Ogre::FOG_LINEAR, m_camera.getViewport()->getBackgroundColour(),
				0.001f, 450.0f, 512.0f);

			m_mapInst = std::make_unique<Map>(m_map, m_app.getConfiguration().dataPath, true);

			// Create transform widget
			m_transformWidget = std::make_unique<TransformWidget>(
				m_app.getSelection(),
				m_sceneMgr,
				m_camera);

			// Watch for transform changes
			m_onTransformChanged = m_app.transformToolChanged.connect(this, &WorldEditor::onTransformToolChanged);
			onTransformToolChanged(m_app.getTransformTool());

			m_onShowNavMesh = m_app.showNavMesh.connect([this]() {
				const auto *navMesh = m_mapInst->getNavMesh();
				if (navMesh)
				{
					if (m_debugDraw.get())
					{
						m_debugDraw->clear();
						m_debugDraw.reset();
					}
					else
					{
						m_debugDraw.reset(new OgreDebugDraw(m_sceneMgr));
						duDebugDrawNavMesh(m_debugDraw.get(), *navMesh, 0);
					}
				}
			});
		}

		WorldEditor::~WorldEditor()
		{
			// All selected objects should be from this editor instance, and we are about to destroy it
			// So deselect all objects before, to prevent a crash on next selection
			m_app.getSelection().clear();

            if (m_light)
            {
				m_sceneMgr.destroyLight(m_light);
                m_light = nullptr;
            }
            
			// Stop paging
			m_work.reset();
			m_workQueue.stop();
			m_worker.join();

			// Unload all nav meshs manually here to prevent a crash in the editor at shutdown
			if (m_mapInst)
			{
				m_mapInst->unloadAllTiles();
			}
		}

		void WorldEditor::update(float delta)
		{
			m_transformWidget->update(&m_camera);

			const Ogre::Vector3 &camPos = m_camera.getDerivedPosition();
			float convertedX = (constants::MapWidth * 32.0f) + camPos.x;
			float convertedY = (constants::MapWidth * 32.0f) + camPos.y;

			paging::PagePosition pos(63 - static_cast<size_t>(convertedX / constants::MapWidth), 63 - static_cast<size_t>(convertedY / constants::MapWidth));
			if (m_previousPage != pos)
			{
				m_previousPage = pos;

				m_memoryPointOfView->updateCenter(pos);
				m_visibleSection->updateCenter(pos);

				pageChanged(pos);
			}

			// Update paging
			m_dispatcher.poll();

			m_worldRenderer->update(delta);
		}

		void WorldEditor::save()
		{
			//TODO
		}

		void WorldEditor::onPageLoad(const paging::Page &page)
		{
			const auto &pos = page.getPosition();

			// Add the page if needed
			auto it = m_pages.find(pos);
			if (it == m_pages.end())
			{
				const Ogre::String fileName =
					"World\\Maps\\" + m_map.directory() + "\\" + m_map.directory() + "_" +
					Ogre::StringConverter::toString(pos[1], 1, '0') + "_" +
					Ogre::StringConverter::toString(pos[0], 1, '0') + ".adt";

				if (!Ogre::ResourceGroupManager::getSingleton().resourceExistsInAnyGroup(fileName))
				{
					//WLOG("Page not found: " << fileName);
					return;
				}

				Ogre::DataStreamPtr file = Ogre::ResourceGroupManager::getSingleton().openResource(fileName);
				std::unique_ptr<adt::Page> page = make_unique<adt::Page>();
				adt::load(file, *page);

				// Store page
				m_pages[pos] = std::move(page->terrain);
			}
		}

		void WorldEditor::onPageAvailabilityChanged(const paging::PageNeighborhood &pages, bool isAvailable)
		{
			auto &mainPage = pages.getMainPage();
			const auto &pos = mainPage.getPosition();

			std::ostringstream objName;
			objName << "TILE_" << pos;

			if (isAvailable)
			{
				// Add the page if needed
				auto it = m_pages.find(pos);
				if (it != m_pages.end())
				{
					// Add terrain page
					terrain::editing::PageAdded add;
					add.added.page = &m_pages[pos];
					add.added.position = pos;
					m_worldRenderer->handleEvent(terrain::editing::TerrainChangeEvent(add));
					
					auto *tile = m_mapInst->getTile(TileIndex2D(pos[0], pos[1]));
					if (!tile)
					{
						WLOG("Could not load tile!");
						return;
					}

					// Iterate through all wmos and check if they are loaded
					for (const auto &entry : tile->wmos.entries)
					{
						auto countIt = m_wmoRefCount.find(entry.uniqueId);
						if (countIt == m_wmoRefCount.end())
						{
							// Load wmo geometry from aabbtree
							auto tree = m_mapInst->getWMOTree(entry.fileName);
							if (tree.get())
							{
								const auto &verts = tree->getVertices();
								const auto &inds = tree->getIndices();

								std::vector<math::Vector3> normals;
								normals.resize(verts.size());

								// Code for smooth normal generation
								for (UInt32 i = 0; i < inds.size(); i += 3)
								{
									auto& v0 = verts[inds[i + 0]];
									auto& v1 = verts[inds[i + 1]];
									auto& v2 = verts[inds[i + 2]];

									math::Vector3 u = v1 - v0;
									math::Vector3 v = v2 - v0;
									math::Vector3 n = u.cross(v);
									n.normalize();

									normals[inds[i + 0]] += n;
									normals[inds[i + 1]] += n;
									normals[inds[i + 2]] += n;
								}

								for (uint i = 0; i < normals.size(); ++i) {
									normals[i].normalize();
								}

								// Build unique object name
								std::ostringstream objStrm;
								objStrm << "WMO_" << entry.uniqueId;

								// Create collision render object
								ogre_utils::ManualObjectPtr obj(m_sceneMgr.createManualObject(objStrm.str()));
								{
									obj->begin("LineOfSightBlock", Ogre::RenderOperation::OT_TRIANGLE_LIST);
									obj->estimateVertexCount(verts.size());
									obj->estimateIndexCount(inds.size() * 3);

									for (UInt32 i = 0; i < verts.size(); ++i)
									{
										obj->position(verts[i].x, verts[i].y, verts[i].z);
										obj->colour(Ogre::ColourValue(0.5f, 0.5f, 0.5f));
										obj->normal(normals[i].x, normals[i].y, normals[i].z);
									}

									UInt32 triIndex = 0;
									for (UInt32 i = 0; i < inds.size(); i += 3)
									{
										obj->index(inds[i]);
										obj->index(inds[i + 1]);
										obj->index(inds[i + 2]);
										triIndex++;
									}
									obj->end();
								}

								// Create scene node with proper transformation and attach the render object to it!
								Ogre::SceneNode *child = m_sceneMgr.getRootSceneNode()->createChildSceneNode(objStrm.str());
								Ogre::Matrix4 transform(
									entry.inverse.m[0][0], entry.inverse.m[0][1], entry.inverse.m[0][2], entry.inverse.m[0][3],
									entry.inverse.m[1][0], entry.inverse.m[1][1], entry.inverse.m[1][2], entry.inverse.m[1][3],
									entry.inverse.m[2][0], entry.inverse.m[2][1], entry.inverse.m[2][2], entry.inverse.m[2][3],
									entry.inverse.m[3][0], entry.inverse.m[3][1], entry.inverse.m[3][2], entry.inverse.m[3][3]);
								transform = transform.inverse();
								child->setPosition(transform.getTrans());
								child->setOrientation(transform.extractQuaternion());
								child->attachObject(obj.get());

								// Assign geometry
								m_wmoGeometry[entry.uniqueId] = std::move(obj);
							}
							else
							{
								WLOG("WMO tree " << entry.fileName << " not loaded");
							}

							// Setup reference counter
							m_wmoRefCount[entry.uniqueId] = 1;
						}
						else
						{
							// Simply increase reference counter
							countIt->second++;
						}
					}

					// Iterate through all doodads and check if they are loaded
					for (const auto &entry : tile->doodads.entries)
					{
						auto countIt = m_doodadRefCount.find(entry.uniqueId);
						if (countIt == m_doodadRefCount.end())
						{
							// Load doodad geometry from aabbtree
							auto tree = m_mapInst->getDoodadTree(entry.fileName);
							if (tree.get())
							{
								const auto &verts = tree->getVertices();
								const auto &inds = tree->getIndices();

								std::vector<math::Vector3> normals;
								normals.resize(verts.size());

								// Code for smooth normal generation
								for (UInt32 i = 0; i < inds.size(); i += 3)
								{
									auto& v0 = verts[inds[i + 0]];
									auto& v1 = verts[inds[i + 1]];
									auto& v2 = verts[inds[i + 2]];

									math::Vector3 u = v1 - v0;
									math::Vector3 v = v2 - v0;
									math::Vector3 n = u.cross(v);
									n.normalize();

									normals[inds[i + 0]] += n;
									normals[inds[i + 1]] += n;
									normals[inds[i + 2]] += n;
								}

								for (uint i = 0; i < normals.size(); ++i) {
									normals[i].normalize();
								}

								// Build unique object name
								std::ostringstream objStrm;
								objStrm << "DOODAD_" << entry.uniqueId;

								// Create collision render object
								ogre_utils::ManualObjectPtr obj(m_sceneMgr.createManualObject(objStrm.str()));
								{
									obj->begin("LineOfSightBlock", Ogre::RenderOperation::OT_TRIANGLE_LIST);
									obj->estimateVertexCount(verts.size());
									obj->estimateIndexCount(inds.size() * 3);

									for (UInt32 i = 0; i < verts.size(); ++i)
									{
										obj->position(verts[i].x, verts[i].y, verts[i].z);
										obj->colour(Ogre::ColourValue(0.0f, 0.5f, 0.0f));
										obj->normal(normals[i].x, normals[i].y, normals[i].z);
									}

									UInt32 triIndex = 0;
									for (UInt32 i = 0; i < inds.size(); i += 3)
									{
										obj->index(inds[i]);
										obj->index(inds[i + 1]);
										obj->index(inds[i + 2]);
										triIndex++;
									}
									obj->end();
								}

								// Create scene node with proper transformation and attach the render object to it!
								Ogre::SceneNode *child = m_sceneMgr.getRootSceneNode()->createChildSceneNode(objStrm.str());
								Ogre::Matrix4 transform(
									entry.inverse.m[0][0], entry.inverse.m[0][1], entry.inverse.m[0][2], entry.inverse.m[0][3],
									entry.inverse.m[1][0], entry.inverse.m[1][1], entry.inverse.m[1][2], entry.inverse.m[1][3],
									entry.inverse.m[2][0], entry.inverse.m[2][1], entry.inverse.m[2][2], entry.inverse.m[2][3],
									entry.inverse.m[3][0], entry.inverse.m[3][1], entry.inverse.m[3][2], entry.inverse.m[3][3]);
								transform = transform.inverse();

								child->setScale(transform[0][0], transform[1][1], transform[2][2]);
								Ogre::Matrix3 rot;
								transform.extract3x3Matrix(rot);
								child->setOrientation(Ogre::Quaternion(rot));
								child->setPosition(transform.getTrans());
								child->attachObject(obj.get());

								// Assign geometry
								m_doodadGeometry[entry.uniqueId] = std::move(obj);
							}

							// Setup reference counter
							m_doodadRefCount[entry.uniqueId] = 1;
						}
						else
						{
							// Simply increase reference counter
							countIt->second++;
						}
					}
				}
				else
				{
					WLOG("Page not loaded: " << pos);
				}
			}
			else
			{
				// Add the page if needed
				auto it = m_pages.find(pos);
				if (it != m_pages.end())
				{
					// Remove terrain page
					terrain::editing::PageRemoved remove;
					remove.removed = pos;
					m_worldRenderer->handleEvent(terrain::editing::TerrainChangeEvent(remove));

					auto *tile = m_mapInst->getTile(TileIndex2D(pos[0], pos[1]));
					if (tile)
					{
						// Iterate through all wmos and check if they are loaded
						for (const auto &entry : tile->wmos.entries)
						{
							auto countIt = m_wmoRefCount.find(entry.uniqueId);
							if (countIt != m_wmoRefCount.end())
							{
								// Simply decrese reference counter
								countIt->second--;
								if (countIt->second == 0)
								{
									// Destroy loaded geometry
									auto geomIt = m_wmoGeometry.find(entry.uniqueId);
									if (geomIt != m_wmoGeometry.end())
									{
										if (geomIt->second->getParentSceneNode())
										{
											m_sceneMgr.destroySceneNode(geomIt->second->getParentSceneNode());
										}
										m_wmoGeometry.erase(geomIt);
									}
									
									// Remove reference counter
									m_wmoRefCount.erase(countIt);
								}
							}
						}

						// Iterate through all wmos and check if they are loaded
						for (const auto &entry : tile->doodads.entries)
						{
							auto countIt = m_doodadRefCount.find(entry.uniqueId);
							if (countIt != m_doodadRefCount.end())
							{
								// Simply decrese reference counter
								countIt->second--;
								if (countIt->second == 0)
								{
									// Destroy loaded geometry
									auto geomIt = m_doodadGeometry.find(entry.uniqueId);
									if (geomIt != m_doodadGeometry.end())
									{
										if (geomIt->second->getParentSceneNode())
										{
											m_sceneMgr.destroySceneNode(geomIt->second->getParentSceneNode());
										}
										m_doodadGeometry.erase(geomIt);
									}

									// Remove reference counter
									m_doodadRefCount.erase(countIt);
								}
							}
						}
					}

					// Remove page from list
					m_pages.erase(it);

					if (m_sceneMgr.hasSceneNode(objName.str()))
					{
						auto *node = m_sceneMgr.getSceneNode(objName.str());
						if (node)
						{
							node->removeAndDestroyAllChildren();
							m_sceneMgr.destroySceneNode(node);
						}
					}

					if (m_sceneMgr.hasManualObject(objName.str()))
					{
						m_sceneMgr.destroyManualObject(objName.str());
					}
				}
			}
		}

		terrain::model::Page * WorldEditor::getTerrainPage(terrain::model::PagePosition position)
		{
			// Add the page if needed
			auto it = m_pages.find(position);
			if (it != m_pages.end())
			{
				return &it->second;
			}

			return nullptr;
		}

		void WorldEditor::onTransformToolChanged(TransformTool tool)
		{
			if (tool == transform_tool::Select)
			{
				m_transformWidget->setVisible(false);
			}
			else
			{
				m_transformWidget->setVisible(true);
				switch (tool)
				{
					case transform_tool::Translate:
						m_transformWidget->setTransformMode(transform_mode::Translate);
						break;
					case transform_tool::Rotate:
						m_transformWidget->setTransformMode(transform_mode::Rotate);
						break;
					case transform_tool::Scale:
						m_transformWidget->setTransformMode(transform_mode::Scale);
						break;
					default:
						break;
				}
			}
		}

		void WorldEditor::addUnitSpawn(proto::UnitSpawnEntry & spawn, bool select)
		{
			const auto *unit = m_project.units.getById(spawn.unitentry());
			if (!unit)
			{
				WLOG("Spawn point of non-existant unit " << spawn.unitentry() << " found");
				return;
			}
			ASSERT(unit);

			UInt32 row = m_displayDbc->getRowByIndex(unit->malemodel());
			if (row == UInt32(-1))
			{
				//WLOG("Could not find creature display id " << unit->malemodel());
				return;
			}

			UInt32 modelId = m_displayDbc->getField<UInt32>(row, 1);
			UInt32 textureId = m_displayDbc->getField<UInt32>(row, 3);

			UInt32 modelRow = m_modelDbc->getRowByIndex(modelId);
			if (modelRow == UInt32(-1))
			{
				WLOG("Could not find model id");
				return;
			}

			String base, ext, path;
			String text = m_modelDbc->getField(modelRow, 2);
			Ogre::StringUtil::splitFullFilename(text, base, ext, path);
			std::replace(path.begin(), path.end(), '/', '\\');
			String dbcSkin = m_displayDbc->getField(row, 6);
			String skin;
			if (!dbcSkin.empty())
			{
				skin = path + dbcSkin + ".blp";
			}

			Ogre::String realFileName = path + base + ".m2";

			Ogre::MeshPtr mesh;
			if (!Ogre::MeshManager::getSingleton().resourceExists(realFileName))
			{
				try
				{
					mesh = Ogre::MeshManager::getSingleton().createManual(realFileName, "WoW", new editor::M2MeshLoader(realFileName));
					mesh->load();
				}
				catch (const Ogre::Exception &)
				{
					// Next!
					return;
				}
			}
			else
			{
				mesh = Ogre::MeshManager::getSingleton().getByName(realFileName, "WoW");
			}

			Ogre::Entity *ent = nullptr;
			try
			{
				ent = m_sceneMgr.createEntity("Spawn_" + Ogre::StringConverter::toString(m_nextUnitSpawn++), mesh);
				ent->setUserAny(Ogre::Any(&spawn));

				m_spawnEntities.push_back(ogre_utils::EntityPtr(ent));
				if (textureId != 0)
				{
					OgreDBCFilePtr textureDbc = OgreDBCFileManager::getSingleton().load("DBFilesClient\\CreatureDisplayInfoExtra.dbc", "WoW");
					UInt32 textureRow = textureDbc->getRowByIndex(textureId);
					if (textureRow == UInt32(-1))
					{
						WLOG("Could not find texture id " << textureId);
					}
					else
					{
						skin = "textures\\BakedNpcTextures\\" + textureDbc->getField(textureRow, 20);
					}
				}

				if (!skin.empty())
				{
					for (UInt32 i = 0; i < ent->getNumSubEntities(); ++i)
					{
						auto *subEnt = ent->getSubEntity(i);
						auto *pass = subEnt->getMaterial()->getTechnique(0)->getPass(0);
						pass->setDiffuse(1.0f, 1.0f, 1.0f, 1.0f);
						if (pass->getNumTextureUnitStates() == 0)
						{
							pass->createTextureUnitState();
						}

						auto *texState = pass->getTextureUnitState(0);
						texState->setTextureName(skin);
					}
				}

				Ogre::SceneNode *node = m_sceneMgr.getRootSceneNode()->createChildSceneNode();
				node->attachObject(ent);
				node->setPosition(spawn.positionx(), spawn.positiony(), spawn.positionz());
				node->setOrientation(Ogre::Quaternion(Ogre::Radian(spawn.rotation()), Ogre::Vector3::UNIT_Z));
				if (unit->scale() != 0.0f)
					node->setScale(Ogre::Vector3(unit->scale()));
				m_spawnNodes.push_back(ogre_utils::SceneNodePtr(node));
			}
			catch (const Ogre::Exception &)
			{
				return;
			}

			if (select && ent)
			{
				m_app.getSelection().addSelected(
					std::unique_ptr<SelectedCreatureSpawn>(new SelectedCreatureSpawn(
						m_map,
						[this]() {
						},
						*ent,
						spawn))
				);
			}
		}

		void WorldEditor::onKeyPressed(const QKeyEvent *event)
		{
			m_transformWidget->onKeyPressed(event);
		}

		void WorldEditor::onKeyReleased(const QKeyEvent *event)
		{
			m_transformWidget->onKeyReleased(event);
		}

		void WorldEditor::onMousePressed(const QMouseEvent *event)
		{
			m_transformWidget->onMousePressed(event);
		}

		void WorldEditor::onMouseReleased(const QMouseEvent *event)
		{
			m_transformWidget->onMouseReleased(event);
		}

		void WorldEditor::onMouseMoved(const QMouseEvent *event)
		{
			m_transformWidget->onMouseMoved(event);
		}

		void WorldEditor::onDoubleClick(const QMouseEvent * e)
		{
			QPoint pos = e->pos();
			Ogre::Ray mouseRay = m_camera.getCameraToViewportRay(
				(Ogre::Real)pos.x() / m_camera.getViewport()->getActualWidth(),
				(Ogre::Real)pos.y() / m_camera.getViewport()->getActualHeight());
			Ogre::RaySceneQuery* pSceneQuery = m_sceneMgr.createRayQuery(mouseRay);
			pSceneQuery->setSortByDistance(true);
			Ogre::RaySceneQueryResult vResult = pSceneQuery->execute();
			for (size_t ui = 0; ui < vResult.size(); ui++)
			{
				if (vResult[ui].movable)
				{
					if (vResult[ui].movable->getMovableType().compare("Entity") == 0)
					{
						const auto &any = ((Ogre::Entity*)vResult[ui].movable)->getUserAny();
						if (!any.isEmpty())
						{
							if (any.getType() == typeid(proto::UnitSpawnEntry*))
							{
								auto *spawn = Ogre::any_cast<proto::UnitSpawnEntry*>(any);
								auto dialog = make_unique<SpawnDialog>(m_app, *spawn);
								dialog->exec();
							}
							else if (any.getType() == typeid(proto::ObjectSpawnEntry*)) 
							{
								auto *objspawn = Ogre::any_cast<proto::ObjectSpawnEntry*>(any);
								auto dialog = make_unique<SpawnDialog>(m_app, *objspawn);
								dialog->exec();
							}

							break;
						}
					}
				}
			}
			m_sceneMgr.destroyQuery(pSceneQuery);
		}

		void WorldEditor::onSelection(Ogre::Entity & entity)
		{
			m_app.getSelection().clear();

			const auto &any = entity.getUserAny();
			if (!any.isEmpty())
			{
				if (any.getType() == typeid(proto::UnitSpawnEntry* const))
				{
					auto *spawn = Ogre::any_cast<proto::UnitSpawnEntry*>(any);
					m_app.getSelection().addSelected(
						std::unique_ptr<SelectedCreatureSpawn>(new SelectedCreatureSpawn(
							m_map,
							[this]() {
							},
							entity,
							*spawn)));

				}
				else if(any.getType() == typeid(proto::ObjectSpawnEntry* const))
				{
					auto *objspawn = Ogre::any_cast<proto::ObjectSpawnEntry*>(any);
					if (objspawn)
					{
						m_app.getSelection().addSelected(
							std::unique_ptr<SelectedObjectSpawn>(new SelectedObjectSpawn(
								m_map,
								[this]() {
								},
								entity,
								*objspawn)));
					}
				}
				else
				{
					ELOG("TYPE: " << any.getType().name());
				}
			}
		}

		void WorldEditor::onSetPoint(const Ogre::Vector3 & point)
		{
			if (!m_startSet)
			{
				m_start = point;
				m_startSet = true;
			}
			else
			{
				m_target = point;
				m_startSet = false;

				// Pathfinding
				math::Vector3 start(m_start.x, m_start.y, m_start.z);
				math::Vector3 end(m_target.x, m_target.y, m_target.z);
				std::vector<math::Vector3> points;
				if (!m_mapInst->calculatePath(start, end, points))
				{
					ELOG("Could not calculate path");
				}
				else
				{
					if (points.size() > 1)
					{
						// Create collision for this map
						if (!m_pathObj.get())
						{
							m_pathObj.reset(m_sceneMgr.createManualObject());
							m_sceneMgr.getRootSceneNode()->attachObject(m_pathObj.get());
						}
						m_pathObj->clear();

						m_pathObj->begin("Editor/PathLine", Ogre::RenderOperation::OT_LINE_STRIP);
						for (auto &p : points)
						{
							m_pathObj->position(p.x, p.y, p.z);
						}
						m_pathObj->end();

						m_pathObj->begin("Editor/PathPoint", Ogre::RenderOperation::OT_POINT_LIST);
						for (auto &p : points)
						{
							m_pathObj->position(p.x, p.y, p.z);
						}
						m_pathObj->end();
					}
				}
			}
		}

		void WorldEditor::onAddUnitSpawn(wowpp::UInt32 entry, const Ogre::Vector3 & point)
		{
			auto *added = m_map.add_unitspawns();
			if (added)
			{
				added->set_unitentry(entry);
				added->set_isactive(true);
				added->set_maxcount(1);
				added->set_positionx(point.x);
				added->set_positiony(point.y);
				added->set_positionz(point.z);
				added->set_respawndelay(120000);

				addUnitSpawn(*added, true);
			}
		}
	}
}
