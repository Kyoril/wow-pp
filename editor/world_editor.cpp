
#include "world_editor.h"
#include "graphics.h"
#include "data/map_entry.h"
#include "data/project.h"
#include "common/typedefs.h"
#include "common/constants.h"
#include "common/make_unique.h"
#include "game/constants.h"
#include "log/default_log_levels.h"
#include "program.h"
#include "graphics.h"
#include "OgreCamera.h"
#include "OgreLight.h"
#include "OgreSceneManager.h"
#include "OgreSceneNode.h"

namespace wowpp
{
	namespace editor
	{
		WorldEditor::WorldEditor(Graphics &graphics, Ogre::Camera &camera, MapEntry &map)
			: m_graphics(graphics)
			, m_camera(camera)
			, m_map(map)
			, m_work(new boost::asio::io_service::work(m_workQueue))
            , m_light(nullptr)
		{
			// Create worker thread
			boost::asio::io_service &workQueue = m_workQueue;
			boost::asio::io_service &dispatcher = m_dispatcher;
			m_worker = boost::thread([&workQueue]()
			{
				workQueue.run();
			});

			const Ogre::Vector3 &camPos = m_camera.getDerivedPosition();
			float convertedX = (constants::MapWidth * 32.0f) + camPos.x;
			float convertedY = (constants::MapWidth * 32.0f) + camPos.z;

			paging::PagePosition pos(static_cast<size_t>(convertedX / constants::MapWidth), static_cast<size_t>(convertedY / constants::MapWidth));

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
				m_graphics.getSceneManager(),
				m_camera));
            
            m_light = graphics.getSceneManager().createLight("Sun");
            m_light->setType(Ogre::Light::LT_DIRECTIONAL);
            m_light->setDirection(Ogre::Vector3(1.0f, -0.8f, 0.0f).normalisedCopy());
            m_graphics.getSceneManager().getRootSceneNode()->attachObject(m_light);
		}

		WorldEditor::~WorldEditor()
		{
            if (m_light)
            {
                m_graphics.getSceneManager().destroyLight(m_light);
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
			float convertedY = (constants::MapWidth * 32.0f) + camPos.z;

			paging::PagePosition pos(static_cast<size_t>(convertedX / constants::MapWidth), static_cast<size_t>(convertedY / constants::MapWidth));

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

		void WorldEditor::onPageAvailabilityChanged(const paging::PageNeighborhood &pages, bool isAvailable)
		{
			auto &mainPage = pages.getMainPage();
			const auto &pos = mainPage.getPosition();

			if (isAvailable)
			{
				// Add the page if needed
				auto it = m_pages.find(pos);
				if (it == m_pages.end())
				{
					const Ogre::String fileName =
						"World\\Maps\\" + m_map.directory + "\\" + m_map.directory + "_" +
						Ogre::StringConverter::toString(pos[1], 2, '0') + "_" +
						Ogre::StringConverter::toString(pos[0], 2, '0') + ".adt";

					if (!Ogre::ResourceGroupManager::getSingleton().resourceExistsInAnyGroup(fileName))
					{
						WLOG("Page [" << pos.x() << "x" << pos.y() << "] does not have a file and will be skipped (" << fileName << ")");
						return;
					}

					Ogre::DataStreamPtr file = Ogre::ResourceGroupManager::getSingleton().openResource(fileName);
					std::unique_ptr<adt::Page> page = make_unique<adt::Page>();
					adt::load(file, *page);
					
					// Store page
					m_pages[pos] = page->terrain;

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
