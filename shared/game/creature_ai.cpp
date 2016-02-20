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

#include "creature_ai.h"
#include "creature_ai_prepare_state.h"
#include "creature_ai_idle_state.h"
#include "creature_ai_combat_state.h"
#include "creature_ai_reset_state.h"
#include "creature_ai_death_state.h"
#include "game_creature.h"
#include "game_unit.h"
#include "world_instance.h"
#include "universe.h"
#include "unit_finder.h"
#include "unit_watcher.h"
#include "common/make_unique.h"

namespace wowpp
{
	CreatureAI::Home::~Home()
	{
	}

	CreatureAI::CreatureAI(GameCreature &controlled, const Home &home)
		: m_controlled(controlled)
		, m_home(home)
	{
		// Connect to spawn event
		m_onSpawned = m_controlled.spawned.connect(
		                  std::bind(&CreatureAI::onSpawned, this));
	}

	CreatureAI::~CreatureAI()
	{
	}

	void CreatureAI::onSpawned()
	{
		m_onKilled = m_controlled.killed.connect([this](GameUnit * killer)
		{
			auto state = make_unique<CreatureAIDeathState>(*this);
			setState(std::move(state));
		});
		m_onDamaged = m_controlled.takenDamage.connect([this](GameUnit * attacker) {
			if (attacker) {
				m_state->onDamage(*attacker);
			}
		});

		// Enter the preparation state
		auto state = make_unique<CreatureAIPrepareState>(*this);
		setState(std::move(state));
	}

	void CreatureAI::idle()
	{
		auto state = make_unique<CreatureAIIdleState>(*this);
		setState(std::move(state));
	}

	void CreatureAI::setState(std::unique_ptr<CreatureAIState> state)
	{
		if (m_state)
		{
			m_state->onLeave();
		}

		assert(state.get());
		m_state = std::move(state);
		m_state->onEnter();
	}

	GameCreature &CreatureAI::getControlled() const
	{
		return m_controlled;
	}

	const CreatureAI::Home &CreatureAI::getHome() const
	{
		return m_home;
	}

	void CreatureAI::enterCombat(GameUnit &victim)
	{
		auto state = make_unique<CreatureAICombatState>(*this, victim);
		setState(std::move(state));
	}

	void CreatureAI::reset()
	{
		auto state = make_unique<CreatureAIResetState>(*this);
		setState(std::move(state));
	}

	void CreatureAI::setHome(Home home)
	{
		m_home = std::move(home);
	}

	void CreatureAI::onThreatened(GameUnit &threat, float amount)
	{
		GameCreature &controlled = getControlled();
		if (threat.getGuid() == controlled.getGuid())
		{
			return;
		}

		auto *worldInstance = controlled.getWorldInstance();
		assert(worldInstance);

		// Check if we are hostile against this unit
		const auto &unitFaction = threat.getFactionTemplate();
		if (!controlled.isFriendlyTo(unitFaction))
		{
			math::Vector3 location(controlled.getLocation());

			// Call for assistance
			if (!controlled.isNeutralToAll())
			{
				worldInstance->getUnitFinder().findUnits(Circle(location.x, location.y, 8.0f), [&controlled, &threat, &worldInstance](GameUnit & unit) -> bool
				{
					if (unit.getTypeId() != object_type::Unit)
						return true;

					if (!unit.isAlive())
						return true;

					if (unit.isInCombat())
						return true;

					const auto &threatFaction = threat.getFactionTemplate();
					const auto &unitFaction = unit.getFactionTemplate();
					if (controlled.isFriendlyTo(unitFaction) &&
					unit.isHostileTo(threatFaction))
					{
						worldInstance->getUniverse().post([&unit, &threat]()
						{
							unit.threatened(threat, 0.0f);
						});
					}

					return false;
				});
			}

			// Warning: This destroys the current AI state as it enters the combat state
			enterCombat(threat);
		}
	}

}
