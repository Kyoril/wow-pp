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

#include <iomanip>
#include <iosfwd>
#include <cstddef>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <vector>
#include <limits>
#include "bounding_box.h"
#include "matrix4.h"

namespace wowpp
{
	namespace math
	{
		BoundingBox::BoundingBox(const Vector3 &min_, const Vector3 &max_)
			: min(min_)
			, max(max_)
		{
		}

		void BoundingBox::transform(const Matrix4 &mat)
		{
			float minVal = std::numeric_limits<float>::lowest();
			float maxVal = std::numeric_limits<float>::max();

			min = { maxVal, maxVal, maxVal };
			max = { minVal, minVal, minVal };

			Vector3 corners[8] = {
				{ min.x, min.y, min.z },
				{ max.x, min.y, min.z },
				{ max.x, max.y, min.z },
				{ min.x, max.y, min.z },
				{ min.x, min.y, max.z },
				{ max.x, min.y, max.z },
				{ max.x, max.y, max.z },
				{ min.x, max.y, max.z }
			};

			for (auto& v : corners)
			{
				v = mat * v;
				min = takeMinimum(min, v);
				max = takeMaximum(max, v);
			}
		}

		void BoundingBox::combine(const BoundingBox &other)
		{
			min = takeMinimum(min, other.min);
			max = takeMaximum(max, other.max);
		}

		float BoundingBox::getVolume() const
		{
			Vector3 e = max - min;
			return e.x * e.y * e.z;
		}

		float BoundingBox::getSurfaceArea() const
		{
			Vector3 e = max - min;
			return 2.0f * (e.x*e.y + e.x*e.z + e.y*e.z);
		}

		Vector3 BoundingBox::getCenter() const
		{
			return (max + min) * 0.5f;
		}

		Vector3 BoundingBox::getExtents() const
		{
			return (max - min) * 0.5f;
		}

		Vector3 BoundingBox::getSize() const
		{
			return max - min;
		}

		std::ostream & operator << (std::ostream &o, const BoundingBox &b)
		{
			return o 
				<< "(Min: " << b.min << " Max: " << b.max << ")";
		}

		io::Writer &operator << (io::Writer &w, BoundingBox const &vector)
		{
			return w
				<< vector.min
				<< vector.max;
		}
		io::Reader &operator >> (io::Reader &r, BoundingBox &vector)
		{
			return r
				>> vector.min
				>> vector.max;
		}
	}
}
