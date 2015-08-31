
#include "terrain_model.h"

namespace wowpp
{
	namespace terrain
	{
		namespace model
		{
			Page::Page()
			{
				for (auto &tile : heights)
				{
					tile.fill(0.0f);
				}
			}

			Page::Page(const Page &other)
			{
				heights = other.heights;
			}
		}
	}
}
