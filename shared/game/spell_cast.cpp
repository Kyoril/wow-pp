
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

	std::pair<game::SpellCastResult, SpellCasting*> SpellCast::startCast(const SpellEntry &spell, SpellTargetMap target, GameTime castTime, bool doReplacePreviousCast)
	{
		assert(m_castState);

		GameUnit *unitTarget = nullptr;
		target.resolvePointers(*m_executer.getWorldInstance(), &unitTarget, nullptr, nullptr, nullptr);

		// Check power
		if (spell.cost > 0)
		{
			if (spell.powerType == power_type::Health)
			{
				// Special case
				DLOG("TODO: Spell cost power type Health");
			}
			else
			{
				UInt32 currentPower = m_executer.getUInt32Value(unit_fields::Power1 + spell.powerType);
				if (currentPower < spell.cost)
				{
					return std::make_pair(game::spell_cast_result::FailedNoPower, nullptr);
				}
			}
		}

		// Range check
		if (spell.minRange != 0.0f || spell.maxRange != 0.0f)
		{
			if (unitTarget)
			{
				const float distance = m_executer.getDistanceTo(*unitTarget);
				if (spell.minRange > 0.0f && distance < spell.minRange)
				{
					return std::make_pair(game::spell_cast_result::FailedTooClose, nullptr);
				}
				else if (spell.maxRange > 0.0f && distance > spell.maxRange)
				{
					return std::make_pair(game::spell_cast_result::FailedOutOfRange, nullptr);
				}
			}
		}

		// TODO: Check spell conditions

		// Check facing (Need to have the target in front of us)
		if (spell.facing & 0x01)
		{
			const auto *world = m_executer.getWorldInstance();
			if (world)
			{
				if (unitTarget)
				{
					float x, y, z, o;
					unitTarget->getLocation(x, y, z, o);

					// 120 degree field of view
					if (!m_executer.isInArc(2.0f * 3.1415927f / 3.0f, x, y))
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
		return m_castState->startCast
			(*this,
			spell,
			target,
			castTime,
			doReplacePreviousCast);
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

	void SpellCast::setState(std::unique_ptr<CastState> castState)
	{
		assert(castState);
		assert(m_castState);

		m_castState = std::move(castState);
	}

	SpellCasting & castSpell(SpellCast &cast, const SpellEntry &spell, SpellTargetMap target, GameTime castTime)
	{
		std::unique_ptr<SingleCastState> newState(
			new SingleCastState(cast, spell, std::move(target), castTime)
			);

		auto &casting = newState->getCasting();
		cast.setState(std::move(newState));
		return casting;
	}

	bool isInSkillRange(const SpellEntry &spell, GameUnit &user, SpellTargetMap &target)
	{
		//TODO
		return true;
	}

}
