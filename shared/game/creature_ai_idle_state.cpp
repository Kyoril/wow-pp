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

#include "creature_ai_idle_state.h"
#include "creature_ai.h"
#include "game_creature.h"
#include "world_instance.h"
#include "tiled_unit_watcher.h"
#include "unit_finder.h"
#include "data/faction_template_entry.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	CreatureAIIdleState::CreatureAIIdleState(CreatureAI &ai)
		: CreatureAIState(ai)
	{
	}

	CreatureAIIdleState::~CreatureAIIdleState()
	{
	}

	void CreatureAIIdleState::onEnter()
	{
		auto &controlled = getControlled();

		// Watch for threat events to enter combat
		m_onThreatened = controlled.threatened.connect([this](GameUnit &threat, float amount)
		{
			// Warning: This may destroy the idle state as it enters the combat state
			getAI().enterCombat(threat);
		});

		auto *worldInstance = controlled.getWorldInstance();
		if (!worldInstance)
		{
			ELOG("Creature entered Idle State but is not in a world instance");
			return;
		}

		float x, y, z, o;
		controlled.getLocation(x, y, z, o);

		Circle circle(x, y, 20.0f);
		m_aggroWatcher = worldInstance->getUnitFinder().watchUnits(circle);
		m_aggroWatcher->visibilityChanged.connect([this](GameUnit& unit, bool isVisible) -> bool
		{
			auto &controlled = getControlled();
			if (&unit == &controlled)
			{
				return false;
			}

			if (!controlled.isAlive())
			{
				return false;
			}

			// Check if we are hostile against this unit
			const auto &ourFaction = controlled.getFactionTemplate();
			const auto &unitFaction = unit.getFactionTemplate();
			if (ourFaction.isNeutralToAll())
			{
				return false;
			}

			if (ourFaction.isHostileTo(unitFaction) && 
				!ourFaction.isFriendlyTo(unitFaction))
			{
				if (isVisible)
				{
					// Little hack since LoS is not working
					float tmp = 0.0f, z2 = 0.0f, z = 0.0f;
					controlled.getLocation(tmp, tmp, z, tmp);
					unit.getLocation(tmp, tmp, z2, tmp);
					if (::abs(z - z2) > 3.0f)
					{
						return false;
					}

					getAI().enterCombat(unit);
					return true;
				}

				// We don't care
				return false;
			}

			return false;
		});

		m_aggroWatcher->start();
	}

	void CreatureAIIdleState::onLeave()
	{

	}

}
