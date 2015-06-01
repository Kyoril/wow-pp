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

#include "mysql_row.h"
#include "mysql_select.h"
#include <cassert>

namespace wowpp
{
	namespace MySQL
	{
		Row::Row(Select &select)
			: m_row(nullptr)
			, m_length(0)
		{
			if (select.nextRow(m_row))
			{
				assert(m_row);
				m_length = select.getFieldCount();
			}
		}

		Row::operator bool () const
		{
			return (m_row != nullptr);
		}

		std::size_t Row::getLength() const
		{
			return m_length;
		}

		const char *Row::getField(std::size_t index) const
		{
			assert(index < m_length);
			return m_row[index];
		}

		Row Row::next(Select &select)
		{
			return Row(select);
		}
	}
}
