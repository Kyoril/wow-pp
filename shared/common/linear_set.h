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

#include <cassert>

namespace wowpp
{
	template <class T>
	class LinearSet
	{
	public:

		typedef std::vector<T> Elements;
		typedef typename Elements::const_iterator const_iterator;

	public:

		LinearSet()
		{
		}

		const_iterator find(const T &element) const
		{
			return std::find(
			           m_elements.begin(),
			           m_elements.end(),
			           element);
		}

		//O(n)
		bool contains(const T &element) const
		{
			return find(element) != m_elements.end();
		}

		//Release: O(1), Debug: O(n)
		template <class U>
		typename std::enable_if<std::is_convertible<U, T>::value, void>::type
		add(U &&element)
		{
			assert(!contains(element));
			optionalAdd(std::forward<U>(element));
			assert(contains(element));
		}

		//Release: O(1), Debug: O(n)
		template <class U>
		typename std::enable_if<std::is_convertible<U, T>::value, bool>::type
		optionalAdd(U &&element)
		{
			if (contains(element))
			{
				return false;
			}

			m_elements.push_back(std::forward<U>(element));
			return true;
		}

		//O(n)
		void remove(const T &element)
		{
			assert(contains(element));
			optionalRemove(element);
			assert(!contains(element));
		}

		//O(n)
		bool optionalRemove(const T &element)
		{
			for (auto i = m_elements.begin(), e = m_elements.end();
			        i != e; ++i)
			{
				if (*i == element)
				{
					*i = std::move(m_elements.back());
					m_elements.pop_back();

					assert(!contains(element));
					return true;
				}
			}

			assert(!contains(element));
			return false;
		}

		//O(n) * O(condition)
		template <class RemovePredicate>
		bool optionalRemoveIf(const RemovePredicate &condition)
		{
			bool removedAny = false;

			for (auto i = m_elements.begin(), e = m_elements.end();
			        i != e; ++i)
			{
				if (condition(*i))
				{
					*i = std::move(m_elements.back());
					--e;
					m_elements.pop_back();
					removedAny = true;
				}
			}

			return removedAny;
		}

		template <class U>
		const_iterator insert(U &&element)
		{
			const auto i = find(element);
			if (i != m_elements.end())
			{
				return i;
			}
			m_elements.push_back(std::forward<U>(element));
			return m_elements.end() - 1;
		}

		const Elements &getElements() const
		{
			return m_elements;
		}

		size_t size() const
		{
			return m_elements.size();
		}

		bool empty() const
		{
			return m_elements.empty();
		}

		void erase(size_t from, size_t count)
		{
			const auto i = m_elements.begin() + from;

			m_elements.erase(
			    i,
			    i + count
			);
		}

		void clear()
		{
			m_elements.clear();
		}

		void swap(LinearSet &other)
		{
			m_elements.swap(other.m_elements);
		}

		const_iterator begin() const
		{
			return m_elements.begin();
		}

		const_iterator end() const
		{
			return m_elements.end();
		}

	private:

		Elements m_elements;
	};


	template <class T>
	void swap(LinearSet<T> &left, LinearSet<T> &right)
	{
		left.swap(right);
	}
}

