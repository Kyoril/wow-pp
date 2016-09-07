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

namespace wowpp
{
	struct HitResult final
	{
		UInt32 procAttacker;
		UInt32 procVictim;
		UInt32 procEx;
		UInt32 amount;

		explicit HitResult(UInt32 attackerProc, UInt32 victimProc, const game::HitInfo &hitInfo, const game::VictimState &victimState, float resisted = 0.0f, UInt32 damage = 0, UInt32 absorbed = 0, bool isDamage = false)
			: procAttacker(attackerProc)
			, procVictim(victimProc)
			, procEx(game::spell_proc_flags_ex::None)
			, amount(0)
		{
			add(hitInfo, victimState, resisted, damage, absorbed, isDamage);
		}

		void add(const game::HitInfo &hitInfo, const game::VictimState &victimState, float resisted = 0.0f, UInt32 damage = 0, UInt32 absorbed = 0, bool isDamage = false)
		{
			using namespace game;
			amount += damage - absorbed;
			amount += damage * (resisted / 100.0f);

			switch (hitInfo)
			{
			case hit_info::Miss:
				procEx |= spell_proc_flags_ex::Miss;
				break;
			case hit_info::CriticalHit:
				procEx |= spell_proc_flags_ex::CriticalHit;
				break;
			default:
				procEx |= spell_proc_flags_ex::NormalHit;
				break;
			}

			switch (victimState)
			{
			case victim_state::Dodge:
				procEx |= spell_proc_flags_ex::Dodge;
				break;
			case victim_state::Parry:
				procEx |= spell_proc_flags_ex::Parry;
				break;
			case victim_state::Interrupt:
				procEx |= spell_proc_flags_ex::Interrupt;
				break;
			case victim_state::Blocks:
				procEx |= spell_proc_flags_ex::Block;
				break;
			case victim_state::Evades:
				procEx |= spell_proc_flags_ex::Evade;
				break;
			case victim_state::IsImmune:
				procEx |= spell_proc_flags_ex::Immune;
				break;
			case victim_state::Deflects:
				procEx |= spell_proc_flags_ex::Deflect;
				break;
			default:
				break;
			}

			if (resisted == 100.0f)
			{
				procEx |= spell_proc_flags_ex::Resist;
				procEx &= ~spell_proc_flags_ex::NormalHit;
			}
			else if (damage && absorbed)
			{
				if (absorbed == damage)
				{
					procEx &= ~spell_proc_flags_ex::NormalHit;
				}
				procEx |= spell_proc_flags_ex::Absorb;
			}
			else if (amount && isDamage)
			{
				procVictim |= spell_proc_flags::TakenDamage;
			}
		}
	};

	// Forwards
	class GameUnit;
	class WorldInstance;
	class SpellTargetMap;

	/// This class is used to calculate spell hits, resistances etc.
	class AttackTable
	{
	public:

		/// Default constructor.
		explicit AttackTable();

		/// 
		/// @param attacker
		/// @param targetMap
		/// @param spell
		/// @param effect
		/// @param targets
		/// @param victimStates
		/// @param hitInfos
		/// @param resists
		void checkMeleeAutoAttack(GameUnit *attacker, GameUnit *target, UInt8 school, std::vector<GameUnit *> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		/// 
		/// @param attacker
		/// @param targetMap
		/// @param spell
		/// @param effect
		/// @param targets
		/// @param victimStates
		/// @param hitInfos
		/// @param resists
		void checkSpecialMeleeAttack(GameUnit *attacker, const proto::SpellEntry &spell, SpellTargetMap &targetMap, UInt8 school, std::vector<GameUnit *> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		/// 
		/// @param attacker
		/// @param targetMap
		/// @param spell
		/// @param effect
		/// @param targets
		/// @param victimStates
		/// @param hitInfos
		/// @param resists
		void checkSpecialMeleeAttackNoCrit(GameUnit *attacker, SpellTargetMap &targetMap, UInt8 school, std::vector<GameUnit *> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		/// 
		/// @param attacker
		/// @param targetMap
		/// @param spell
		/// @param effect
		/// @param targets
		/// @param victimStates
		/// @param hitInfos
		/// @param resists
		void checkRangedAttack(GameUnit *attacker, SpellTargetMap &targetMap, UInt8 school, std::vector<GameUnit *> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		/// 
		/// @param attacker
		/// @param targetMap
		/// @param spell
		/// @param effect
		/// @param targets
		/// @param victimStates
		/// @param hitInfos
		/// @param resists
		void checkPositiveSpell(GameUnit *attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit *> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		/// 
		/// @param attacker
		/// @param targetMap
		/// @param spell
		/// @param effect
		/// @param targets
		/// @param victimStates
		/// @param hitInfos
		/// @param resists
		void checkPositiveSpellNoCrit(GameUnit *attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit *> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		/// 
		/// @param attacker
		/// @param targetMap 
		/// @param spell
		/// @param effect
		/// @param targets
		/// @param victimStates
		/// @param hitInfos 
		/// @param resists 
		void checkSpell(GameUnit *attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit *> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);

	private:

		/// Validates whether two target indices are valid in this constellation.
		/// @param targetA The first target index as used in DBC files.
		/// @param targetB The second target index as used in DBC files.
		/// @return true if these indices are valid, false otherwise.
		bool checkIndex(UInt32 targetA, UInt32 targetB);
		/// 
		/// @param attacker
		/// @param targetMap
		/// @param targetA
		/// @param targetB
		/// @param radius
		/// @param maxtargets
		void refreshTargets(GameUnit &attacker, SpellTargetMap &targetMap, UInt32 targetA, UInt32 targetB, float radius, UInt32 maxtargets);

	private:

		std::unordered_map<UInt32, std::unordered_map<UInt32, std::vector<GameUnit *>>> m_targets;
		std::unordered_map<UInt32, std::unordered_map<UInt32, std::vector<game::VictimState>>> m_victimStates;
		std::unordered_map<UInt32, std::unordered_map<UInt32, std::vector<game::HitInfo>>> m_hitInfos;
		std::unordered_map<UInt32, std::unordered_map<UInt32, std::vector<float>>> m_resists;
	};
}
