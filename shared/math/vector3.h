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

namespace wowpp
{
	namespace math
	{
		/// Represents a mathematical 3d vector and provides vector operations.
		struct Vector3 final
		{
			float x, y, z;

			/// Initializes the vector using the given 3d coordinates.
			/// @param x_ The x component.
			/// @param y_ The y component.
			/// @param z_ The z component.
			Vector3(float x_ = 0.0f, float y_ = 0.0f, float z_ = 0.0f)
				: x(x_)
				, y(y_)
				, z(z_)
			{
			}
			/// Copies the elements of another vector.
			/// @param other The vector to copy.
			Vector3(const Vector3 &other)
				: x(other.x)
				, y(other.y)
				, z(other.z)
			{
			}
			/// Calculates the dot-product of this vector and another vector.
			float dot(const Vector3 &other) const
			{
				return x * other.x + y * other.y + z * other.z;
			}
			/// Calculates the cross-product of this vector and another vector.
			/// Results in a new vector from this x other.
			/// @param other The other vector.
			Vector3 cross(const Vector3 &other) const
			{
				return Vector3(
					y * other.z - z * other.y,
					z * other.x - x * other.z,
					x * other.y - y * other.x
					);
			}
			/// Calculates the magnitude of this vector.
			const float length() const
			{
				return sqrtf(x * x + y * y + z * z);
			}
			/// Normalizes this vector. Has no effect if the length of this vector 
			/// is equal to zero.
			/// @returns The magnitude of this vector.
			float normalize()
			{
				const float len = length();
				if (len == 0.0f) return 0.0f;

				x /= len;
				y /= len;
				z /= len;

				return len;
			}

			inline float operator [] (const size_t i) const
			{
				assert(i < 3);
				return *(&x + i);
			}

			inline float& operator [] (const size_t i)
			{
				assert(i < 3);
				return *(&x + i);
			}
			inline Vector3 operator +(const Vector3 &v) const
			{
				return Vector3(x + v.x, y + v.y, z + v.z);
			}
			inline Vector3 operator -(const Vector3 &v) const
			{
				return Vector3(x - v.x, y - v.y, z - v.z);
			}
			inline Vector3 operator *(float value) const
			{
				return Vector3(x * value, y * value, z * value);
			}
			inline Vector3 operator /(float value) const
			{
				return Vector3(x / value, y / value, z / value);
			}
			inline bool operator ==(const Vector3 &other) const
			{
				return (x == other.x && y == other.y && z == other.z);
			}
			inline bool operator !=(const Vector3 &other) const
			{
				return (x != other.x || y != other.y || z != other.z);
			}
			inline bool operator < (const Vector3& other) const
			{
				return (x < other.x && y < other.y && z < other.z);
			}
			inline bool operator > (const Vector3& other) const
			{
				return (x > other.x && y > other.y && z > other.z);
			}
			inline Vector3& operator = (const Vector3& other)
			{
				x = other.x;
				y = other.y;
				z = other.z;
				return *this;
			}
			inline Vector3& operator = (const float scalar)
			{
				x = scalar;
				y = scalar;
				z = scalar;
				return *this;
			}
		};
	}
}