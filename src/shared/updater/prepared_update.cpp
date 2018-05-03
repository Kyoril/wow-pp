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
#include "prepared_update.h"

namespace wowpp
{
	namespace updating
	{
		PreparedUpdateStep::PreparedUpdateStep()
		{
		}

		PreparedUpdateStep::PreparedUpdateStep(std::string destinationPath, StepFunction step)
			: destinationPath(std::move(destinationPath))
			, step(std::move(step))
		{
		}


		PreparedUpdate::Estimates::Estimates()
			: downloadSize(0)
			, updateSize(0)
		{
		}

		PreparedUpdate::PreparedUpdate()
		{
		}


		PreparedUpdate accumulate(std::vector<PreparedUpdate> updates)
		{
			PreparedUpdate sum;

			for (PreparedUpdate & part : updates)
			{
				sum.estimates.downloadSize += part.estimates.downloadSize;
				sum.estimates.updateSize += part.estimates.updateSize;

				sum.steps.insert(
				    sum.steps.end(),
				    std::make_move_iterator(part.steps.begin()),
				    std::make_move_iterator(part.steps.end())
				);
			}

			return sum;
		}
	}
}
