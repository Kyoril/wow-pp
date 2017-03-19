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

#pragma once

#include "common/typedefs.h"
#include "common/box.h"
#include "common/vector.h"
#include "terrain_model.h"

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
			typedef simple::signal<void(const TerrainChangeEvent &)> TerrainChangeSignal;
		}
	}
}
