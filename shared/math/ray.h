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

#include "common/typedefs.h"
#include "bounding_box.h"
#include "vector3.h"
#include <cmath>

namespace wowpp
{
	namespace math
	{
		/// Represents a ray, which is used to determine line of sight collision or blink.
		struct Ray final
		{
			/// Starting point of the ray.
			Vector3 origin;
			/// Destination point of the ray.
			Vector3 destination;
			/// Normalized direction of the ray.
			Vector3 direction;
			/// Perctual hit distance or 1.0f if nothing was hit.
			float hitDistance = 1.0f;

			/// Initializes the ray by providing a start point and an end point. This
			/// will automatically calculate the direction of the ray.
			/// @param start The starting point of the ray.
			/// @param end The ending point of the ray used to calculate the direction.
			explicit Ray(const Vector3 &start, const Vector3 &end)
				: origin(start)
				, destination(end)
			{
				assert(origin != destination);
				direction = (destination - origin);
				direction.normalize();
			}
			/// Initializes the ray by providing an origin, a direction and a maximum distance.
			/// @param start The starting point of the ray.
			/// @param dir The normalized direction of the ray.
			/// @param maxDistance The maximum distance of the ray. Used to calculate the ending point.
			///                    Has to be >= 0.
			explicit Ray(Vector3 start, Vector3 dir, float maxDistance)
				: origin(std::move(start))
				, direction(std::move(dir))
			{
				assert(maxDistance > 0.0f);
				assert(direction.length() >= 0.9999f && direction.length() <= 1.0001f);
				destination = origin + (direction * maxDistance);
			}
			/// Gets the vector representation of this ray.
			/// @returns Vector representation of this ray.
			Vector3 getVector() const {
				return destination - origin;
			}
			/// Gets the maximum length of the vector (so distance between origin and destination).
			/// @returns Maximum length of the ray in world units.
			float getLength() const {
				return getVector().length();
			}

			/// Checks whether this ray intersects with a triangle.
			/// @param a The first vertex of the triangle.
			/// @param b The second vertex of the triangle.
			/// @param c The third vertex of the triangle.
			/// @returns A pair of true and the hit distance, if the ray intersects. Otherwise,
			///          a pair of false and 0 is returned.
			std::pair<bool, float> intersectsTriangle(const Vector3 &a, const Vector3 &b, const Vector3 &c, bool ignoreBackface = false)
			{
				const float upscaleFactor = 100.0f;

				Vector3 rayDir = direction * upscaleFactor;
				Vector3 v0 = a * upscaleFactor;
				Vector3 v1 = b * upscaleFactor - v0;
				Vector3 v2 = c * upscaleFactor - v0;

				Vector3 p = rayDir.cross(v2);
				float det = v1.dot(p);

				if ((ignoreBackface && det < 1e-5) || std::abs(det) < 1e-5)
					return { false, 0.0f };

				Vector3 t = origin * upscaleFactor - v0;
				float e1 = t.dot(p) / det;

				if (e1 < 0.0f || e1 > 1.0f) {
					return{ false, 0.0f };
				}

				Vector3 q = t.cross(v1);
				float e2 = rayDir.dot(q) / det;

				if (e2 < 0.0f || (e1 + e2) > 1.0f) {
					return{ false, 0.0f };
				}

				float d = v2.dot(q) / det;
				if (d < 1e-5) {
					return{ false, 0.0f };
				}

				return { true, d / getLength() };
			}

			/// Checks whether this ray intersects with a bounding box.
			/// @param box The bounding box to check.
			/// @returns First paramater is true if ray cast intersects. Second parameter returns the
			///          hit distance, which can be used to calculate the hit point (if first parameter is true).
			std::pair<bool, float> intersectsAABB(const BoundingBox &box) const
			{
				const Vector3 invDir{
					1.0f / direction.x,
					1.0f / direction.y,
					1.0f / direction.z
				};
				
				float t1 = (box.min.x - origin.x) * invDir.x;
				float t2 = (box.max.x - origin.x) * invDir.x;
				float t3 = (box.min.y - origin.y) * invDir.y;
				float t4 = (box.max.y - origin.y) * invDir.y;
				float t5 = (box.min.z - origin.z) * invDir.z;
				float t6 = (box.max.z - origin.z) * invDir.z;

				float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
				float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

				// if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behind us
				if (tmax < 0) {
					return std::make_pair(false, 0.0f);
				}

				// if tmin > tmax, ray doesn't intersect AABB
				if (tmin > tmax) {
					return std::make_pair(false, 0.0f);
				}

				return std::make_pair(true, tmin / getLength());
			}
		};

		/// Enumerates all tiles that a ray crosses.
		template<typename Callback>
		void forEachTileInRayXY(const Ray &ray, float cellSize, Callback callback)
		{
			Int32 x1 = floor(ray.origin.x / cellSize);
			Int32 y1 = floor(ray.origin.y / cellSize);
			Int32 x2 = floor(ray.destination.x / cellSize);
			Int32 y2 = floor(ray.destination.y / cellSize);

			Int32 dx = ::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
			Int32 dy = -::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
			Int32 e = dx + dy, e2;

			while (true)
			{
				if (!callback(x1, y1))
					return;

				if (x1 == x2 && y1 == y2)
					break;
				e2 = 2 * e;
				if (e2 > dy) { e += dy; x1 += sx; }
				if (e2 < dx) { e += dx; y1 += sy; }
			}
		}
	}
}
