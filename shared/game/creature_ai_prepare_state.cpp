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

#include "creature_ai_prepare_state.h"
#include "creature_ai.h"
#include "game_creature.h"
#include "world_instance.h"
#include "tiled_unit_watcher.h"
#include "unit_finder.h"
#include "universe.h"
#include "data/faction_template_entry.h"
#include "log/default_log_levels.h"
#include "proto_data/trigger_helper.h"

namespace wowpp
{
	CreatureAIPrepareState::CreatureAIPrepareState(CreatureAI &ai)
		: CreatureAIState(ai)
		, m_preparation(ai.getControlled().getTimers())
	{
		m_preparation.ended.connect([this]()
		{
			// Enter idle state
			getAI().idle();
		});

		// Start preparation timer (3 seconds)
		m_preparation.setEnd(getCurrentTime() + constants::OneSecond * 6);
	}

	CreatureAIPrepareState::~CreatureAIPrepareState()
	{
	}

	void CreatureAIPrepareState::onEnter()
	{
		// Watch for threat events to enter combat
		auto &ai = getAI();
		m_onThreatened = getControlled().threatened.connect(std::bind(&CreatureAI::onThreatened, &ai, std::placeholders::_1, std::placeholders::_2));

		auto &controlled = getControlled();
		controlled.raiseTrigger(trigger_event::OnSpawn);
	}

	void CreatureAIPrepareState::onLeave()
	{

	}

}
