
#include "spell_cast.h"
#include "game_unit.h"
#include "common/countdown.h"
#include "no_cast_state.h"
#include "single_cast_state.h"
#include "log/default_log_levels.h"
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

		// TODO: Check spell conditions

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
