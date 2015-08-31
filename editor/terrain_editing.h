
#pragma once

#include "common/typedefs.h"
#include "common/box.h"
#include "common/vector.h"
#include "terrain_model.h"
#include <boost/variant.hpp>
#include <boost/signals2.hpp>

namespace wowpp
{
	namespace terrain
	{
		namespace editing
		{
			struct PositionedPage
			{
				terrain::model::Page *page;
				model::PagePosition position;

				PositionedPage()
					: page(nullptr)
				{
				}
			};

			struct PageRemoved
			{
				model::PagePosition removed;
			};

			struct PageAdded
			{
				PositionedPage added;
			};

			typedef boost::variant<PageRemoved, PageAdded> TerrainChangeEvent;
			typedef boost::signals2::signal<void(const TerrainChangeEvent &)> TerrainChangeSignal;
		}
	}
}
