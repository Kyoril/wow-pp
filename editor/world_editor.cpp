
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
#include "ogre_m2_loader.h"

namespace wowpp
{
	namespace editor
	{
		WorldEditor::WorldEditor(Ogre::SceneManager &sceneMgr, Ogre::Camera &camera, proto::MapEntry &map, proto::Project &project)
			: m_sceneMgr(sceneMgr)
			, m_camera(camera)
			, m_map(map)
			, m_work(new boost::asio::io_service::work(m_workQueue))
            , m_light(nullptr)
			, m_project(project)
		{
			// Create worker thread
			boost::asio::io_service &workQueue = m_workQueue;
			boost::asio::io_service &dispatcher = m_dispatcher;
			m_worker = boost::thread([&workQueue]()
			{
				workQueue.run();
			});

			// Move camera to first spawn position (if any)
			if (!m_map.unitspawns().empty())
			{
				const auto &spawn = m_map.unitspawns().begin();
				m_camera.setPosition(spawn->positionx(), spawn->positiony(), spawn->positionz() + 5.0f);
			}

			Ogre::String realFileName = "CREATURE\\Furbolg\\Furbolg.m2";

			// Spawn all entities
			UInt32 i = 0;
			for (const auto &spawn : m_map.unitspawns())
			{
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

				Ogre::Entity *ent = m_sceneMgr.createEntity("Spawn_" + Ogre::StringConverter::toString(i++), mesh);
				Ogre::SceneNode *node = m_sceneMgr.getRootSceneNode()->createChildSceneNode();
				node->attachObject(ent);
				node->setPosition(spawn.positionx(), spawn.positiony(), spawn.positionz());
				node->setOrientation(Ogre::Quaternion(Ogre::Radian(spawn.rotation()), Ogre::Vector3::UNIT_Z));
			}

			Ogre::String objectFileName = "WORLD\\GENERIC\\HUMAN\\ACTIVEDOODADS\\DOORS\\DEADMINEDOOR01.M2";

			i = 0;
			for (const auto &spawn : m_map.objectspawns())
			{
				Ogre::MeshPtr mesh;
				if (!Ogre::MeshManager::getSingleton().resourceExists(objectFileName))
				{
					mesh = Ogre::MeshManager::getSingleton().createManual(objectFileName, "WoW", new editor::M2MeshLoader(objectFileName));
					mesh->load();
				}
				else
				{
					mesh = Ogre::MeshManager::getSingleton().getByName(objectFileName, "WoW");
				}

				Ogre::Entity *ent = m_sceneMgr.createEntity("ObjSpawn_" + Ogre::StringConverter::toString(i++), mesh);
				Ogre::SceneNode *node = m_sceneMgr.getRootSceneNode()->createChildSceneNode();
				node->attachObject(ent);
				node->setPosition(spawn.positionx(), spawn.positiony(), spawn.positionz());
				node->setOrientation(Ogre::Quaternion(spawn.rotationz(), spawn.rotationw(), spawn.rotationx(), spawn.rotationy()));
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
		}

		WorldEditor::~WorldEditor()
		{
            if (m_light)
            {
				m_sceneMgr.destroyLight(m_light);
                m_light = nullptr;
            }
            
			// Stop paging
			m_work.reset();
			m_workQueue.stop();
			m_worker.join();
		}

		void WorldEditor::update(float delta)
		{
			const Ogre::Vector3 &camPos = m_camera.getDerivedPosition();
			float convertedX = (constants::MapWidth * 32.0f) + camPos.x;
			float convertedY = (constants::MapWidth * 32.0f) + camPos.y;

			paging::PagePosition pos(63 - static_cast<size_t>(convertedX / constants::MapWidth), 63 - static_cast<size_t>(convertedY / constants::MapWidth));

			m_memoryPointOfView->updateCenter(pos);
			m_visibleSection->updateCenter(pos);

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
					Ogre::StringConverter::toString(pos[1], 2, '0') + "_" +
					Ogre::StringConverter::toString(pos[0], 2, '0') + ".adt";

				if (!Ogre::ResourceGroupManager::getSingleton().resourceExistsInAnyGroup(fileName))
				{
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

					// Remove page from list
					m_pages.erase(it);
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

		void WorldEditor::onKeyPressed(const QKeyEvent *event)
		{
		}

		void WorldEditor::onKeyReleased(const QKeyEvent *event)
		{
		}

		void WorldEditor::onMousePressed(const QMouseEvent *event)
		{
		}

		void WorldEditor::onMouseReleased(const QMouseEvent *event)
		{
		}

		void WorldEditor::onMouseMoved(const QMouseEvent *event)
		{
		}
	}
}
