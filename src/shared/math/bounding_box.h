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

#include "vector3.h"

namespace wowpp
{
	namespace math
	{
		struct Matrix4;

		/// This class represents a bounding box in the world.
		class BoundingBox final
		{
		public:

			/// Default constructor.
			BoundingBox() = default;
			/// Destructor
			~BoundingBox() = default;
			/// Custom constructor.
			/// @param min New minimum vector of the bounding box.
			/// @param max New maximum vector of the bounding box.
			BoundingBox(const Vector3 &min, const Vector3 &max);

			/// Transforms this bounding box by using a transformation matrix.
			/// @param mat The matrix used to transform this bounding box.
			void transform(const Matrix4 &mat);
			/// Combines this bounding box with another one.
			/// @param other The other bounding box.
			void combine(const BoundingBox &other);
			/// Calculates the volume of this bounding box.
			/// @returns The volume of this bounding box.
			float getVolume() const;
			/// Calculates the area of all surfaces of the bounding box.
			/// @returns Area of all surfaces of the bounding box.
			float getSurfaceArea() const;
			/// Calculates the center of this bounding box.
			/// @returns Center of the bounding box.
			Vector3 getCenter() const;
			/// Calculates the extents of this bounding box.
			/// @returns Extents of the bounding box.
			Vector3 getExtents() const;
			/// Calculates the size of the bounding box.
			/// @returns Size of the bounding box.
			Vector3 getSize() const;

		public:

			// Fields
			Vector3 min, max;
		};

		inline BoundingBox operator * (const Matrix4& mat, const BoundingBox& bb) 
		{
			BoundingBox bbox = bb;
			bbox.transform(mat);
			return bbox;
		}

		inline BoundingBox operator + (const BoundingBox& bb1, const BoundingBox& bb2)
		{
			BoundingBox bbox = bb1;
			bbox.combine(bb2);
			return bbox;
		}

		std::ostream & operator << (std::ostream &o, const BoundingBox &b);

		io::Writer &operator << (io::Writer &w, BoundingBox const &vector);
		io::Reader &operator >> (io::Reader &r, BoundingBox &vector);
	}
}
