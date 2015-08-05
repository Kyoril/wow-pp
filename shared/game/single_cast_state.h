//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "spell_cast.h"
#include "common/countdown.h"
#include "data/spell_entry.h"
#include "boost/signals2.hpp"
#include <memory>

namespace wowpp
{
	/// 
	class SingleCastState final : public SpellCast::CastState
	{
	public:

		explicit SingleCastState(
			SpellCast &cast,
			const SpellEntry &spell,
			SpellTargetMap target,
			GameTime castTime);
		std::pair<game::SpellCastResult, SpellCasting *> startCast(
			SpellCast &cast,
			const SpellEntry &spell,
			SpellTargetMap target,
			GameTime castTime,
			bool doReplacePreviousCast) override;
		void stopCast() override;
		void onUserStartsMoving() override;
		SpellCasting &getCasting() { return m_casting; }

	private:

		Int32 calculateEffectBasePoints(const SpellEntry::Effect &effect);
		void spellEffectTeleportUnits(const SpellEntry::Effect &effect);
		void spellEffectSchoolDamage(const SpellEntry::Effect &effect);
		void spellEffectHeal(const SpellEntry::Effect &effect);
		void spellEffectNormalizedWeaponDamage(const SpellEntry::Effect &effect);
		void spellEffectWeaponDamageNoSchool(const SpellEntry::Effect &effect);
		void spellEffectWeaponDamage(const SpellEntry::Effect &effect);
		void spellEffectDrainPower(const SpellEntry::Effect &effect);
		void spellEffectProficiency(const SpellEntry::Effect &effect);
		void spellEffectAddComboPoints(const SpellEntry::Effect &effect);
		void spellEffectApplyAura(const SpellEntry::Effect &effect);

	private:

		SpellCast &m_cast;
		const SpellEntry &m_spell;
		SpellTargetMap m_target;
		SpellCasting m_casting;
		bool m_hasFinished;
		Countdown m_countdown;
		std::shared_ptr<char> m_isAlive;
		boost::signals2::scoped_connection m_onTargetDied, m_onTargetRemoved;
		boost::signals2::scoped_connection m_onUserDamaged, m_onUserMoved;
		float m_x, m_y, m_z;
		GameTime m_castTime;

		void sendEndCast(bool success);
		void onCastFinished();
		void onTargetRemovedOrDead();
		void onUserDamaged();
	};
}
