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

#include "attack_table.h"

namespace wowpp
{
	AttackTable::AttackTable() {
	}
	
	void AttackTable::checkPositiveSpell(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
	{
		UInt32 targetA = effect.targeta();
		UInt32 targetB = effect.targetb();
		if (!checkIndex(targetA, targetB))
		{
			UInt8 school = spell.schoolmask();
			refreshTargets(*attacker, targetMap, targetA, targetB, effect.radius(), spell.maxtargets());
			std::uniform_real_distribution<float> hitTableDistribution(0.0f, 99.9f);

			for (GameUnit* targetUnit : m_targets[targetA][targetB])
			{
				m_hitInfos[targetA][targetB].push_back(game::hit_info::NoAction);
				m_victimStates[targetA][targetB].push_back(game::victim_state::Unknown1);
				float hitTableRoll = hitTableDistribution(randomGenerator);
				if (targetUnit->isImmune(school))
				{
					m_victimStates[targetA][targetB].back() = game::victim_state::IsImmune;
				}
				else if ((hitTableRoll -= targetUnit->getCritChance(*attacker, school)) < 0.0f)
				{
					m_hitInfos[targetA][targetB].back() = game::hit_info::CriticalHit;
				}
				m_resists[targetA][targetB].push_back(0.0f);
			}
		}
		
		targets = m_targets[targetA][targetB];
		victimStates = m_victimStates[targetA][targetB];
		hitInfos = m_hitInfos[targetA][targetB];
		resists = m_resists[targetA][targetB];
	}
	
	void AttackTable::checkPositiveSpellNoCrit(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
	{
		UInt32 targetA = effect.targeta();
		UInt32 targetB = effect.targetb();
		if (!checkIndex(targetA, targetB))
		{
			UInt8 school = spell.schoolmask();
			refreshTargets(*attacker, targetMap, targetA, targetB, effect.radius(), spell.maxtargets());

			for (GameUnit* targetUnit : m_targets[targetA][targetB])
			{
				m_hitInfos[targetA][targetB].push_back(game::hit_info::NoAction);
				m_victimStates[targetA][targetB].push_back(game::victim_state::Unknown1);
				if (targetUnit->isImmune(school))
				{
					m_victimStates[targetA][targetB].back() = game::victim_state::IsImmune;
				}
				m_resists[targetA][targetB].push_back(0.0f);
			}
		}
		
		targets = m_targets[targetA][targetB];
		victimStates = m_victimStates[targetA][targetB];
		hitInfos = m_hitInfos[targetA][targetB];
		resists = m_resists[targetA][targetB];
	}
	
	void AttackTable::checkNonBinarySpell(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
	{
		UInt32 targetA = effect.targeta();
		UInt32 targetB = effect.targetb();
		if (!checkIndex(targetA, targetB))
		{
			UInt8 school = spell.schoolmask();
			refreshTargets(*attacker, targetMap, targetA, targetB, effect.radius(), spell.maxtargets());
			std::uniform_real_distribution<float> hitTableDistribution(0.0f, 99.9f);

			for (GameUnit* targetUnit : m_targets[targetA][targetB])
			{
				m_hitInfos[targetA][targetB].push_back(game::hit_info::NoAction);
				m_victimStates[targetA][targetB].push_back(game::victim_state::Normal);
				float hitTableRoll = hitTableDistribution(randomGenerator);
				if ((hitTableRoll -= targetUnit->getMissChance(*attacker, school)) < 0.0f)
				{
					m_hitInfos[targetA][targetB].back() = game::hit_info::Miss;
				}
				else if (targetUnit->isImmune(school))
				{
					m_victimStates[targetA][targetB].back() = game::victim_state::IsImmune;
				}
				else if ((hitTableRoll -= targetUnit->getCritChance(*attacker, school)) < 0.0f)
				{
					m_hitInfos[targetA][targetB].back() = game::hit_info::CriticalHit;
				}

				m_resists[targetA][targetB].push_back(targetUnit->getResiPercentage(effect, *attacker));
			}
		}
		
		targets = m_targets[targetA][targetB];
		victimStates = m_victimStates[targetA][targetB];
		hitInfos = m_hitInfos[targetA][targetB];
		resists = m_resists[targetA][targetB];
	}
	
	void AttackTable::checkNonBinarySpellNoCrit(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
	{
		UInt32 targetA = effect.targeta();
		UInt32 targetB = effect.targetb();
		if (!checkIndex(targetA, targetB))
		{
			UInt8 school = spell.schoolmask();
			refreshTargets(*attacker, targetMap, targetA, targetB, effect.radius(), spell.maxtargets());
			std::uniform_real_distribution<float> hitTableDistribution(0.0f, 99.9f);

			for (GameUnit* targetUnit : m_targets[targetA][targetB])
			{
				m_hitInfos[targetA][targetB].push_back(game::hit_info::NoAction);
				m_victimStates[targetA][targetB].push_back(game::victim_state::Normal);
				float hitTableRoll = hitTableDistribution(randomGenerator);
				if ((hitTableRoll -= targetUnit->getMissChance(*attacker, school)) < 0.0f)
				{
					m_hitInfos[targetA][targetB].back() = game::hit_info::Miss;
				}
				else if (targetUnit->isImmune(school))
				{
					m_victimStates[targetA][targetB].back() = game::victim_state::IsImmune;
				}

				m_resists[targetA][targetB].push_back(targetUnit->getResiPercentage(effect, *attacker));
			}
		}
		
		targets = m_targets[targetA][targetB];
		victimStates = m_victimStates[targetA][targetB];
		hitInfos = m_hitInfos[targetA][targetB];
		resists = m_resists[targetA][targetB];
	}
	
	void AttackTable::refreshTargets(GameUnit &attacker, SpellTargetMap &targetMap, UInt32 targetA, UInt32 targetB, float radius, UInt32 maxtargets)
	{
		std::vector<GameUnit*> targets;
		GameUnit *unitTarget = nullptr;
		WorldInstance *world = attacker.getWorldInstance();

		if (targetMap.getTargetMap() == game::spell_cast_target_flags::Self || targetA == game::targets::UnitCaster)
			unitTarget = &attacker;
		else if (world && targetMap.hasUnitTarget())
		{
			unitTarget = dynamic_cast<GameUnit*>(world->findObjectByGUID(targetMap.getUnitTarget()));
		}
		
		float x, y, tmp;
		switch (targetA)
		{
		case game::targets::UnitCaster:
		case game::targets::UnitTargetAlly:
		case game::targets::UnitTargetEnemy:
			if (unitTarget) targets.push_back(unitTarget);
			break;
		case game::targets::UnitPartyCaster:
			{
				attacker.getLocation(x, y, tmp, tmp);
				auto &finder = attacker.getWorldInstance()->getUnitFinder();
				finder.findUnits(Circle(x, y, radius), [this, &attacker, &targets, maxtargets](GameUnit &unit) -> bool
				{
					if (unit.getTypeId() != object_type::Character)
						return true;

					GameCharacter *unitChar = dynamic_cast<GameCharacter*>(&unit);
					GameCharacter *attackerChar = dynamic_cast<GameCharacter*>(&attacker);
					if (unitChar == attackerChar ||
						(unitChar->getGroupId() != 0 &&
						unitChar->getGroupId() == attackerChar->getGroupId()))
					{
						targets.push_back(&unit);
						if (maxtargets > 0 &&
							targets.size() >= maxtargets)
						{
							// No more units
							return false;
						}
					}

					return true;
				});
			}
			break;
		case game::targets::UnitAreaEnemyDst:
			{
				targetMap.getDestLocation(x, y, tmp);
				auto &finder = attacker.getWorldInstance()->getUnitFinder();
				finder.findUnits(Circle(x, y, radius), [this, &attacker, &targets, maxtargets](GameUnit &unit) -> bool
				{
					const auto &factionA = unit.getFactionTemplate();
					const auto &factionB = attacker.getFactionTemplate();
					if (!isFriendlyTo(factionA, factionB) && unit.isAlive())
					{
						targets.push_back(&unit);
						if (maxtargets > 0 &&
							targets.size() >= maxtargets)
						{
							// No more units
							return false;
						}
					}

					return true;
				});
			}
			break;
		case game::targets::UnitPartyTarget:
			{
				if (unitTarget)
				{
					unitTarget->getLocation(x, y, tmp, tmp);
					auto &finder = attacker.getWorldInstance()->getUnitFinder();
					finder.findUnits(Circle(x, y, radius), [this, unitTarget, &targets, maxtargets](GameUnit &unit) -> bool
					{
						if (unit.getTypeId() != object_type::Character)
							return true;

						GameCharacter *unitChar = dynamic_cast<GameCharacter*>(&unit);
						GameCharacter *targetChar = dynamic_cast<GameCharacter*>(unitTarget);
						if (unitChar == targetChar ||
							(unitChar->getGroupId() != 0 &&
							unitChar->getGroupId() == targetChar->getGroupId()))
						{
							targets.push_back(&unit);
							if (maxtargets > 0 &&
								targets.size() >= maxtargets)
							{
								// No more units
								return false;
							}
						}

						return true;
					});
				}
			}
			break;
		default:
			break;
		}
		
		switch (targetB)
		{
		case game::targets::UnitAreaEnemySrc:
			{
				attacker.getLocation(x, y, tmp, tmp);
				auto &finder = attacker.getWorldInstance()->getUnitFinder();
				finder.findUnits(Circle(x, y, radius), [this, &attacker, &targets, maxtargets](GameUnit &unit) -> bool
				{
					const auto &factionA = unit.getFactionTemplate();
					const auto &factionB = attacker.getFactionTemplate();
					if (!isFriendlyTo(factionA, factionB) && unit.isAlive())
					{
						targets.push_back(&unit);
						if (maxtargets > 0 &&
							targets.size() >= maxtargets)
						{
							// No more units
							return false;
						}
					}

					return true;
				});
			}
			break;
		default:
			break;
		}
		
		m_targets[targetA][targetB] = targets;
	}
	
	bool AttackTable::checkIndex(UInt32 targetA, UInt32 targetB)
	{
		std::unordered_map<UInt32,std::unordered_map<UInt32,std::vector<GameUnit*>>>::iterator ta = m_targets.find(targetA);
		if (ta != m_targets.end())
		{
			std::unordered_map<UInt32,std::vector<GameUnit*>>::iterator tb = (*ta).second.find(targetB);
			if (tb != (*ta).second.end()) {
				return true;
			}
		}
		return false;
	}
}
