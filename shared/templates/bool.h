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

#pragma once

#include "simple_file_format/sff_read_tree.h"
#include "simple_file_format/sff_write.h"
#include <string>

namespace wowpp
{
	template <class I>
	void getBool(const sff::read::tree::Table<I> &table,
	             const std::string &name,
	             bool &value)
	{
		value = table.getInteger(name, value ? 1 : 0) != 0;
	}

	template <class C>
	void setBool(sff::write::Table<C> &table,
	             const std::string &name,
	             bool value)
	{
		table.addKey(name, value ? 1 : 0);
	}
}
