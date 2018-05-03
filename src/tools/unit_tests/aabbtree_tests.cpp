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

#include "pch.h"
#include <boost/test/unit_test.hpp>
#include "math/aabb_tree.h"
#include "math/ray.h"
#include "math/vector3.h"

namespace wowpp
{
	BOOST_AUTO_TEST_CASE(AABBTree_raytrace_test)
	{
		std::vector<math::Vector3> triangle({
			{-1, -1, 0},
			{+1, -1, 0},
			{ 0, +1, 0}
		});

		math::Ray ray({ 0, 0, 1 }, { 0, 0, -1 });
		math::AABBTree tree(triangle, { 0, 1, 2 });

		BOOST_CHECK(tree.intersectRay(ray, nullptr) == true);
	}

	BOOST_AUTO_TEST_CASE(AABBTree_complex_model_test)
	{
		std::ifstream inFile("model.obj", std::ios::in);
		BOOST_CHECK(inFile);

		std::vector<math::Vector3> vertices;
		std::vector<UInt32> indices;

		std::string line;
		while (std::getline(inFile, line))
		{
			char type;

			std::istringstream lineStream(line);
			lineStream >> type;

			switch (type)
			{
				case 'v':
				{
					math::Vector3 v;
					lineStream >> v.x >> v.y >> v.z;
					vertices.push_back(std::move(v));
				}
				break;

				case 'f':
				{
					UInt32 index = 0;
					for (int i = 0; i < 3; ++i) {
						lineStream >> index;
						indices.push_back(index - 1);
					}
				}
				break;
			}
		}

		math::Ray ray({ 0, 13.57f, 9.045f }, { 0, -6.319f, 80.861f });
		math::AABBTree tree(vertices, indices);
		BOOST_CHECK(tree.intersectRay(ray, nullptr) == true);
	}
}
