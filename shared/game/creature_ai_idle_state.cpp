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
		, m_aggroDelay(ai.getControlled().getTimers())
		, m_nextMove(ai.getControlled().getTimers())
	{
		m_aggroDelay.ended.connect(std::bind(&CreatureAIIdleState::onStartAggroWatcher, this));
		m_nextMove.ended.connect(std::bind(&CreatureAIIdleState::onChooseNextMove, this));
	}

	CreatureAIIdleState::~CreatureAIIdleState()
	{
	}

	void CreatureAIIdleState::onEnter()
	{
		auto &controlled = getControlled();

		// Handle incoming threat
		auto &ai = getAI();
		m_onThreatened = controlled.threatened.connect(std::bind(&CreatureAI::onThreatened, &ai, std::placeholders::_1, std::placeholders::_2));

		auto *worldInstance = controlled.getWorldInstance();
		assert(worldInstance);

		math::Vector3 location(controlled.getLocation());

		// Check if it is a pet
		UInt64 ownerGUID = controlled.getUInt64Value(unit_fields::SummonedBy);
		if (ownerGUID != 0)
		{
			// Find owner
			auto *owner = worldInstance->findObjectByGUID(ownerGUID);
			if (owner)
			{
				m_onOwnerMoved = owner->moved.connect([this](GameObject &obj, const math::Vector3 &oldPosition, float oldRotation)
				{
					const auto &loc = obj.getLocation();
					if (loc != oldPosition)
					{
						// Make this unit follow it's owner
						getControlled().getMover().moveTo(loc);
					}
				});
			}
		}
		else
		{
			onCreatureMovementChanged();
		}

		// If the unit is passive, it should not attack by itself
		UInt32 unitFlags = controlled.getUInt32Value(unit_fields::UnitFlags);
		if (!(unitFlags & game::unit_flags::Passive) &&
			!(unitFlags & game::unit_flags::NotSelectable))	// Not sure about this one...
		{
			Circle circle(location.x, location.y, 40.0f);
			m_aggroWatcher = worldInstance->getUnitFinder().watchUnits(circle);
			m_aggroWatcher->visibilityChanged.connect([this](GameUnit & unit, bool isVisible) -> bool
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

			// Add a little delay
			m_aggroDelay.setEnd(getCurrentTime() + 1000);
		}
	}

	void CreatureAIIdleState::onLeave()
	{
		m_aggroDelay.cancel();
		m_nextMove.cancel();

		onTargetReached.disconnect();
		m_onMoved.disconnect();
		m_onOwnerMoved.disconnect();
		m_aggroWatcher.reset();
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

	void CreatureAIIdleState::onMoved(GameObject & object, const math::Vector3 & oldLocation, float oldRotation)
	{
		const auto &loc = object.getLocation();
		if (loc != oldLocation)
		{
			if (m_aggroWatcher)
			{
				m_aggroWatcher->setShape(Circle(loc.x, loc.y, 40.0f));
			}
		}
	}

	void CreatureAIIdleState::onChooseNextMove()
	{
		auto *world = getControlled().getWorldInstance();
		if (world)
		{
			auto *mapData = world->getMapData();
			if (mapData)
			{
				math::Vector3 targetPoint;
				if (mapData->getRandomPointOnGround(getAI().getHome().position, 8.0f, targetPoint))
				{
					getControlled().getMover().moveTo(targetPoint, getControlled().getSpeed(movement_type::Walk));
					return;
				}
			}
		}

		// Try again in a few seconds
		m_nextMove.setEnd(getCurrentTime() + (constants::OneSecond * 3));
	}

	void CreatureAIIdleState::onStartAggroWatcher()
	{
		// Watch for movement
		m_onMoved = getControlled().moved.connect(
			std::bind(&CreatureAIIdleState::onMoved, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		if (m_aggroWatcher)
		{
			m_aggroWatcher->start();
		}
	}

}
