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

#include <memory>

namespace wowpp
{
	namespace detail
	{
		template <class T, class F>
		struct WeakPtrFunction0
		{
			std::weak_ptr<T> weak;
			F function;


			WeakPtrFunction0(std::shared_ptr<T> ptr, F function)
				: weak(ptr)
				, function(function)
			{
			}

			void operator ()()
			{
				std::shared_ptr<T> strong = weak.lock();
				if (strong)
				{
					function(strong.get());
				}
			}
		};


		template <class T, class F>
		struct WeakPtrFunction1
		{
			std::weak_ptr<T> weak;
			F function;


			WeakPtrFunction1(std::shared_ptr<T> ptr, F function)
				: weak(ptr)
				, function(function)
			{
			}

			template <class A1>
			void operator ()(A1 a1)
			{
				std::shared_ptr<T> strong = weak.lock();
				if (strong)
				{
					function(strong.get(), a1);
				}
			}
		};


		template <class T, class F>
		struct WeakPtrFunction2
		{
			std::weak_ptr<T> weak;
			F function;


			WeakPtrFunction2(std::shared_ptr<T> ptr, F function)
				: weak(ptr)
				, function(function)
			{
			}

			template <class A1, class A2>
			void operator ()(A1 a1, A2 a2)
			{
				std::shared_ptr<T> strong = weak.lock();
				if (strong)
				{
					function(strong.get(), a1, a2);
				}
			}
		};
	}


	template <class T, class F>
	detail::WeakPtrFunction0<T, F> bind_weak_ptr_0(std::shared_ptr<T> ptr, F function)
	{
		return detail::WeakPtrFunction0<T, F>(ptr, function);
	}

	template <class T, class F>
	detail::WeakPtrFunction1<T, F> bind_weak_ptr_1(std::shared_ptr<T> ptr, F function)
	{
		return detail::WeakPtrFunction1<T, F>(ptr, function);
	}

	template <class T, class F>
	detail::WeakPtrFunction2<T, F> bind_weak_ptr_2(std::shared_ptr<T> ptr, F function)
	{
		return detail::WeakPtrFunction2<T, F>(ptr, function);
	}
}
