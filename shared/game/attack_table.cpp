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
#include "game_character.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	AttackTable::AttackTable() {
	}
	
	void AttackTable::checkMeleeAutoAttack(GameUnit* attacker, GameUnit *target, UInt8 school, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
	{
		UInt32 targetA = game::targets::UnitTargetEnemy;
		UInt32 targetB = game::targets::None;
		if (!checkIndex(targetA, targetB))
		{
			SpellTargetMap targetMap;
			targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
			targetMap.m_unitTarget = target->getGuid();
			refreshTargets(*attacker, targetMap, targetA, targetB, 0.0f, 0.0f);
			std::uniform_real_distribution<float> hitTableDistribution(0.0f, 99.9f);

			for (GameUnit* targetUnit : m_targets[targetA][targetB])
			{
				// Check if we are in front of the target for parry
				const bool targetLookingAtUs = targetUnit->isInArc(2.0f * 3.1415927f / 3.0f, attacker->getLocation().x, attacker->getLocation().y);
				game::HitInfo hitInfo = game::hit_info::NormalSwing;
				game::VictimState victimState = game::victim_state::Normal;
				float attackTableRoll = hitTableDistribution(randomGenerator);
				if ((attackTableRoll -= targetUnit->getMissChance(*attacker, school, true)) < 0.0f)
				{
					hitInfo = game::hit_info::Miss;
				}
				else if (targetUnit->isImmune(school))
				{
					victimState = game::victim_state::IsImmune;
				}
				else if ((targetLookingAtUs || (targetUnit->getTypeId() != object_type::Character)) && (attackTableRoll -= targetUnit->getDodgeChance(*attacker)) < 0.0f)
				{
					victimState = game::victim_state::Dodge;
				}
				else if (targetLookingAtUs && targetUnit->canParry() && (attackTableRoll -= targetUnit->getParryChance(*attacker)) < 0.0f)
				{
					victimState = game::victim_state::Parry;
				}
				else if ((attackTableRoll -= targetUnit->getGlancingChance(*attacker)) < 0.0f)
				{
					hitInfo = game::hit_info::Glancing;
				}
				else if (targetLookingAtUs && targetUnit->canBlock() && (attackTableRoll -= targetUnit->getBlockChance()) < 0.0f)
				{
					victimState = game::victim_state::Blocks;
				}
				else if ((attackTableRoll -= targetUnit->getCritChance(*attacker, school)) < 0.0f)
				{
					hitInfo = game::hit_info::CriticalHit;
				}
				else if ((attackTableRoll -= targetUnit->getCrushChance(*attacker)) < 0.0f)
				{
					hitInfo = game::hit_info::Crushing;
				}

//				m_resists[targetA][targetB].push_back(targetUnit->getResiPercentage(effect, *attacker));
				m_resists[targetA][targetB].push_back(0.0f);
				m_hitInfos[targetA][targetB].push_back(hitInfo);
				m_victimStates[targetA][targetB].push_back(victimState);
				
				attacker->doneMeleeAttack(targetUnit, victimState);
				targetUnit->takenMeleeAttack(attacker, victimState);
			}
		}
		
		targets = m_targets[targetA][targetB];
		victimStates = m_victimStates[targetA][targetB];
		hitInfos = m_hitInfos[targetA][targetB];
		resists = m_resists[targetA][targetB];
	}
	
	void AttackTable::checkSpecialMeleeAttack(GameUnit* attacker, SpellTargetMap &targetMap, UInt8 school, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
	{
		UInt32 targetA = game::targets::UnitTargetEnemy;
		UInt32 targetB = game::targets::None;
		if (!checkIndex(targetA, targetB))
		{
			refreshTargets(*attacker, targetMap, targetA, targetB, 0.0f, 0.0f);
			std::uniform_real_distribution<float> hitTableDistribution(0.0f, 99.9f);

			for (GameUnit* targetUnit : m_targets[targetA][targetB])
			{
				const bool targetLookingAtUs = targetUnit->isInArc(2.0f * 3.1415927f / 3.0f, attacker->getLocation().x, attacker->getLocation().y);
				game::HitInfo hitInfo = game::hit_info::NormalSwing;
				game::VictimState victimState = game::victim_state::Normal;
				float attackTableRoll = hitTableDistribution(randomGenerator);
				if ((attackTableRoll -= targetUnit->getMissChance(*attacker, school, false)) < 0.0f)
				{
					hitInfo = game::hit_info::Miss;
				}
				else if (targetUnit->isImmune(school))
				{
					victimState = game::victim_state::IsImmune;
				}
				else if ((targetLookingAtUs || targetUnit->getTypeId() != object_type::Character) && (attackTableRoll -= targetUnit->getDodgeChance(*attacker)) < 0.0f)
				{
					victimState = game::victim_state::Dodge;
				}
				else if (targetLookingAtUs && targetUnit->canParry() && (attackTableRoll -= targetUnit->getParryChance(*attacker)) < 0.0f)
				{
					victimState = game::victim_state::Parry;
				}
				else if (targetLookingAtUs && targetUnit->canBlock() && (attackTableRoll -= targetUnit->getBlockChance()) < 0.0f)
				{
					victimState = game::victim_state::Blocks;
				}
				else
				{
					if (attacker->getTypeId() == wowpp::object_type::Character)
					{
						attackTableRoll = hitTableDistribution(randomGenerator);
						if ((attackTableRoll -= targetUnit->getCritChance(*attacker, school)) < 0.0f)
						{
							hitInfo = game::hit_info::CriticalHit;
						}
					}
					else
					{
						if ((attackTableRoll -= targetUnit->getCritChance(*attacker, school)) < 0.0f)
						{
							hitInfo = game::hit_info::CriticalHit;
						}
						else if ((attackTableRoll -= targetUnit->getCrushChance(*attacker)) < 0.0f)
						{
							hitInfo = game::hit_info::Crushing;
						}
					}
				}

//				m_resists[targetA][targetB].push_back(targetUnit->getResiPercentage(effect, *attacker));
				m_resists[targetA][targetB].push_back(0.0f);
				m_hitInfos[targetA][targetB].push_back(hitInfo);
				m_victimStates[targetA][targetB].push_back(victimState);
		
				attacker->doneMeleeAttack(targetUnit, victimState);
				targetUnit->takenMeleeAttack(attacker, victimState);
			}
		}
		
		targets = m_targets[targetA][targetB];
		victimStates = m_victimStates[targetA][targetB];
		hitInfos = m_hitInfos[targetA][targetB];
		resists = m_resists[targetA][targetB];
	}
	
	void AttackTable::checkSpecialMeleeAttackNoGlanceCritCrush(GameUnit* attacker, SpellTargetMap &targetMap, UInt8 school, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
	{
		UInt32 targetA = game::targets::UnitTargetEnemy;
		UInt32 targetB = game::targets::None;
		if (!checkIndex(targetA, targetB))
		{
			refreshTargets(*attacker, targetMap, targetA, targetB, 0.0f, 0.0f);
			std::uniform_real_distribution<float> hitTableDistribution(0.0f, 99.9f);

			for (GameUnit* targetUnit : m_targets[targetA][targetB])
			{
				const bool targetLookingAtUs = targetUnit->isInArc(2.0f * 3.1415927f / 3.0f, attacker->getLocation().x, attacker->getLocation().y);
				game::HitInfo hitInfo = game::hit_info::NormalSwing;
				game::VictimState victimState = game::victim_state::Normal;
				float attackTableRoll = hitTableDistribution(randomGenerator);
				if ((attackTableRoll -= targetUnit->getMissChance(*attacker, school, false)) < 0.0f)
				{
					hitInfo = game::hit_info::Miss;
				}
				else if (targetUnit->isImmune(school))
				{
					victimState = game::victim_state::IsImmune;
				}
				else if ((targetLookingAtUs || targetUnit->getTypeId() != object_type::Character) && (attackTableRoll -= targetUnit->getDodgeChance(*attacker)) < 0.0f)
				{
					victimState = game::victim_state::Dodge;
				}
				else if (targetLookingAtUs && targetUnit->canParry() && (attackTableRoll -= targetUnit->getParryChance(*attacker)) < 0.0f)
				{
					victimState = game::victim_state::Parry;
				}
				else if (targetLookingAtUs && targetUnit->canBlock() && (attackTableRoll -= targetUnit->getBlockChance()) < 0.0f)
				{
					victimState = game::victim_state::Blocks;
				}

//				m_resists[targetA][targetB].push_back(targetUnit->getResiPercentage(effect, *attacker));
				m_resists[targetA][targetB].push_back(0.0f);
				m_hitInfos[targetA][targetB].push_back(hitInfo);
				m_victimStates[targetA][targetB].push_back(victimState);
		
				attacker->doneMeleeAttack(targetUnit, victimState);
				targetUnit->takenMeleeAttack(attacker, victimState);
			}
		}
		
		targets = m_targets[targetA][targetB];
		victimStates = m_victimStates[targetA][targetB];
		hitInfos = m_hitInfos[targetA][targetB];
		resists = m_resists[targetA][targetB];
	}
	
	void AttackTable::checkRangedAttack(GameUnit* attacker, SpellTargetMap &targetMap, UInt8 school, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
	{
		
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
				game::HitInfo hitInfo = game::hit_info::NoAction;
				game::VictimState victimState = game::victim_state::Unknown1;
				float attackTableRoll = hitTableDistribution(randomGenerator);
				if (targetUnit->isImmune(school))
				{
					victimState = game::victim_state::IsImmune;
				}
				else if ((attackTableRoll -= targetUnit->getCritChance(*attacker, school)) < 0.0f)
				{
					hitInfo = game::hit_info::CriticalHit;
				}
				m_resists[targetA][targetB].push_back(0.0f);
				m_hitInfos[targetA][targetB].push_back(hitInfo);
				m_victimStates[targetA][targetB].push_back(victimState);
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
				game::HitInfo hitInfo = game::hit_info::NoAction;
				game::VictimState victimState = game::victim_state::Unknown1;
				if (targetUnit->isImmune(school))
				{
					victimState = game::victim_state::IsImmune;
				}
				m_resists[targetA][targetB].push_back(0.0f);
				m_hitInfos[targetA][targetB].push_back(hitInfo);
				m_victimStates[targetA][targetB].push_back(victimState);
			}
		}
		
		targets = m_targets[targetA][targetB];
		victimStates = m_victimStates[targetA][targetB];
		hitInfos = m_hitInfos[targetA][targetB];
		resists = m_resists[targetA][targetB];
	}
	
	void AttackTable::checkSpell(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
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
				game::HitInfo hitInfo = game::hit_info::NoAction;
				game::VictimState victimState = game::victim_state::Normal;
				float attackTableRoll = hitTableDistribution(randomGenerator);
				if ((attackTableRoll -= targetUnit->getMissChance(*attacker, school, false)) < 0.0f)
				{
					hitInfo = game::hit_info::Miss;
				}
				else if (targetUnit->isImmune(school))
				{
					victimState = game::victim_state::IsImmune;
				}
				else if ((attackTableRoll -= targetUnit->getCritChance(*attacker, school)) < 0.0f)
				{
					hitInfo = game::hit_info::CriticalHit;
				}

				m_resists[targetA][targetB].push_back(targetUnit->getResiPercentage(effect, *attacker));
				m_hitInfos[targetA][targetB].push_back(hitInfo);
				m_victimStates[targetA][targetB].push_back(victimState);
			}
		}
		
		targets = m_targets[targetA][targetB];
		victimStates = m_victimStates[targetA][targetB];
		hitInfos = m_hitInfos[targetA][targetB];
		resists = m_resists[targetA][targetB];
	}
	
	void AttackTable::checkSpellNoCrit(GameUnit* attacker, SpellTargetMap &targetMap, const proto::SpellEntry &spell, const proto::SpellEffect &effect, std::vector<GameUnit*> &targets, std::vector<game::VictimState> &victimStates, std::vector<game::HitInfo> &hitInfos, std::vector<float> &resists)
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
				game::HitInfo hitInfo = game::hit_info::NoAction;
				game::VictimState victimState = game::victim_state::Normal;
				float attackTableRoll = hitTableDistribution(randomGenerator);
				if ((attackTableRoll -= targetUnit->getMissChance(*attacker, school, false)) < 0.0f)
				{
					hitInfo = game::hit_info::Miss;
				}
				else if (targetUnit->isImmune(school))
				{
					victimState = game::victim_state::IsImmune;
				}

				m_resists[targetA][targetB].push_back(targetUnit->getResiPercentage(effect, *attacker));
				m_hitInfos[targetA][targetB].push_back(hitInfo);
				m_victimStates[targetA][targetB].push_back(victimState);
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
		{
			unitTarget = &attacker;
		}
		else if (world && targetMap.hasUnitTarget())
		{
			unitTarget = dynamic_cast<GameUnit*>(world->findObjectByGUID(targetMap.getUnitTarget()));
		}
		
		float x, y, tmp;
		switch (targetA)
		{
		case game::targets::UnitCaster:			//1
		case game::targets::UnitTargetEnemy:	//6
		case game::targets::UnitTargetAlly:		//21
		case game::targets::UnitTargetAny:		//25
			if (unitTarget) targets.push_back(unitTarget);
			break;
		case game::targets::UnitPartyCaster:
			{
				math::Vector3 location = attacker.getLocation();
				auto &finder = attacker.getWorldInstance()->getUnitFinder();
				finder.findUnits(Circle(location.x, location.y, radius), [this, &attacker, &targets, maxtargets](GameUnit &unit) -> bool
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
					math::Vector3 location = attacker.getLocation();
					auto &finder = attacker.getWorldInstance()->getUnitFinder();
					finder.findUnits(Circle(location.x, location.y, radius), [this, unitTarget, &targets, maxtargets](GameUnit &unit) -> bool
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
				math::Vector3 location = attacker.getLocation();
				auto &finder = attacker.getWorldInstance()->getUnitFinder();
				finder.findUnits(Circle(location.x, location.y, radius), [this, &attacker, &targets, maxtargets](GameUnit &unit) -> bool
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
