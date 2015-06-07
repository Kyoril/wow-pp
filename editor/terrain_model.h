
#pragma once

#include "common/typedefs.h"
#include "common/vector.h"
#include "game/constants.h"
#include <functional>
#include <array>

namespace wowpp
{
	namespace terrain
	{
		namespace model
		{
			typedef std::array<float, constants::VertsPerTile> Heightmap;
			typedef std::array<Heightmap, constants::TilesPerPageSquared> TileHeights;
			typedef Vector<size_t, 2> PagePosition;
			typedef Vector<size_t, 2> VertexPosition;

			class Page
			{
			public:

				TileHeights heights;
				
				explicit Page();
				explicit Page(const Page &other);
			};

			typedef std::function<Page *(PagePosition)> SparseTerrain;
		}
	}
}
