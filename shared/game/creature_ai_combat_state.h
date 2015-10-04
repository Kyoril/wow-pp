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

#include "common/typedefs.h"
#include "creature_ai_state.h"
#include "common/timer_queue.h"
#include "common/countdown.h"
#include <boost/signals2.hpp>
#include <memory>
#include <map>

namespace wowpp
{
	class GameUnit;

	/// Handle the idle state of a creature AI. In this state, most units
	/// watch for hostile units which come close enough, and start attacking these
	/// units.
	class CreatureAICombatState : public CreatureAIState
	{
		struct ThreatEntry
		{
			GameUnit *threatener;
			float amount;

			explicit ThreatEntry(GameUnit *threatener, float amount = 0.0f)
				: threatener(threatener)
				, amount(amount)
			{
			}
		};

		typedef std::map<UInt64, ThreatEntry> ThreatList;
		typedef std::map<UInt64, boost::signals2::scoped_connection> UnitSignals;

	public:

		/// Initializes a new instance of the CreatureAIIdleState class.
		/// @param ai The ai class instance this state belongs to.
		explicit CreatureAICombatState(CreatureAI &ai, GameUnit &victim);
		/// Default destructor.
		virtual ~CreatureAICombatState();

		/// 
		virtual void onEnter() override;
		/// 
		virtual void onLeave() override;
		/// 
		virtual void onDamage(GameUnit &attacker) override;

	private:

		void addThreat(GameUnit &threatener, float amount);
		void removeThreat(GameUnit &threatener);
		void updateVictim();
		void chaseTarget(GameUnit &target);

	private:

		ThreatList m_threat;
		UnitSignals m_killedSignals;
		UnitSignals m_despawnedSignals;
		boost::signals2::scoped_connection m_onThreatened, m_onVictimMoved, m_onControlledMoved;
		Countdown m_moveUpdate;
		float m_targetX, m_targetY, m_targetZ;
		GameTime m_moveStart, m_moveEnd;
	};
}
