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
#include "basic_template_load_context.h"
#include "basic_template_save_context.h"

namespace wowpp
{
	template <class Id>
	class BasicTemplate
	{
	public:

		typedef Id Identifier;

	public:

		Identifier id;
		String name;

	public:

		BasicTemplate()
		{
		}

		virtual ~BasicTemplate()
		{
		}

		bool loadBase(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
		{
			if (!wrapper.table.tryGetInteger("id", id))
			{
				context.onError("This object has no id");
				return false;
			}

			return true;
		}

		void saveBase(BasicTemplateSaveContext &context) const
		{
			context.table.addKey("id", id);
		}

		virtual bool operator ==(const BasicTemplate &other) const
		{
			return (id == other.id);
		}
	};
}
