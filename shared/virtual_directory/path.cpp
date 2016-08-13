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
#include "path.h"

namespace wowpp
{
	namespace virtual_dir
	{
		void appendPath(Path &left, const Path &right)
		{
			if (!left.empty() &&
			        !right.empty())
			{
				size_t slashes = 0;

				if (left.back() == PathSeparator)
				{
					++slashes;
				}

				if (right.front() == PathSeparator)
				{
					++slashes;
				}

				switch (slashes)
				{
				case 0:
					left += PathSeparator;
					break;

				case 2:
					//GCC 4.6 does not know pop_back from C++11
					//left.pop_back();

					left.erase(left.end() - 1);
					break;

				default:
					break;
				}
			}

			left += right;
		}

		Path joinPaths(Path left, const Path &right)
		{
			appendPath(left, right);
			return left;
		}

		std::pair<Path, Path> splitLeaf(Path path)
		{
			std::pair<Path, Path> result;

			result.first = std::move(path);

			const auto separator = std::find(
			                           result.first.rbegin(),
			                           result.first.rend(),
			                           PathSeparator).base();

			result.second.assign(separator, result.first.end());
			result.first.erase(separator, result.first.end());
			return result;
		}

		std::pair<Path, Path> splitRoot(Path path)
		{
			std::pair<Path, Path> result;

			result.first = std::move(path);

			const auto separator = std::find(
			                           result.first.begin(),
			                           result.first.end(),
			                           PathSeparator);

			auto secondBegin = separator;
			if (secondBegin != result.first.end() &&
			        *secondBegin == PathSeparator)
			{
				++secondBegin;
			}
			result.second.assign(secondBegin, result.first.end());
			result.first.erase(separator, result.first.end());
			return result;
		}
	}
}
