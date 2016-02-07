
#include "spell_cast.h"
#include "game_unit.h"
#include "common/countdown.h"
#include "no_cast_state.h"
#include "single_cast_state.h"
#include "log/default_log_levels.h"
#include "world_instance.h"
#include <cassert>

namespace wowpp
{
	SpellCast::SpellCast(TimerQueue &timers, GameUnit &executer)
		: m_timers(timers)
		, m_executer(executer)
		, m_castState(new NoCastState)
	{
	}

	std::pair<game::SpellCastResult, SpellCasting*> SpellCast::startCast(const proto::SpellEntry &spell, SpellTargetMap target, Int32 basePoints, GameTime castTime, bool isProc, UInt64 itemGuid)
	{
		assert(m_castState);

		GameUnit *unitTarget = nullptr;
		target.resolvePointers(*m_executer.getWorldInstance(), &unitTarget, nullptr, nullptr, nullptr);

		// Check if caster is dead
		if ((spell.attributes(0) & game::spell_attributes::CastableWhileDead) == 0)
		{
			if (!m_executer.isAlive())
			{
				return std::make_pair(game::spell_cast_result::FailedCasterDead, nullptr);
			}
		}

		// Check for cooldown
		if (m_executer.hasCooldown(spell.id()))
		{
			return std::make_pair(game::spell_cast_result::FailedNotReady, nullptr);
		}

		auto *instance = m_executer.getWorldInstance();
		if (!instance)
		{
			ELOG("Caster is not in a world instance");
			return std::make_pair(game::spell_cast_result::FailedError, nullptr);
		}

		auto *map = instance->getMapData();
		if (!map)
		{
			ELOG("World instance has no map data loaded");
			return std::make_pair(game::spell_cast_result::FailedError, nullptr);
		}

		if (spell.mechanic() != 0)
		{
			if (unitTarget)
			{
				if (unitTarget->isImmuneAgainstMechanic(spell.mechanic()))
				{
					return std::make_pair(game::spell_cast_result::FailedPreventedByMechanic, nullptr);
				}
			}
		}

		//Must be behind the target.
		if (unitTarget)
		{
			math::Vector3 location(m_executer.getLocation());

			if (spell.attributes(2) == 0x100000 &&
				(spell.attributes(1) & game::spell_attributes_ex_a::MeleeCombatStart) == game::spell_attributes_ex_a::MeleeCombatStart &&
				unitTarget->isInArc(3.1415927f, location.x, location.y))
			{
				return std::make_pair(game::spell_cast_result::FailedNotBehind, nullptr);
			}
		}
		
		// Check power
		if (spell.cost() > 0)
		{
			if (spell.powertype() == game::power_type::Health)
			{
				// Special case
				DLOG("TODO: Spell cost power type Health");
			}
			else
			{
				UInt32 currentPower = m_executer.getUInt32Value(unit_fields::Power1 + spell.powertype());
				if (currentPower < spell.cost())
				{
					return std::make_pair(game::spell_cast_result::FailedNoPower, nullptr);
				}
			}
		}

		// Range check
		if (spell.minrange() != 0.0f || spell.maxrange() != 0.0f)
		{
			if (unitTarget)
			{
				const float combatReach = unitTarget->getFloatValue(unit_fields::CombatReach) + m_executer.getFloatValue(unit_fields::CombatReach);
				const float distance = m_executer.getDistanceTo(*unitTarget);
				if (spell.minrange() > 0.0f && distance < spell.minrange())
				{
					return std::make_pair(game::spell_cast_result::FailedTooClose, nullptr);
				}
				else if (spell.maxrange() > 0.0f && distance > spell.maxrange() + combatReach)
				{
					return std::make_pair(game::spell_cast_result::FailedOutOfRange, nullptr);
				}
			}
		}

		// Check facing (Need to have the target in front of us)
		if (spell.facing() & 0x01)
		{
			const auto *world = m_executer.getWorldInstance();
			if (world)
			{
				if (unitTarget)
				{
					math::Vector3 location(unitTarget->getLocation());

					// 120 degree field of view
					if (!m_executer.isInArc(2.0f * 3.1415927f / 3.0f, location.x, location.y))
					{
						return std::make_pair(game::spell_cast_result::FailedUnitNotInfront, nullptr);
					}
				}
				else
				{
					WLOG("Couldn't find unit target for spell with facing requirement!");
				}
			}
		}

		// Check if we have enough resources for that spell
		if (isProc)
		{
			std::shared_ptr<SingleCastState> newState(
				new SingleCastState(*this, spell, std::move(target), basePoints, castTime, true, itemGuid)
				);
			newState->activate();

			return std::make_pair(game::spell_cast_result::CastOkay, nullptr);
		}
		else
		{
			// Check for line of sight
			{
				if (unitTarget && unitTarget != &m_executer)
				{
					if (!m_executer.isInLineOfSight(*unitTarget))
					{
						return std::make_pair(game::spell_cast_result::FailedLineOfSight, nullptr);
					}
				}
			}

			return m_castState->startCast
				(*this,
					spell,
					std::move(target),
					basePoints,
					castTime,
					false,
					itemGuid);
		}
	}

	void SpellCast::stopCast()
	{
		assert(m_castState);
		m_castState->stopCast();
	}

	void SpellCast::onUserStartsMoving()
	{
		assert(m_castState);
		m_castState->onUserStartsMoving();
	}

	void SpellCast::setState(std::shared_ptr<CastState> castState)
	{
		assert(castState);
		assert(m_castState);

		m_castState = std::move(castState);
		m_castState->activate();
	}

	SpellCasting & castSpell(SpellCast &cast, const proto::SpellEntry &spell, SpellTargetMap target, Int32 basePoints, GameTime castTime, UInt64 itemGuid)
	{
		std::shared_ptr<SingleCastState> newState(
			new SingleCastState(cast, spell, std::move(target), basePoints, castTime, false, itemGuid)
			);

		auto &casting = newState->getCasting();
		cast.setState(std::move(newState));
		return casting;
	}
}
