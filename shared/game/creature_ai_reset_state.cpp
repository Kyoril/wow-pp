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
#include "creature_ai_reset_state.h"
#include "creature_ai.h"
#include "defines.h"
#include "game_creature.h"
#include "world_instance.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include "each_tile_in_sight.h"
#include "common/constants.h"
#include "proto_data/trigger_helper.h"
#include "log/default_log_levels.h"
#include "unit_mover.h"
#include "universe.h"

namespace wowpp
{
	CreatureAIResetState::CreatureAIResetState(CreatureAI &ai)
		: CreatureAIState(ai)
	{
		// Enter idle mode when home point was reached
		m_onHomeReached = getControlled().getMover().targetReached.connect([this]() {
			auto &ai = getAI();
			auto *world = ai.getControlled().getWorldInstance();
			if (world)
			{
				auto &universe = world->getUniverse();
				universe.post([&ai]() {
					ai.idle();
				});
			}
		});
	}

	CreatureAIResetState::~CreatureAIResetState()
	{
	}

	void CreatureAIResetState::onEnter()
	{
		auto &controlled = getControlled();
		controlled.removeFlag(unit_fields::DynamicFlags, game::unit_dynamic_flags::Lootable);
		controlled.removeFlag(unit_fields::DynamicFlags, game::unit_dynamic_flags::OtherTagger);
		controlled.raiseTrigger(trigger_event::OnReset);

		m_onStunChanged = getControlled().stunStateChanged.connect([this](bool stunned)
		{
			auto &controlled = getControlled();
			if (!controlled.isStunned() && !controlled.isRooted())
			{
				controlled.getMover().moveTo(getAI().getHome().position);
			}
		});
		m_onRootChanged = getControlled().rootStateChanged.connect([this](bool rooted)
		{
			auto &controlled = getControlled();
			if (!controlled.isStunned() && !controlled.isRooted())
			{
				controlled.getMover().moveTo(getAI().getHome().position);
			}
		});

		controlled.getMover().moveTo(getAI().getHome().position);
	}

	void CreatureAIResetState::onLeave()
	{
		m_onStunChanged.disconnect();
		m_onRootChanged.disconnect();

		auto &controlled = getControlled();

		// Fully heal unit
		if (controlled.isAlive())
		{
			controlled.heal(controlled.getUInt32Value(unit_fields::MaxHealth), nullptr, true);
		}

		controlled.raiseTrigger(trigger_event::OnReachedHome);
	}

}
