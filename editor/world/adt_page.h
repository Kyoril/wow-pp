
#pragma once

#include "terrain_model.h"
#include "OgreDataStream.h"

namespace wowpp
{
	namespace adt
	{
		struct Page
		{
			terrain::model::Page terrain;
		};

		void load(const Ogre::DataStreamPtr &file, adt::Page &out_page);
	}
}
