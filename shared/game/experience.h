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

#include "common/typedefs.h"

namespace wowpp
{
	namespace xp
	{
		static UInt32 getGrayLevel(UInt32 level)
		{
			if (level <= 5) {
				return 0;
			}

			if (level <= 39) {
				return level - 5 - level / 10;
			}

			if (level <= 59) {
				return level - 1 - level / 5;
			}

			return level - 9;
		}

		static UInt32 getZeroDifference(UInt32 level)
		{
			if (level < 8) {
				return 5;
			}
			if (level < 10) {
				return 6;
			}
			if (level < 12) {
				return 7;
			}
			if (level < 16) {
				return 8;
			}
			if (level < 20) {
				return 9;
			}
			if (level < 30) {
				return 11;
			}
			if (level < 40) {
				return 12;
			}
			if (level < 45) {
				return 13;
			}
			if (level < 50) {
				return 14;
			}
			if (level < 55) {
				return 15;
			}
			if (level < 60) {
				return 16;
			}
			return 17;
		}

		static UInt32 getXpDiff(UInt32 level)
		{
			if (level < 29) {
				return 0;
			}
			if (level == 29) {
				return 1;
			}
			if (level == 30) {
				return 3;
			}
			if (level == 31) {
				return 6;
			}

			return 5 * (level - 30);
		}

		static float getGroupXpRate(UInt32 count, bool raid)
		{
			if (raid)
			{
				return 1.0f;
			}
			else
			{
				if (count <= 2) {
					return 1.0f;
				}
				if (count == 3) {
					return 1.166f;
				}
				if (count == 4) {
					return 1.3f;
				}

				return 1.4f;
			}
		}
	}
}
