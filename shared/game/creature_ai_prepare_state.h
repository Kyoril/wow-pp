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

#include "creature_ai_state.h"
#include "common/timer_queue.h"
#include "common/countdown.h"

namespace wowpp
{
	/// Handle the preparation state of a creature AI. Creatures enter this state
	/// immediatly after they spawned. In this state, creature start casting their
	/// respective spells on themself and they won't aggro nearby enemies if they
	/// come too close.
	class CreatureAIPrepareState : public CreatureAIState
	{
	public:

		/// Initializes a new instance of the CreatureAIIdleState class.
		/// @param ai The ai class instance this state belongs to.
		explicit CreatureAIPrepareState(CreatureAI &ai);
		/// Default destructor.
		virtual ~CreatureAIPrepareState();

		///
		virtual void onEnter() override;
		///
		virtual void onLeave() override;

	private:

		simple::scoped_connection m_onThreatened;
		Countdown m_preparation;
	};
}
