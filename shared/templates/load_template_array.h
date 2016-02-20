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

#include "template_manager.h"
#include "basic_template_load_context.h"
#include <memory>

namespace wowpp
{
	template <class T, class Ptr, class Context, class LoadTemplate>
	bool loadTemplateArray(
	    TemplateManager<T, Ptr> &manager,
	    const sff::read::tree::Array<DataFileIterator> &array,
	    Context &context,
	    LoadTemplate loadTemplate
	)
	{
		bool success = true;
		const auto count = array.getSize();

		for (size_t i = 0; i < count; ++i)
		{
			const sff::read::tree::Table<DataFileIterator> *const table =
			    array.getTable(i);

			if (!table)
			{
				context.onError("Non-Table array element found");
				continue;
			}

			Ptr templ(new T);

			ReadTableWrapper wrapper(*table);
			if (!loadTemplate(*templ, context, wrapper))
			{
				success = false;
				continue;
			}

			if (manager.getById(templ->id))
			{
				context.onError(
				    "Id " + boost::lexical_cast<String>(templ->id) +
				    " cannot be assigned again");
				continue;
			}

			manager.add(std::move(templ));
		}

		return success;
	}
}
