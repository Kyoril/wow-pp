
#pragma once

#include "common/work_queue.h"
#include "page.h"
#include "loaded_page_section.h"
#include "page_neighborhood.h"
#include "page_loader_listener.h"
#include "page_pov_partitioner.h"
#include "world_page_loader.h"
#include "world_renderer.h"
#include "scene_node_ptr.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <QMouseEvent>

namespace Ogre
{
	class Camera;
    class Light;
}

namespace wowpp
{
	struct MapEntry;

	namespace editor
	{
		class Graphics;

		class WorldEditor final : public paging::IPageLoaderListener
		{
		public:

			explicit WorldEditor(Graphics &graphics, Ogre::Camera &camera, MapEntry &map);
			~WorldEditor();

			void update(float delta);
			void save();
			void onKeyPressed(const QKeyEvent *event);
			void onKeyReleased(const QKeyEvent *event);
			void onMousePressed(const QMouseEvent *event);
			void onMouseReleased(const QMouseEvent *event);
			void onMouseMoved(const QMouseEvent *event);

		private:

			void onPageAvailabilityChanged(const paging::PageNeighborhood &page, bool isAvailable) override;
			terrain::model::Page *getTerrainPage(terrain::model::PagePosition position);

		private:

			Graphics &m_graphics;
			Ogre::Camera &m_camera;
			MapEntry &m_map;
			boost::asio::io_service m_dispatcher;
			boost::asio::io_service m_workQueue;
			std::unique_ptr<boost::asio::io_service::work> m_work;
			boost::thread m_worker;
			std::unique_ptr<paging::LoadedPageSection> m_visibleSection;
			std::unique_ptr<WorldPageLoader> m_pageLoader;
			std::unique_ptr<paging::PagePOVPartitioner> m_memoryPointOfView;
			std::unique_ptr<view::WorldRenderer> m_worldRenderer;
			std::map<paging::PagePosition, terrain::model::Page> m_pages;
            Ogre::Light *m_light;
		};
	}
}
