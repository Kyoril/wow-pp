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

#include "mysql_select.h"
#include "mysql_connection.h"

namespace wowpp
{
	namespace MySQL
	{
		Select::Select(Connection &connection, const String &query, bool throwOnError)
			: m_result(nullptr)
		{
			if (throwOnError)
			{
				connection.tryExecute(query);
			}
			else
			{
				if (!connection.execute(query))
				{
					return;
				}
			}

			m_result = connection.storeResult();
		}

		Select::~Select()
		{
			if (m_result)
			{
				::mysql_free_result(m_result);
			}
		}

		void Select::freeResult()
		{
			assert(m_result);
			::mysql_free_result(m_result);
			m_result = nullptr;
		}

		bool Select::success() const
		{
			return (m_result != nullptr);
		}

		bool Select::nextRow(MYSQL_ROW &row)
		{
			assert(m_result);
			row = ::mysql_fetch_row(m_result);
			return (row != nullptr);
		}

		std::size_t Select::getFieldCount() const
		{
			assert(m_result);
			return ::mysql_num_fields(m_result);
		}
	}
}
