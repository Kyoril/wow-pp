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
#include "creature_ai_idle_state.h"
#include "creature_ai.h"
#include "game_creature.h"
#include "world_instance.h"
#include "tiled_unit_watcher.h"
#include "unit_finder.h"
#include "unit_mover.h"
#include "universe.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	CreatureAIIdleState::CreatureAIIdleState(CreatureAI &ai)
		: CreatureAIState(ai)
		, m_nextMove(ai.getControlled().getTimers())
		, m_nextUpdate(0)
	{
	}

	CreatureAIIdleState::~CreatureAIIdleState()
	{
	}

	void CreatureAIIdleState::onEnter()
	{
		CreatureAIState::onEnter();

		m_nextMove.ended.connect(std::bind(&CreatureAIIdleState::onChooseNextMove, this));

		auto &controlled = getControlled();

		controlled.getMover().setTerrainMovement(false);

		// Handle incoming threat
		auto &ai = getAI();
		m_onThreatened = controlled.threatened.connect(std::bind(&CreatureAI::onThreatened, &ai, std::placeholders::_1, std::placeholders::_2));

		auto *worldInstance = controlled.getWorldInstance();
		ASSERT(worldInstance);

		// Check if it is a pet
		UInt64 ownerGUID = controlled.getUInt64Value(unit_fields::SummonedBy);
		if (ownerGUID == 0)
		{
			onCreatureMovementChanged();
		}

		// If the unit is passive, it should not attack by itself
		UInt32 unitFlags = controlled.getUInt32Value(unit_fields::UnitFlags);
		if (!(unitFlags & game::unit_flags::Passive) &&
			!(unitFlags & game::unit_flags::NotSelectable))	// Not sure about this one...
		{
			const auto &location = controlled.getLocation();
	
			m_aggroWatcher = worldInstance->getUnitFinder().watchUnits(Circle(location.x, location.y, 40.0f), [this](GameUnit & unit, bool isVisible) -> bool
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

				if (!unit.isAlive())
				{
					return false;
				}

				// Check if we are hostile against this unit
				const auto &unitFaction = unit.getFactionTemplate();
				if (controlled.isNeutralToAll())
				{
					return false;
				}

				const float dist = controlled.getDistanceTo(unit, true);

				const bool isHostile = controlled.isHostileTo(unitFaction);
				const bool isFriendly = controlled.isFriendlyTo(unitFaction);
				if (isHostile && !isFriendly)
				{
					if (isVisible)
					{
						const Int32 ourLevel = static_cast<Int32>(controlled.getLevel());
						const Int32 otherLevel = static_cast<Int32>(unit.getLevel());
						const Int32 diff = ::abs(ourLevel - otherLevel);

						// Check distance
						float reqDist = 20.0f;
						if (ourLevel < otherLevel)
						{
							reqDist = limit<float>(reqDist - diff, 5.0f, 40.0f);
						}
						else if (otherLevel < ourLevel)
						{
							reqDist = limit<float>(reqDist + diff, 5.0f, 40.0f);
						}

						if (dist > reqDist)
						{
							return false;
						}

						// Check stealth
						if (unit.isStealthed())
						{
							if (!controlled.canDetectStealth(unit)) {
								return false;
							}
						}

						if (!controlled.isInLineOfSight(unit))
						{
							return false;
						}

						getAI().enterCombat(unit);
						return true;
					}

					// We don't care
					return false;
				}
				else if (isFriendly)
				{
					if (isVisible)
					{
						if (dist < 8.0f)
						{
							auto *victim = unit.getVictim();
							if (unit.isInCombat() && victim != nullptr)
							{
								if (!victim->isHostileTo(unitFaction)) {
									return false;
								}

								if (!controlled.isInLineOfSight(unit))
								{
									return false;
								}

								getAI().enterCombat(*victim);
								return true;
							}
						}
					}
				}

				return false;
			});
			m_aggroWatcher->start();
		}
	}

	void CreatureAIIdleState::onLeave()
	{
		m_nextMove.cancel();

		onTargetReached.disconnect();
		m_aggroWatcher.reset();

		CreatureAIState::onLeave();
	}

	void CreatureAIIdleState::onCreatureMovementChanged()
	{
		if (getControlled().getMovementType() == game::creature_movement::Random)
		{
			onTargetReached = getControlled().getMover().targetReached.connect([this]() {
				m_nextMove.setEnd(getCurrentTime() + (constants::OneSecond * 4));
			});

			m_nextMove.setEnd(getCurrentTime() + (constants::OneSecond * 2));
		}
	}

	void CreatureAIIdleState::onControlledMoved()
	{
		if (m_aggroWatcher)
		{
			auto now = getCurrentTime();
			if (now > m_nextUpdate)
			{
				// Should be enough
				m_nextUpdate = now + (constants::OneSecond * 1.5f);

				const auto &loc = getControlled().getLocation();
				m_aggroWatcher->setShape(Circle(loc.x, loc.y, 40.0f));
			}
		}
	}

	void CreatureAIIdleState::onChooseNextMove()
	{
		const float dist = 7.5f;
		const auto &loc = getAI().getHome().position;
		const Circle clipping(loc.x, loc.y, dist);

		auto *world = getControlled().getWorldInstance();
		if (world)
		{
			auto *mapData = world->getMapData();
			if (mapData)
			{
				math::Vector3 targetPoint;
				if (mapData->getRandomPointOnGround(getAI().getHome().position, dist, targetPoint))
				{
					if (getControlled().getMover().moveTo(targetPoint, getControlled().getSpeed(movement_type::Walk), &clipping))
					{
						return;
					}
				}
			}
		}

		// Try again in a few seconds
		m_nextMove.setEnd(getCurrentTime() + (constants::OneSecond * 3));
	}
}
