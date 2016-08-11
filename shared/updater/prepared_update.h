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

#pragma once

namespace wowpp
{
	namespace updating
	{
		struct UpdateParameters;


		struct PreparedUpdateStep
		{
			typedef std::function<bool (const UpdateParameters &)> StepFunction;


			std::string destinationPath;
			StepFunction step;


			PreparedUpdateStep();
			PreparedUpdateStep(std::string destinationPath, StepFunction step);
			//TODO: move operations
		};


		struct PreparedUpdate
		{
			struct Estimates
			{
				boost::uintmax_t downloadSize;
				boost::uintmax_t updateSize;		// Uncompressed update size for better progress!

				Estimates();
			};


			std::vector<PreparedUpdateStep> steps;
			Estimates estimates;


			PreparedUpdate();
			//TODO: move operations
		};


		PreparedUpdate accumulate(std::vector<PreparedUpdate> updates);
	}
}
