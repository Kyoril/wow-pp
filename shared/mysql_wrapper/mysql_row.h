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
#include "mysql_exception.h"

namespace wowpp
{
	namespace MySQL
	{
		struct Select;


		struct Row
		{
			explicit Row(Select &select);
			operator bool () const;
			std::size_t getLength() const;
			const char *getField(std::size_t index) const;

			template <class T, class F = T>
			bool getField(std::size_t index, T &value) const
			{
				try
				{
					const char *const field = getField(index);
					if (!field)
					{
						return false;
					}
					value = boost::lexical_cast<F>(field);
				}
				catch (const boost::bad_lexical_cast &)
				{
					return false;
				}

				return true;
			}

			template <class T>
			void tryGetField(std::size_t index, T &value) const
			{
				if (index >= getLength())
				{
					throw OutOfRangeException();
				}

				if (!getField(index, value))
				{
					throw InvalidTypeException(value);
				}
			}

			template <class T>
			T tryGetField(std::size_t index) const
			{
				T temp;
				tryGetField(index, temp);
				return temp;
			}

			static Row next(Select &select);

		private:

			MYSQL_ROW m_row;
			std::size_t m_length;
		};
	}
}
