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
#include "game_object.h"
#include "shared/proto_data/spells.pb.h"

namespace wowpp
{
	typedef std::map<UInt64, HitResult> HitResultMap;

	///
	class SingleCastState final
		: public SpellCast::CastState
		, public std::enable_shared_from_this<SingleCastState>
	{
	private:

		SingleCastState(const SingleCastState& Other) = delete;
		SingleCastState &operator=(const SingleCastState &Other) = delete;

	public:

		explicit SingleCastState(
		    SpellCast &cast,
		    const proto::SpellEntry &spell,
		    SpellTargetMap target,
			const game::SpellPointsArray &basePoints,
		    GameTime castTime,
		    bool isProc = false,
		    UInt64 itemGuid = 0);
		void activate() override;
		std::pair<game::SpellCastResult, SpellCasting *> startCast(
		    SpellCast &cast,
		    const proto::SpellEntry &spell,
		    SpellTargetMap target,
			const game::SpellPointsArray &basePoints,
		    GameTime castTime,
		    bool doReplacePreviousCast,
		    UInt64 itemGuid) override;
		void stopCast(game::SpellInterruptFlags reason, UInt64 interruptCooldown = 0) override;
		void onUserStartsMoving() override;
		void finishChanneling() override;
		SpellCasting &getCasting() {
			return m_casting;
		}

		template <class T>
		void sendPacketFromCaster(GameUnit &caster, T generator)
		{
			auto *worldInstance = caster.getWorldInstance();
			if (!worldInstance)
			{
				return;
			}

			TileIndex2D tileIndex;
			worldInstance->getGrid().getTilePosition(caster.getLocation(), tileIndex[0], tileIndex[1]);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			generator(packet);

			forEachSubscriberInSight(
				worldInstance->getGrid(),
				tileIndex,
				[&buffer, &packet](ITileSubscriber & subscriber)
			{
				subscriber.sendPacket(
					packet,
					buffer
				);
			});
		}

		template <class T>
		void sendPacketToCaster(GameUnit &caster, T generator)
		{
			auto *worldInstance = caster.getWorldInstance();
			if (!worldInstance)
			{
				return;
			}

			TileIndex2D tileIndex;
			worldInstance->getGrid().getTilePosition(caster.getLocation(), tileIndex[0], tileIndex[1]);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			generator(packet);

			forEachSubscriberInSight(
				worldInstance->getGrid(),
				tileIndex,
				[&buffer, &packet, &caster](ITileSubscriber & subscriber)
			{
				if (subscriber.getControlledObject() == &caster)
				{
					subscriber.sendPacket(
						packet,
						buffer
					);
				}

			});
		}

	public:

		/// Determines if this spell is a channeled spell.
		bool isChanneled() const { return (m_spell.attributes(1) & game::spell_attributes_ex_a::Channeled_1) || (m_spell.attributes(1) & game::spell_attributes_ex_a::Channeled_2); }
		/// Determines if this spell has a charge effect.
		bool hasChargeEffect() const;

	private:

		bool consumeItem(bool delayed = true);
		bool consumeReagents(bool delayed = true);
		bool consumePower();
		void applyCooldown(UInt64 cooldownTimeMS, UInt64 catCooldownTimeMS);
		void applyAllEffects(bool executeInstants, bool executeDelayed);
		Int32 calculateEffectBasePoints(const proto::SpellEffect &effect);
		UInt32 getSpellPointsTotal(const proto::SpellEffect &effect, UInt32 spellPower, UInt32 bonusPct);
		void meleeSpecialAttack(const proto::SpellEffect &effect, bool basepointsArePct);

		// Spell effect handlers implemented in spell_effects.cpp and spell_effects_scripted.cpp

		void spellEffectInstantKill(const proto::SpellEffect &effect);
		void spellEffectDummy(const proto::SpellEffect &effect);
		void spellEffectSchoolDamage(const proto::SpellEffect &effect);
		void spellEffectTeleportUnits(const proto::SpellEffect &effect);
		void spellEffectApplyAura(const proto::SpellEffect &effect);
		void spellEffectPersistentAreaAura(const proto::SpellEffect &effect);
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
		void spellEffectSummonPet(const proto::SpellEffect &effect);
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
		void spellEffectInterruptCast(const proto::SpellEffect &effect);
		void spellEffectLearnSpell(const proto::SpellEffect &effect);
		void spellEffectScriptEffect(const proto::SpellEffect &effect);
		void spellEffectDispelMechanic(const proto::SpellEffect &effect);
		void spellEffectResurrect(const proto::SpellEffect &effect);
		void spellEffectResurrectNew(const proto::SpellEffect &effect);
		void spellEffectKnockBack(const proto::SpellEffect &effect);
		void spellEffectSkill(const proto::SpellEffect &effect);


	private:

		std::shared_ptr<GameItem> getItem() const;

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
		simple::signal<void()> completedEffects;
		std::unordered_map<UInt64, simple::scoped_connection> m_completedEffectsExecution;
		simple::scoped_connection m_onTargetDied;
		simple::scoped_connection m_onTargetRemoved, m_damaged;
		simple::scoped_connection m_onThreatened;
		simple::scoped_connection m_onAttackError, m_removeAurasOnImmunity;
		float m_x, m_y, m_z;
		GameTime m_castTime;
		GameTime m_castEnd;
		game::SpellPointsArray m_basePoints;
		bool m_isProc;
		UInt64 m_itemGuid;
		GameTime m_projectileStart, m_projectileEnd;
		math::Vector3 m_projectileOrigin, m_projectileDest;
		bool m_connectedMeleeSignal;
		UInt32 m_delayCounter;
		std::set<std::weak_ptr<GameObject>, std::owner_less<std::weak_ptr<GameObject>>> m_affectedTargets;
		bool m_tookCastItem;
		bool m_tookReagents;
		UInt32 m_attackerProc;
		UInt32 m_victimProc;
		bool m_canTrigger;
		HitResultMap m_hitResults;
		game::WeaponAttack m_attackType;
		std::vector<UInt64> m_dynObjectsToDespawn;
		bool m_instantsCast, m_delayedCast;
		simple::scoped_connection m_onChannelAuraRemoved;

		void sendEndCast(bool success);
		void onCastFinished();
		void onTargetKilled(GameUnit *);
		void onTargetDespawned(GameObject &);
		void onUserDamaged();
		void executeMeleeAttack();	// deal damage stored in m_meleeDamage

		typedef std::function<void(const wowpp::proto::SpellEffect &)> EffectHandler;
	};
}
