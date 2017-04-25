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
#include "unit_mover.h"

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
		m_onSpawned = m_controlled.spawned.connect(this, &CreatureAI::onSpawned);
		m_onDespawned = m_controlled.despawned.connect(this, &CreatureAI::onDespawned);
	}

	CreatureAI::~CreatureAI()
	{
	}

	void CreatureAI::onSpawned()
	{
		m_onKilled = m_controlled.killed.connect([this](GameUnit * killer)
		{
			auto state = std::make_shared<CreatureAIDeathState>(*this);
			setState(std::move(state));
		});
		m_onDamaged = m_controlled.takenDamage.connect([this](GameUnit * attacker, UInt32 damage, game::DamageType type) {
			if (attacker) {
				m_state->onDamage(*attacker);
			}
		});

		// Initialize the units mover
		auto &mover = m_controlled.getMover();
		mover.moveTo(mover.getCurrentLocation());

		// Enter the preparation state
		auto state = std::make_shared<CreatureAIPrepareState>(*this);
		setState(std::move(state));
	}

	void CreatureAI::onDespawned(GameObject &)
	{
		setState(nullptr);
	}

	void CreatureAI::idle()
	{
		auto state = std::make_shared<CreatureAIIdleState>(*this);
		setState(std::move(state));
	}

	void CreatureAI::setState(CreatureAI::CreatureAIStatePtr state)
	{
		if (m_state)
		{
			m_state->onLeave();
			m_state.reset();
		}

		// Every state change causes the creature to leave evade mode
		m_evading = false;

		if (state)
		{
			m_state = std::move(state);
			m_state->onEnter();
		}
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
		auto state = std::make_shared<CreatureAICombatState>(*this, victim);
		setState(std::move(state));
	}

	void CreatureAI::reset()
	{
		auto state = std::make_shared<CreatureAIResetState>(*this);
		setState(std::move(state));

		// We are now evading
		m_evading = true;
	}

	void CreatureAI::onCombatMovementChanged()
	{
		if (m_state)
		{
			m_state->onCombatMovementChanged();
		}
	}

	void CreatureAI::onCreatureMovementChanged()
	{
		if (m_state)
		{
			m_state->onCreatureMovementChanged();
		}
	}

	void CreatureAI::onControlledMoved()
	{
		if (m_state)
		{
			m_state->onControlledMoved();
		}
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
		ASSERT(worldInstance);

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

					if (!controlled.isInLineOfSight(unit))
						return false;

					const auto &threatFaction = threat.getFactionTemplate();
					const auto &unitFaction = unit.getFactionTemplate();
					if (controlled.isFriendlyTo(unitFaction) &&
					unit.isHostileTo(threatFaction))
					{
						worldInstance->getUniverse().post([&unit, &threat]()
						{
							unit.threaten(threat, 0.0f);
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
