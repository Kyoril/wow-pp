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
#include "common/countdown.h"

namespace wowpp
{
	class UnitWatcher;
	class GameObject;

	namespace math
	{
		struct Vector3;
	}

	/// Handle the idle state of a creature AI. In this state, most units
	/// watch for hostile units which come close enough, and start attacking these
	/// units.
	class CreatureAIIdleState : public CreatureAIState
	{
	public:

		/// Initializes a new instance of the CreatureAIIdleState class.
		/// @param ai The ai class instance this state belongs to.
		explicit CreatureAIIdleState(CreatureAI &ai);
		/// Default destructor.
		virtual ~CreatureAIIdleState();

		/// @copydoc CreatureAIState::onEnter
		virtual void onEnter() override;
		/// @copydoc CreatureAIState::onLeave
		virtual void onLeave() override;
		/// @copydoc CreatureAIState::onCreatureMovementChanged
		virtual void onCreatureMovementChanged() override;
		/// @copydoc CreatureAIState::onControlledMoved
		virtual void onControlledMoved() override;

	private:

		void onChooseNextMove();

	private:

		std::unique_ptr<UnitWatcher> m_aggroWatcher;
		simple::scoped_connection m_onThreatened, onTargetReached;
		Countdown m_nextMove;
		GameTime m_nextUpdate;
	};
}
