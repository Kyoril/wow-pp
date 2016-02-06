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

#include "spell_cast.h"
#include "attack_table.h"
#include "common/countdown.h"
#include "shared/proto_data/spells.pb.h"
#include "boost/signals2.hpp"
#include <boost/noncopyable.hpp>
#include <memory>
#include <unordered_map>

namespace wowpp
{
	/// 
	class SingleCastState final : public SpellCast::CastState, public std::enable_shared_from_this<SingleCastState>, public boost::noncopyable
	{
	public:

		explicit SingleCastState(
			SpellCast &cast,
			const proto::SpellEntry &spell,
			SpellTargetMap target,
			Int32 basePoints,
			GameTime castTime,
			bool isProc = false,
			UInt64 itemGuid = 0);
		void activate() override;
		std::pair<game::SpellCastResult, SpellCasting *> startCast(
			SpellCast &cast,
			const proto::SpellEntry &spell,
			SpellTargetMap target,
			Int32 basePoints,
			GameTime castTime,
			bool doReplacePreviousCast,
			UInt64 itemGuid) override;
		void stopCast() override;
		void onUserStartsMoving() override;
		SpellCasting &getCasting() { return m_casting; }

	private:

		bool consumeItem();
		bool consumePower();
		void applyAllEffects();
		Int32 calculateEffectBasePoints(const proto::SpellEffect &effect);
		UInt32 getSpellPointsTotal(const proto::SpellEffect &effect, UInt32 spellPower, UInt32 bonusPct);
		void spellEffectInstantKill(const proto::SpellEffect &effect);
		void spellEffectSchoolDamage(const proto::SpellEffect &effect);
		void spellEffectTeleportUnits(const proto::SpellEffect &effect);
		void spellEffectApplyAura(const proto::SpellEffect &effect);
		void spellEffectDrainPower(const proto::SpellEffect &effect);
		void spellEffectHeal(const proto::SpellEffect &effect);
		void spellEffectBind(const proto::SpellEffect &effect);
		void spellEffectQuestComplete(const proto::SpellEffect &effect);
		void spellEffectWeaponDamageNoSchool(const proto::SpellEffect &effect);
		void spellEffectCreateItem(const proto::SpellEffect &effect);
		void spellEffectEnergize(const proto::SpellEffect &effect);
		void spellEffectWeaponPercentDamage(const proto::SpellEffect &effect);
		void spellEffectOpenLock(const proto::SpellEffect &effect);
		void spellEffectApplyAreaAuraParty(const proto::SpellEffect &effect);
		void spellEffectDispel(const proto::SpellEffect &effect);
		void spellEffectSummon(const proto::SpellEffect &effect);
		void spellEffectWeaponDamage(const proto::SpellEffect &effect);
		void spellEffectProficiency(const proto::SpellEffect &effect);
		void spellEffectPowerBurn(const proto::SpellEffect &effect);
		void spellEffectTriggerSpell(const proto::SpellEffect &effect);
		void spellEffectScript(const proto::SpellEffect &effect);
		void spellEffectAddComboPoints(const proto::SpellEffect &effect);
		void spellEffectDuel(const proto::SpellEffect &effect);
		void spellEffectCharge(const proto::SpellEffect &effect);
		void spellEffectAttackMe(const proto::SpellEffect &effect);
		void spellEffectNormalizedWeaponDamage(const proto::SpellEffect &effect);
		void spellEffectStealBeneficialBuff(const proto::SpellEffect &effect);
		
		void meleeSpecialAttack(const proto::SpellEffect &effect, bool basepointsArePct);

	private:

		SpellCast &m_cast;
		const proto::SpellEntry &m_spell;
		SpellTargetMap m_target;
		SpellCasting m_casting;
		AttackTable m_attackTable;
		std::vector<UInt32> m_meleeDamage;	// store damage results of melee effects
		bool m_hasFinished;
		Countdown m_countdown;
		Countdown m_impactCountdown;
		boost::signals2::signal<void()> completedEffects;
		boost::signals2::scoped_connection m_completedEffectsExecution;
		boost::signals2::scoped_connection m_onTargetDied, m_onTargetRemoved;
		boost::signals2::scoped_connection m_onUserDamaged, m_onUserMoved;
		float m_x, m_y, m_z;
		GameTime m_castTime;
		Int32 m_basePoints;
		bool m_isProc;
		UInt64 m_itemGuid;

		void sendEndCast(bool success);
		void onCastFinished();
		void onTargetRemovedOrDead();
		void onUserDamaged();
		void executeMeleeAttack();	// deal damage stored in m_meleeDamage
		
		typedef std::function<void(const wowpp::proto::SpellEffect&)> EffectHandler;
	};
}
