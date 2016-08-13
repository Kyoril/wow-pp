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

#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <ostream>

namespace wowpp
{
	namespace math
	{
		struct Vector3;

		struct Quaternion final
		{
			inline Quaternion()
				: w(1.0f)
				, x(0.0f)
				, y(0.0f)
				, z(0.0f)
			{
			}

			inline Quaternion(float w_, float x_, float y_, float z_)
				: w(w_)
				, x(x_)
				, y(y_)
				, z(z_)
			{
			}
			
			inline Quaternion(float angle, const Vector3& axis)
			{
				fromAngleAxis(angle, axis);
			}

			void fromAngleAxis(float angle, const Vector3& axis);
			float length() const;
			float normalize();

			inline Quaternion& operator= (const Quaternion& q)
			{
				w = q.w;
				x = q.x;
				y = q.y;
				z = q.z;
				return *this;
			}

			Quaternion operator* (float scalar) const;
			Vector3 operator* (const Vector3& vector) const;

			inline bool isNaN() const
			{
				return x != x || y != y || z != z || w != w;
			}

			float w, x, y, z;
		};
	}
}
