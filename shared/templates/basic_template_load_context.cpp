//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "basic_template_load_context.h"

namespace wowpp
{
	BasicTemplateLoadContext::~BasicTemplateLoadContext()
	{
	}

	bool BasicTemplateLoadContext::executeLoadLater()
	{
		bool success = true;

		for (const LoadLaterFunction & function : loadLater)
		{
			if (!function())
			{
				success = false;
			}
		}

		return success;
	}


	ReadTableWrapper::ReadTableWrapper(const SffTemplateTable &_table)
		: table(_table)
	{
	}

	bool loadPosition(
		std::array<float, 3> &position,
		const sff::read::tree::Array<DataFileIterator> &array)
	{
		if (array.getSize() != position.size())
		{
			return false;
		}

		position[0] = array.getInteger(0, 0.0f);
		position[1] = array.getInteger(1, 0.0f);
		position[2] = array.getInteger(2, 0.0f);
		return true;
	}
}
