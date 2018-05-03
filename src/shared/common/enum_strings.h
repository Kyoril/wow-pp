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

#include "typedefs.h"
#include "case_insensitive_equal.h"

namespace wowpp
{
	template <class T, unsigned Count, T InvalidIdentifier>
	struct EnumStrings
	{
		typedef T Identifier;
		typedef std::array<String, Count> StringArray;


		EnumStrings(const StringArray &array)
			: m_array(array)
		{
		}

		const String &getName(Identifier id) const
		{
			return m_array[id];
		}

		Identifier getIdentifier(const String &name) const
		{
			for (std::size_t i = 0; i < m_array.size(); ++i)
			{
				if (case_insensitive_equal(m_array[i], name))
				{
					return static_cast<Identifier>(i);
				}
			}

			return InvalidIdentifier;
		}

	private:

		const StringArray &m_array;
	};
}
