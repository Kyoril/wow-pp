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

#include "utilities.h"

namespace wowpp
{
	template <class T>
	struct Box
	{
		typedef T Type;

		T minimum;
		T maximum;


		Box()
		{
			clear();
		}

		Box(T min_, T max_)
		{
			minimum = min_;
			maximum = max_;
		}

		void clear()
		{
			minimum = T();
			maximum = T();
		}

		Box operator + () const
		{
			return *this;
		}

		Box operator - () const
		{
			Box temp;

			temp.minimum = -minimum;
			temp.maximum = -maximum;

			return temp;
		}
	};

	template <class T>
	Box<T> &operator += (Box<T> &left, const Box<T> &right)
	{
		left.minimum += right.minimum;
		left.maximum += right.maximum;

		return left;
	}

	template <class T>
	Box<T> &operator -= (Box<T> &left, const Box<T> &right)
	{
		left.minimum -= right.minimum;
		left.maximum -= right.maximum;

		return left;
	}

	template <class T>
	Box<T> operator + (const Box<T> &left, const Box<T> &right)
	{
		Box<T> temp(left);
		temp += right;
		return temp;
	}

	template <class T>
	Box<T> operator - (const Box<T> &left, const Box<T> &right)
	{
		Box<T> temp(left);
		temp -= right;
		return temp;
	}


	template <class T>
	bool operator == (const Box<T> &left, const Box<T> &right)
	{
		return (left.minimum == right.minimum) &&
		       (left.maximum == right.maximum);
	}

	template <class T>
	bool operator != (const Box<T> &left, const Box<T> &right)
	{
		return !(left == right);
	}

	template <class T>
	Box<T> makeBox(const T &minimum, const T &maximum)
	{
		return Box<T>(minimum, maximum);
	}
}
