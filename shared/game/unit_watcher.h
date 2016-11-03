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

#include "circle.h"
#include "common/simple.hpp"

namespace wowpp
{
	class GameUnit;

	namespace detail
	{
		struct StopOnTrue final
		{
			typedef bool result_type;

			template<class InputIterator>
			bool operator ()(InputIterator begin, InputIterator end) const
			{
				while (begin != end)
				{
					if (*begin)
					{
						return true;
					}

					++begin;
				}

				return false;
			}
		};
	}

	/// Abstract base class to watch for units in a specified area.
	class UnitWatcher
	{
	public:

		typedef std::function<bool(GameUnit &, bool)> VisibilityChange;

		/// Creates a new instance of the UnitWatcher class and assigns a circle shape.
		explicit UnitWatcher(const Circle &shape, VisibilityChange visibilityChanged);
		/// Default destructor.
		virtual ~UnitWatcher();
		/// Gets the current shape.
		const Circle &getShape() const {
			return m_shape;
		}
		/// Updates the shape.
		void setShape(const Circle &shape);
		/// Starts watching for units in the specified shape.
		virtual void start() = 0;

	protected:

		VisibilityChange m_visibilityChanged;

	private:

		Circle m_shape;

		virtual void onShapeUpdated() = 0;
	};
}
