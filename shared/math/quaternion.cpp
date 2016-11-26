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

#include <cmath>
#include <cstddef>
#include <ostream>
#include <cassert>
#include "quaternion.h"
#include "vector3.h"

namespace wowpp
{
	namespace math
	{
		void Quaternion::fromAngleAxis(float angle, const Vector3 & axis)
		{
			float fHalfAngle(0.5f * angle);
			float fSin = sinf(fHalfAngle);
			w = cosf(fHalfAngle);
			x = fSin * axis.x;
			y = fSin * axis.y;
			z = fSin * axis.z;
		}

		float Quaternion::length() const
		{
			return w*w + x*x + y*y + z*z;
		}

		float Quaternion::normalize()
		{
			float len = length();
			float factor = 1.0f / sqrtf(len);
			*this = *this * factor;
			return len;
		}

		Quaternion Quaternion::operator*(float scalar) const
		{
			return Quaternion(scalar*w, scalar*x, scalar*y, scalar*z);
		}

		Vector3 Quaternion::operator* (const Vector3& vector) const
		{
			Vector3 uv, uuv;
			Vector3 qvec(x, y, z);
			uv = qvec.cross(vector);
			uuv = qvec.cross(uv);
			uv *= (2.0f * w);
			uuv *= 2.0f;

			return vector + uv + uuv;
		}
	}
}
