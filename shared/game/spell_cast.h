
#pragma once

#include "game/defines.h"
#include "data/spell_entry.h"
#include "spell_target_map.h"
#include "common/timer_queue.h"
#include <boost/signals2.hpp>

namespace wowpp
{
	class GameUnit;

	/// 
	class SpellCasting final
	{
	public:

		typedef boost::signals2::signal<void(bool)> EndSignal;

	public:

		EndSignal ended;
	};

	/// This class handles spell casting logic for an executing unit object.
	class SpellCast final
	{
	public:

		class CastState;

	public:

		explicit SpellCast(TimerQueue &timer, GameUnit &executer);

		GameUnit &getExecuter() const { return m_executer; }
		TimerQueue &getTimers() const { return m_timers; }

		std::pair<game::SpellCastResult, SpellCasting*> startCast(
			const SpellEntry &spell,
			SpellTargetMap target,
			GameTime castTime,
			bool doReplacePreviousCast);
		void stopCast();
		void onUserStartsMoving();
		void setState(std::unique_ptr<CastState> castState);

	private:

		TimerQueue &m_timers;
		GameUnit &m_executer;
		std::unique_ptr<CastState> m_castState;
	};

	/// 
	class SpellCast::CastState
	{
	public:

		virtual ~CastState() { }

		virtual std::pair<game::SpellCastResult, SpellCasting*> startCast(
			SpellCast &cast,
			const SpellEntry &spell,
			SpellTargetMap target,
			GameTime castTime,
			bool doReplacePreviousCast
			) = 0;
		virtual void stopCast() = 0;
		virtual void onUserStartsMoving() = 0;
	};

	SpellCasting &castSpell(
		SpellCast &cast,
		const SpellEntry &spell,
		SpellTargetMap target,
		GameTime castTime
		);

	bool isInSkillRange(
		const SpellEntry &spell,
		GameUnit &user,
		SpellTargetMap &target);
}
