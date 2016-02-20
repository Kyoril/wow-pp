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

#include <string>
#include <locale>
#include <algorithm>

namespace wowpp
{
	template <class T>
	bool case_insensitive_equal_chars(T left_lower, T right)
	{
		std::locale loc;
		return (left_lower == std::tolower(right, loc));
	}

	template <class T>
	bool case_insensitive_equal(const T *left_lower, const T *right)
	{
		while (true)
		{
			if (*left_lower)
			{
				if (!case_insensitive_equal_chars(*left_lower, *right))
				{
					return false;
				}
			}
			else
			{
				if (*right)
				{
					return false;
				}
				else
				{
					return true;
				}
			}
		}
	}

	template <class T>
	bool case_insensitive_equal(const std::basic_string<T> &left_lower, const std::basic_string<T> &right)
	{
		if (left_lower.size() != right.size())
		{
			return false;
		}

		return std::equal(
		           left_lower.begin(),
		           left_lower.end(),
		           right.begin(),
		           case_insensitive_equal_chars<T>);
	}
}
