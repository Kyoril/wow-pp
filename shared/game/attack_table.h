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
		void checkSpecialMeleeAttack(GameUnit *attacker, SpellTargetMap &targetMap, UInt8 school, std::vector<GameUnit *> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
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
