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

#include "circle.h"

namespace wowpp
{
	IShape::~IShape()
	{
	}

	Circle::Circle()
		: x(0.0f)
		, y(0.0f)
		, radius(0.0f)
	{
	}

	Circle::Circle(game::Distance x, game::Distance y, game::Distance radius)
		: x(x)
		, y(y)
		, radius(radius)
	{
	}

	Vector<game::Point, 2> Circle::getBoundingRect() const
	{
		return Vector<game::Point, 2>(
			game::Point(x - radius, y - radius),
			game::Point(x + radius, y + radius)
			);
	}

	bool Circle::isPointInside(const game::Point &point) const
	{
		const auto distSq = (game::Point(x, y) - point).lengthSq();
		const auto radiusSq = (radius * radius);
		return (distSq < radiusSq);
	}

}
