
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
            typedef std::array<float, 3> Normal;
            typedef std::array<Normal, constants::VertsPerTile> Normalmap;
			typedef std::array<float, constants::VertsPerTile> Heightmap;
			typedef std::array<Heightmap, constants::TilesPerPageSquared> TileHeights;
            typedef std::array<Normalmap, constants::TilesPerPageSquared> TileNormals;
			typedef Vector<size_t, 2> PagePosition;
			typedef Vector<size_t, 2> VertexPosition;

			class Page
			{
			public:

				TileHeights heights;
                TileNormals normals;
				
				explicit Page();
				explicit Page(const Page &other);
			};

			typedef std::function<Page *(PagePosition)> SparseTerrain;
		}
	}
}
