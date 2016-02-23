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

#pragma once

#include "game/defines.h"
#include "shared/proto_data/spells.pb.h"
#include "spell_target_map.h"
#include "common/timer_queue.h"

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

		GameUnit &getExecuter() const {
			return m_executer;
		}
		TimerQueue &getTimers() const {
			return m_timers;
		}

		std::pair<game::SpellCastResult, SpellCasting *> startCast(
		    const proto::SpellEntry &spell,
		    SpellTargetMap target,
		    Int32 basePoints,
		    GameTime castTime,
		    bool isProc,
		    UInt64 itemGuid);
		void stopCast(UInt64 interruptCooldown = 0);
		void onUserStartsMoving();
		void setState(std::shared_ptr<CastState> castState);

	private:

		TimerQueue &m_timers;
		GameUnit &m_executer;
		std::shared_ptr<CastState> m_castState;
	};

	///
	class SpellCast::CastState
	{
	public:

		virtual ~CastState() { }

		virtual void activate() = 0;
		virtual std::pair<game::SpellCastResult, SpellCasting *> startCast(
		    SpellCast &cast,
		    const proto::SpellEntry &spell,
		    SpellTargetMap target,
		    Int32 basePoints,
		    GameTime castTime,
		    bool doReplacePreviousCast,
		    UInt64 itemGuid
		) = 0;
		virtual void stopCast(UInt64 interruptCooldown = 0) = 0;
		virtual void onUserStartsMoving() = 0;
	};

	SpellCasting &castSpell(
	    SpellCast &cast,
	    const proto::SpellEntry &spell,
	    SpellTargetMap target,
	    Int32 basePoints,
	    GameTime castTime,
	    UInt64 itemGuid
	);
}
