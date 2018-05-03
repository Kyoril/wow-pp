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

namespace wowpp
{
	template <class T>
	std::unique_ptr<T> make_unique()
	{
		return std::unique_ptr<T>(new T());
	}

	template <class T, class A0>
	std::unique_ptr<T> make_unique(A0  &&a0)
	{
		return std::unique_ptr<T>(new T(std::forward<A0>(a0)));
	}

	template <class T, class A0, class A1>
	std::unique_ptr<T> make_unique(A0  &&a0, A1  &&a1)
	{
		return std::unique_ptr<T>(new T(std::forward<A0>(a0),
		                                std::forward<A1>(a1)));
	}

	template <class T, class A0, class A1, class A2>
	std::unique_ptr<T> make_unique(A0  &&a0, A1  &&a1, A2 &&a2)
	{
		return std::unique_ptr<T>(new T(std::forward<A0>(a0),
		                                std::forward<A1>(a1),
		                                std::forward<A2>(a2)));
	}

	template <class T, class A0, class A1, class A2, class A3>
	std::unique_ptr<T> make_unique(A0  &&a0, A1  &&a1, A2 &&a2, A3 &&a3)
	{
		return std::unique_ptr<T>(new T(std::forward<A0>(a0),
		                                std::forward<A1>(a1),
		                                std::forward<A2>(a2),
		                                std::forward<A3>(a3)));
	}

	template <class T, class A0, class A1, class A2, class A3, class A4>
	std::unique_ptr<T> make_unique(A0  &&a0, A1  &&a1, A2 &&a2, A3 &&a3, A4 &&a4)
	{
		return std::unique_ptr<T>(new T(std::forward<A0>(a0),
		                                std::forward<A1>(a1),
		                                std::forward<A2>(a2),
		                                std::forward<A3>(a3),
		                                std::forward<A4>(a4)));
	}

	template <class T, class A0, class A1, class A2, class A3, class A4, class A5>
	std::unique_ptr<T> make_unique(A0  &&a0, A1  &&a1, A2 &&a2, A3 &&a3, A4 &&a4, A5 &&a5)
	{
		return std::unique_ptr<T>(new T(std::forward<A0>(a0),
		                                std::forward<A1>(a1),
		                                std::forward<A2>(a2),
		                                std::forward<A3>(a3),
		                                std::forward<A4>(a4),
		                                std::forward<A5>(a5)));
	}
}
