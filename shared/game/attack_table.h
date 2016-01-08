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
#include "game/game_unit.h"
#include "game/game_character.h"
#include "game/world_instance.h"
#include "proto_data/faction_helper.h"
#include <unordered_map>

namespace wowpp
{
	class AttackTable
	{
	public:
		AttackTable();
		void checkMeleeAutoAttack(GameUnit* attacker, GameUnit *target, UInt8 school, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		void checkSpecialMeleeAttack(GameUnit* attacker, SpellTargetMap &targetMap, UInt8 school, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		void checkRangedAttack(GameUnit* attacker, SpellTargetMap &targetMap, UInt8 school, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		void checkPositiveSpell(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		void checkPositiveSpellNoCrit(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		void checkBinarySpell(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		void checkNonBinarySpell(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
		void checkNonBinarySpellNoCrit(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists);
	private:
		std::unordered_map<UInt32,std::unordered_map<UInt32,std::vector<GameUnit*>>> m_targets;
		std::unordered_map<UInt32,std::unordered_map<UInt32,std::vector<game::VictimState>>> m_victimStates;
		std::unordered_map<UInt32,std::unordered_map<UInt32,std::vector<game::HitInfo>>> m_hitInfos;
		std::unordered_map<UInt32,std::unordered_map<UInt32,std::vector<float>>> m_resists;
		
		bool checkIndex(UInt32 targetA, UInt32 targetB);
		void refreshTargets(GameUnit &attacker, SpellTargetMap &targetMap, UInt32 targetA, UInt32 targetB, float radius, UInt32 maxtargets);
	};
}
