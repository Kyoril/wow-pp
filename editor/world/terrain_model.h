
#pragma once

#include "common/typedefs.h"
#include "common/vector.h"
#include "game/constants.h"
#include <functional>
#include <vector>
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
			typedef std::vector<String> Textures;
			typedef Vector<size_t, 2> PagePosition;
			typedef Vector<size_t, 2> VertexPosition;
			typedef std::vector<UInt32> TextureIds;
			typedef std::array<TextureIds, constants::TilesPerPageSquared> TileTextureIds;
			typedef std::array<UInt8, 64 * 64> AlphaMap;
			typedef std::vector<AlphaMap> AlphaMaps;
			typedef std::array<AlphaMaps, constants::TilesPerPageSquared> TileAlphaMaps;

			struct M2Placement
			{
				UInt32 mmidEntry;
				UInt32 uniqueId;
				float position[3];
				float rotation[3];
				UInt16 scale;
				UInt16 flags;
			};

			class Page
			{
			public:

				TileHeights heights;
                TileNormals normals;
				Textures textures;
				TileTextureIds textureIds;
				TileAlphaMaps alphaMaps;
				std::vector<String> m2Ids;
				std::vector<M2Placement> m2Placements;
				
				explicit Page();
				explicit Page(const Page &other);
			};

			typedef std::function<Page *(PagePosition)> SparseTerrain;
		}
	}
}
