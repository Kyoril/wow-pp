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
#include "vector.h"
#include <memory>

namespace wowpp
{
	template <class T>
	class Grid
	{
	public:

		typedef T value_type;
		typedef value_type &reference;
		typedef const value_type &const_reference;
		typedef typename std::vector<value_type> Contents;
		typedef typename Contents::size_type size_type;
		typedef typename Contents::iterator iterator;
		typedef typename Contents::const_iterator const_iterator;


	public:

		Grid()
			: m_width(0)
		{
		}

		Grid(size_type width, size_type height)
			: m_contents(width * height)
			, m_width(width)
		{
		}

		Grid(size_type width, size_type height, const value_type &value)
			: m_contents(width * height, value)
			, m_width(width)
		{
		}

		template <class Unsigned>
		explicit Grid(Vector<Unsigned, 2> size)
		    : m_contents(size[0] * size[1])
		    , m_width(size[0])
		{
		}

		Grid(Grid &&other)
			: m_contents(std::move(other.m_contents))
			, m_width(other.m_width)
		{
		}

		Grid(const Grid &other)
			: m_contents(other.m_contents)
			, m_width(other.m_width)
		{
		}

		Grid &operator = (const Grid &other)
		{
			if (this != &other)
			{
				Grid(other).swap(*this);
			}
			return *this;
		}

		Grid &operator = (Grid &&other)
		{
			other.swap(*this);
			return *this;
		}

		void swap(Grid &other)
		{
			m_contents.swap(other.m_contents);
			std::swap(m_width, other.m_width);
		}

		bool empty() const
		{
			return (m_width == 0);
		}

		Vector<size_t, 2> getSize() const
		{
			return makeVector(width(), height());
		}

		size_type width() const
		{
			return m_width;
		}

		size_type height() const
		{
			return (m_width == 0 ? 0 : (m_contents.size() / m_width));
		}

		void clear()
		{
			m_width = 0;
			m_contents.clear();
		}

		iterator begin()
		{
			return m_contents.begin();
		}

		const_iterator begin() const
		{
			return m_contents.begin();
		}

		iterator end()
		{
			return m_contents.end();
		}

		const_iterator end() const
		{
			return m_contents.end();
		}

		value_type &operator ()(size_type x, size_type y)
		{
			return get(getIndex(x, y));
		}

		const value_type &operator ()(size_type x, size_type y) const
		{
			return get(getIndex(x, y));
		}

		template <class Unsigned>
		value_type &operator [](const Vector<Unsigned, 2> &position)
		{
			return get(getIndex(position[0], position[1]));
		}

		template <class Unsigned>
		const value_type &operator [](const Vector<Unsigned, 2> &position) const
		{
			return get(getIndex(position[0], position[1]));
		}

		template <class Unsigned>
		void resizePreservingPositions(Vector<Unsigned, 2> size)
		{
			const auto oldSize = getSize();
			Grid newGrid(size);
			const Vector<Unsigned, 2> preservedElements(
			        std::min(oldSize[0], size[0]),
			        std::min(oldSize[1], size[1]));
			for (size_t y = 0; y < preservedElements[1]; ++y)
			{
				for (size_t x = 0; x < preservedElements[0]; ++x)
				{
					newGrid(x, y) = (*this)(x, y);
				}
			}
			newGrid.swap(*this);
		}

	private:

		Contents m_contents;
		size_type m_width;


		value_type &get(size_type index)
		{
			assert(index < m_contents.size());
			return m_contents[index];
		}

		const value_type &get(size_type index) const
		{
			assert(index < m_contents.size());
			return m_contents[index];
		}

		size_type getIndex(size_type x, size_type y) const
		{
			assert(x < m_width);
			return (x + y * width());
		}
	};


	template <class T>
	void swap(Grid<T> &left, Grid<T> &right)
	{
		left.swap(right);
	}
}
