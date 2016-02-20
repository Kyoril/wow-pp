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

#include "creature_ai_state.h"
#include "creature_ai.h"
#include "game_creature.h"

namespace wowpp
{
	CreatureAIState::CreatureAIState(CreatureAI &ai)
		: m_ai(ai)
	{
	}

	CreatureAIState::~CreatureAIState()
	{
	}

	CreatureAI &CreatureAIState::getAI() const
	{
		return m_ai;
	}

	GameCreature &CreatureAIState::getControlled() const
	{
		return m_ai.getControlled();
	}

	void CreatureAIState::onEnter()
	{
	}

	void CreatureAIState::onLeave()
	{
	}

	void CreatureAIState::onDamage(GameUnit &attacker)
	{
	}

	void CreatureAIState::onHeal(GameUnit &healer)
	{
	}

	void CreatureAIState::onControlledDeath()
	{
	}
}
