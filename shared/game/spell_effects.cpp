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

#include "pch.h"
#include "single_cast_state.h"
#include "game_character.h"
#include "game_creature.h"
#include "game_item.h"
#include "inventory.h"
#include "spell_cast.h"
#include "world_instance.h"
#include "each_tile_in_sight.h"
#include "universe.h"
#include "unit_mover.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	static UInt32 getLockId(const proto::ObjectEntry &entry)
	{
		UInt32 lockId = 0;

		switch (entry.type())
		{
			case 0:
			case 1:
				lockId = entry.data(1);
				break;
			case 2:
			case 3:
			case 6:
			case 10:
			case 12:
			case 13:
			case 24:
			case 26:
				lockId = entry.data(0);
				break;
			case 25:
				lockId = entry.data(4);
				break;
		}

		return lockId;
	}

	void SingleCastState::spellEffectAddComboPoints(const proto::SpellEffect &effect)
	{
		GameCharacter *character = nullptr;
		if (isPlayerGUID(m_cast.getExecuter().getGuid()))
		{
			character = dynamic_cast<GameCharacter *>(&m_cast.getExecuter());
		}

		if (!character)
		{
			ELOG("Invalid character");
			return;
		}

		m_affectedTargets.insert(character->shared_from_this());

		UInt64 comboTarget = m_target.getUnitTarget();
		character->addComboPoints(comboTarget, UInt8(calculateEffectBasePoints(effect)));
	}

	void SingleCastState::spellEffectDuel(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		if (!caster.isGameCharacter())
		{
			// This spell effect is only usable by player characters right now
			ELOG("Caster is not a game character!");
			return;
		}

		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		// Did we find at least one valid target?
		if (targets.empty())
		{
			WLOG("No targets found");
			return;
		}

		// Get the first target
		GameUnit *targetUnit = targets.front();
		ASSERT(targetUnit);

		// Check if target is a character
		if (!targetUnit->isGameCharacter())
		{
			// Skip this target
			WLOG("Target is not a character");
			return;
		}

		// Check if target is already dueling
		if (targetUnit->getUInt64Value(character_fields::DuelArbiter))
		{
			// Target is already dueling
			WLOG("Target is already dueling");
			return;
		}

		// Target is dead
		if (!targetUnit->isAlive())
		{
			WLOG("Target is dead");
			return;
		}

		// We affect this target
		m_affectedTargets.insert(targetUnit->shared_from_this());

		// Find flag object entry
		auto &project = targetUnit->getProject();
		const auto *entry = project.objects.getById(effect.miscvaluea());
		if (!entry)
		{
			ELOG("Could not find duel arbiter object: " << effect.miscvaluea());
			return;
		}

		// Spawn new duel arbiter flag
		if (auto *world = caster.getWorldInstance())
		{
			auto flagObject = world->spawnWorldObject(
				*entry,
				caster.getLocation(),
				0.0f,
				0.0f
			);
			flagObject->setUInt32Value(world_object_fields::AnimProgress, 0);
			flagObject->setUInt32Value(world_object_fields::Level, caster.getLevel());
			flagObject->setUInt32Value(world_object_fields::Faction, caster.getFactionTemplate().id());
			world->addGameObject(*flagObject);

			// Save this object to prevent it from being deleted immediatly
			caster.addWorldObject(flagObject);

			// Save them
			caster.setUInt64Value(character_fields::DuelArbiter, flagObject->getGuid());
			targetUnit->setUInt64Value(character_fields::DuelArbiter, flagObject->getGuid());
			DLOG("Duel arbiter spawned: " << flagObject->getGuid());
		}
	}

	void SingleCastState::spellEffectWeaponDamageNoSchool(const proto::SpellEffect &effect)
	{
		//TODO: Implement
		meleeSpecialAttack(effect, false);
	}

	void SingleCastState::spellEffectCreateItem(const proto::SpellEffect &effect)
	{
		// Get item entry
		const auto *item = m_cast.getExecuter().getProject().items.getById(effect.itemtype());
		if (!item)
		{
			ELOG("SPELL_EFFECT_CREATE_ITEM: Could not find item by id " << effect.itemtype());
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		bool wasCreated = false;

		m_attackTable.checkPositiveSpellNoCrit(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);
		const auto itemCount = calculateEffectBasePoints(effect);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				auto *charUnit = reinterpret_cast<GameCharacter *>(targetUnit);
				auto &inv = charUnit->getInventory();

				std::map<UInt16, UInt16> addedBySlot;
				auto result = inv.createItems(*item, itemCount, &addedBySlot);
				if (result != game::inventory_change_failure::Okay)
				{
					charUnit->inventoryChangeFailure(result, nullptr, nullptr);
					continue;
				}

				wasCreated = true;

				// Send item notification
				for (auto &slot : addedBySlot)
				{
					auto inst = inv.getItemAtSlot(slot.first);
					if (inst)
					{
						UInt8 bag = 0, subslot = 0;
						Inventory::getRelativeSlots(slot.first, bag, subslot);
						const auto totalCount = inv.getItemCount(item->id());

						TileIndex2D tile;
						if (charUnit->getTileIndex(tile))
						{
							std::vector<char> buffer;
							io::VectorSink sink(buffer);
							game::Protocol::OutgoingPacket itemPacket(sink);
							game::server_write::itemPushResult(itemPacket, charUnit->getGuid(), std::cref(*inst), false, true, bag, subslot, slot.second, totalCount);
							forEachSubscriberInSight(
								charUnit->getWorldInstance()->getGrid(),
								tile,
								[&](ITileSubscriber & subscriber)
							{
								auto subscriberGroup = subscriber.getControlledObject()->getGroupId();
								if ((charUnit->getGroupId() == 0 && subscriber.getControlledObject()->getGuid() == charUnit->getGuid()) ||
									(charUnit->getGroupId() != 0 && subscriberGroup == charUnit->getGroupId())
									)
								{
									subscriber.sendPacket(itemPacket, buffer);
								}
							});
						}
					}
				}
			}
		}

		// Increase crafting skill eventually
		if (wasCreated && caster.isGameCharacter())
		{
			GameCharacter &casterChar = reinterpret_cast<GameCharacter&>(caster);
			if (m_spell.skill() != 0)
			{
				// Increase skill point if possible
				UInt16 current = 0, max = 0;
				if (casterChar.getSkillValue(m_spell.skill(), current, max))
				{
					const UInt32 yellowLevel = m_spell.trivialskilllow();
					const UInt32 greenLevel = m_spell.trivialskilllow() + (m_spell.trivialskillhigh() - m_spell.trivialskilllow()) / 2;
					const UInt32 grayLevel = m_spell.trivialskillhigh();

					// Determine current rank
					UInt32 successChance = 0;
					if (current < yellowLevel)
					{
						// Orange
						successChance = 100;
					}
					else if (current < greenLevel)
					{
						// Yellow
						successChance = 75;
					}
					else if (current < grayLevel)
					{
						// Green
						successChance = 25;
					}

					// Do we need to roll?
					if (successChance > 0 && successChance != 100)
					{
						// Roll
						std::uniform_int_distribution<UInt32> roll(0, 100);
						UInt32 val = roll(randomGenerator);
						if (val >= successChance)
						{
							// No luck - exit here
							return;
						}
					}
					else if (successChance == 0)
					{
						return;
					}

					// Increase spell value
					current = std::min<UInt16>(max, current + 1);
					casterChar.setSkillValue(m_spell.skill(), current, max);
				}
			}
		}
	}

	void SingleCastState::spellEffectWeaponDamage(const proto::SpellEffect &effect)
	{
		//TODO: Implement
		meleeSpecialAttack(effect, false);
	}

	void SingleCastState::spellEffectApplyAura(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		bool isPositive = Aura::isPositive(m_spell, effect);
		UInt8 school = m_spell.schoolmask();

		if (isPositive)
		{
			m_attackTable.checkPositiveSpellNoCrit(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);	//Buff
		}
		else
		{
			m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);	//Debuff
		}

		UInt32 aura = effect.aura();
		bool modifiedByBonus;
		switch (aura)
		{
			case game::aura_type::PeriodicDamage:
			case game::aura_type::PeriodicHeal:
			case game::aura_type::PeriodicLeech:
				modifiedByBonus = true;
			default:
				modifiedByBonus = false;
		}

		// World was already checked. If world would be nullptr, unitTarget would be null as well
		auto *world = m_cast.getExecuter().getWorldInstance();
		auto &universe = world->getUniverse();
		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 totalPoints = 0;
			game::SpellMissInfo missInfo = game::spell_miss_info::None;
			bool spellFailed = false;

			if (hitInfos[i] == game::hit_info::Miss)
			{
				missInfo = game::spell_miss_info::Miss;
			}
			else if (victimStates[i] == game::victim_state::Evades)
			{
				missInfo = game::spell_miss_info::Evade;
			}
			else if (victimStates[i] == game::victim_state::IsImmune)
			{
				missInfo = game::spell_miss_info::Immune;
			}
			else if (victimStates[i] == game::victim_state::Normal)
			{
				if (resists[i] == 100.0f)
				{
					missInfo = game::spell_miss_info::Resist;
				}
				else
				{
					if (modifiedByBonus)
					{
						UInt32 spellPower = caster.getBonus(school);
						UInt32 spellBonusPct = caster.getBonusPct(school);
						totalPoints = getSpellPointsTotal(effect, spellPower, spellBonusPct);
						totalPoints -= totalPoints * (resists[i] / 100.0f);
					}
					else
					{
						totalPoints = calculateEffectBasePoints(effect);
					}

					if (effect.aura() == game::aura_type::PeriodicDamage &&
						m_spell.attributes(4) & game::spell_attributes_ex_d::StackDotModifier)
					{
						targetUnit->getAuras().forEachAuraOfType(game::aura_type::PeriodicDamage, [&totalPoints, this](Aura &aura) -> bool
						{
							if (aura.getSpell().id() == m_spell.id())
							{
								Int32 remainingTicks = aura.getMaxTickCount() - aura.getTickCount();
								Int32 remainingDamage = aura.getBasePoints() * remainingTicks;

								totalPoints += remainingDamage / aura.getMaxTickCount();
							}

							return true;
						});
					}
				}
			}

			if (missInfo != game::spell_miss_info::None)
			{
				m_completedEffectsExecution[targetUnit->getGuid()] = completedEffects.connect([this, &caster, targetUnit, school, missInfo]()
				{
					std::map<UInt64, game::SpellMissInfo> missedTargets;
					missedTargets[targetUnit->getGuid()] = missInfo;

					sendPacketFromCaster(caster,
						std::bind(game::server_write::spellLogMiss, std::placeholders::_1,
							m_spell.id(),
							caster.getGuid(),
							0,
							std::cref(missedTargets)));
				});	// End connect
			}
			else if (targetUnit->isAlive())
			{
				std::shared_ptr<Aura> aura = std::make_shared<Aura>(m_spell, effect, totalPoints, caster, *targetUnit, m_target, m_itemGuid, false, [&universe](std::function<void()> work)
				{
					universe.post(work);
				}, [&universe](Aura & self)
				{
					// Prevent aura from being deleted before being removed from the list
					auto strong = self.shared_from_this();
					universe.post([strong]()
					{
						strong->getTarget().getAuras().removeAura(*strong);
					});
				});

				// TODO: Dimishing return and custom durations

				// TODO: Apply spell haste

				// TODO: Check if aura already expired

				const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
				if (!noThreat)
				{
					targetUnit->threaten(caster, 0.0f);
				}

				// TODO: Add aura to unit target
				if (isChanneled())
				{
					m_onChannelAuraRemoved = aura->misapplied.connect([this]() {
						stopCast(game::spell_interrupt_flags::None);
					});
				}

				targetUnit->getAuras().addAura(std::move(aura));

				// We need to be sitting for this aura to work
				if (m_spell.aurainterruptflags() & game::spell_aura_interrupt_flags::NotSeated)
				{
					// Sit down
					caster.setStandState(unit_stand_state::Sit);
				}
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}

		// If auras should be removed on immunity, do so!
		if (aura == game::aura_type::MechanicImmunity &&
			(m_spell.attributes(1) & game::spell_attributes_ex_a::DispelAurasOnImmunity) != 0)
		{
			if (!m_removeAurasOnImmunity.connected())
			{
				m_removeAurasOnImmunity = completedEffects.connect([this] {
					UInt32 immunityMask = 0;
					for (Int32 i = 0; i < m_spell.effects_size(); ++i)
					{
						if (m_spell.effects(i).type() == game::spell_effects::ApplyAura &&
							m_spell.effects(i).aura() == game::aura_type::MechanicImmunity)
						{
							immunityMask |= (1 << m_spell.effects(i).miscvaluea());
						}
					}

					for (auto &target : m_affectedTargets)
					{
						auto strong = target.lock();
						if (strong)
						{
							auto unit = std::static_pointer_cast<GameUnit>(strong);
							unit->getAuras().removeAllAurasDueToMechanic(immunityMask);
						}
					}
				});
			}
		}
	}

	void SingleCastState::spellEffectPersistentAreaAura(const proto::SpellEffect & effect)
	{
		// Check targets
		GameUnit &caster = m_cast.getExecuter();
		if (!m_target.hasDestTarget())
		{
			WLOG("SPELL_EFFECT_APPLY_AREA_AURA: No dest target info found!");
			return;
		}

		math::Vector3 dstLoc;
		m_target.getDestLocation(dstLoc.x, dstLoc.y, dstLoc.z);

		static UInt64 lowGuid = 1;

		// Create a new dynamic object
		auto dynObj = std::make_shared<DynObject>(
			caster.getProject(),
			caster.getTimers(),
			caster,
			m_spell,
			effect
			);
		// TODO: Add lower guid counter
		auto guid = createEntryGUID(lowGuid++, m_spell.id(), guid_type::Player);
		dynObj->setGuid(guid);
		dynObj->relocate(dstLoc, 0.0f, false);
		dynObj->initialize();

		// Remember to destroy this object on end of channeling
		if (isChanneled())
		{
			m_dynObjectsToDespawn.push_back(guid);
		}
		else
		{
			// Timed despawn
			dynObj->triggerDespawnTimer(m_spell.duration());
		}

		if (effect.amplitude())
		{
			dynObj->startUnitWatcher();
			spellEffectApplyAura(effect);
		}

		// Add this object to the unit (this will also sawn it)
		caster.addDynamicObject(dynObj);

		if (m_hitResults.find(caster.getGuid()) == m_hitResults.end())
		{
			HitResult procInfo(m_attackerProc, m_victimProc, game::hit_info::NoAction, game::victim_state::Normal);
			m_hitResults.emplace(caster.getGuid(), procInfo);
		}
		else
		{
			HitResult &procInfo = m_hitResults.at(caster.getGuid());
			procInfo.add(game::hit_info::NoAction, game::victim_state::Normal);
		}
	}

	void SingleCastState::spellEffectHeal(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);		//Buff, HoT

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt8 school = m_spell.schoolmask();
			UInt32 totalPoints;
			bool crit = false;
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				totalPoints = 0;
			}
			else
			{
				UInt32 spellPower = caster.getBonus(school);
				UInt32 spellBonusPct = caster.getBonusPct(school);
				totalPoints = getSpellPointsTotal(effect, spellPower, spellBonusPct);

				caster.applyHealingDoneBonus(m_spell.spelllevel(), caster.getLevel(), 1, totalPoints);
				targetUnit->applyHealingTakenBonus(1, totalPoints);

				if (hitInfos[i] == game::hit_info::CriticalHit)
				{
					crit = true;
					totalPoints *= 2.0f;
				}
			}

			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			if (targetUnit->heal(totalPoints, &caster, noThreat))
			{
				// Send spell heal packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellHealLog, std::placeholders::_1,
						targetUnit->getGuid(),
						caster.getGuid(),
						m_spell.id(),
						totalPoints,
						crit));
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i], totalPoints);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i], totalPoints);
			}
		}
	}

	void SingleCastState::spellEffectBind(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;

		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				GameCharacter *character = dynamic_cast<GameCharacter *>(targetUnit);
				character->setHome(caster.getMapId(), caster.getLocation(), caster.getOrientation());
			}
		}
	}

	void SingleCastState::spellEffectQuestComplete(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;

		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			if (targetUnit->isGameCharacter())
			{
				reinterpret_cast<GameCharacter*>(targetUnit)->completeQuest(effect.miscvaluea());
			}
		}
	}

	void SingleCastState::spellEffectTriggerSpell(const proto::SpellEffect &effect)
	{
		if (!effect.triggerspell())
		{
			WLOG("Spell " << m_spell.id() << ": No spell to trigger found! Trigger effect will be ignored.");
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		caster.castSpell(m_target, effect.triggerspell(), { 0, 0, 0 }, 0, true);
	}

	void SingleCastState::spellEffectEnergize(const proto::SpellEffect &effect)
	{
		Int32 powerType = effect.miscvaluea();
		if (powerType < 0 || powerType > 5) {
			return;
		}

		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkPositiveSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);		//Buff, HoT

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 power = calculateEffectBasePoints(effect);
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				power = 0;
			}

			UInt32 curPower = targetUnit->getUInt32Value(unit_fields::Power1 + powerType);
			UInt32 maxPower = targetUnit->getUInt32Value(unit_fields::MaxPower1 + powerType);
			if (curPower + power > maxPower)
			{
				curPower = maxPower;
			}
			else
			{
				curPower += power;
			}
			targetUnit->setUInt32Value(unit_fields::Power1 + powerType, curPower);
			sendPacketFromCaster(m_cast.getExecuter(),
				std::bind(game::server_write::spellEnergizeLog, std::placeholders::_1,
					m_cast.getExecuter().getGuid(), targetUnit->getGuid(), m_spell.id(), static_cast<UInt8>(powerType), power));

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectPowerBurn(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt8 school = m_spell.schoolmask();
			Int32 powerType = effect.miscvaluea();
			UInt32 burn;
			UInt32 damage = 0;
			UInt32 resisted = 0;
			UInt32 absorbed = 0;
			if (victimStates[i] == game::victim_state::IsImmune)
			{
				burn = 0;
			}
			if (hitInfos[i] == game::hit_info::Miss)
			{
				burn = 0;
			}
			else
			{
				burn = calculateEffectBasePoints(effect);
				resisted = burn * (resists[i] / 100.0f);
				burn -= resisted;
				burn = 0 - targetUnit->addPower(game::PowerType(powerType), 0 - burn);
				damage = burn * effect.multiplevalue();
				absorbed = targetUnit->consumeAbsorb(damage, school);
			}

			// Update health value
			const bool noThreat = ((m_spell.attributes(1) & game::spell_attributes_ex_a::NoThreat) != 0);
			float threat = noThreat ? 0.0f : damage - absorbed;
			if (!noThreat && m_cast.getExecuter().isGameCharacter())
			{
				reinterpret_cast<GameCharacter&>(m_cast.getExecuter()).applySpellMod(spell_mod_op::Threat, m_spell.id(), threat);
			}
			if (targetUnit->dealDamage(damage - absorbed, school, game::DamageType::Direct, &caster, threat))
			{
				// Send spell damage packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
						targetUnit->getGuid(),
						caster.getGuid(),
						m_spell.id(),
						damage,
						school,
						absorbed,
						resisted,
						false,
						0,
						false));	//crit
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i], damage, absorbed, damage ? true : false);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i], damage, absorbed, damage ? true : false);
			}
		}
	}


	void SingleCastState::spellEffectWeaponPercentDamage(const proto::SpellEffect &effect)
	{
		meleeSpecialAttack(effect, true);
	}

	void SingleCastState::spellEffectOpenLock(const proto::SpellEffect &effect)
	{
		// Try to get the target
		WorldObject *obj = nullptr;
		if (!m_target.hasGOTarget())
		{
			DLOG("TODO: SPELL_EFFECT_OPEN_LOCK without GO target");
			return;
		}

		auto *world = m_cast.getExecuter().getWorldInstance();
		if (!world)
		{
			return;
		}

		GameObject *raw = world->findObjectByGUID(m_target.getGOTarget());
		if (!raw)
		{
			WLOG("SPELL_EFFECT_OPEN_LOCK: Could not find target object");
			return;
		}

		if (!raw->isWorldObject())
		{
			WLOG("SPELL_EFFECT_OPEN_LOCK: Target object is not a world object");
			return;
		}

		obj = reinterpret_cast<WorldObject*>(raw);
		m_affectedTargets.insert(obj->shared_from_this());

		UInt32 currentState = obj->getUInt32Value(world_object_fields::State);

		const auto &entry = obj->getEntry();
		UInt32 lockId = getLockId(entry);
		DLOG("Lock id: " << lockId);

		// TODO: Get lock info

		// We need to consume eventual cast items NOW before we send the loot packet to the client
		// If we remove the item afterwards, the client will reject the loot dialog and request a 
		// close immediatly. Don't worry, consumeItem may be called multiple times: It will only 
		// remove the item once
		if (!consumeItem(false))
		{
			return;
		}

		// If it is a door, try to open it
		switch (entry.type())
		{
			case world_object_type::Door:
			case world_object_type::Button:
				obj->setUInt32Value(world_object_fields::State, (currentState == 1 ? 0 : 1));
				break;
			case world_object_type::Chest:
			{
				obj->setUInt32Value(world_object_fields::State, (currentState == 1 ? 0 : 1));

				// Open chest loot window
				auto loot = obj->getObjectLoot();
				if (loot && !loot->isEmpty())
				{
					// Start inspecting the loot
					if (m_cast.getExecuter().isGameCharacter())
					{
						GameCharacter *character = reinterpret_cast<GameCharacter *>(&m_cast.getExecuter());
						character->lootinspect(loot);
					}
				}
				break;
			}
			default:	// Make the compiler happy
				break;
		}

		if (m_cast.getExecuter().isGameCharacter())
		{
			GameCharacter *character = reinterpret_cast<GameCharacter *>(&m_cast.getExecuter());
			character->onQuestObjectCredit(m_spell.id(), *obj);
			character->objectInteraction(*obj);
		}

		// Raise interaction triggers
		obj->raiseTrigger(trigger_event::OnInteraction);
	}

	void SingleCastState::spellEffectApplyAreaAuraParty(const proto::SpellEffect &effect)
	{

	}

	void SingleCastState::spellEffectDispel(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		UInt8 school = m_spell.schoolmask();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			UInt32 totalPoints = 0;
			bool spellFailed = false;

			if (hitInfos[i] == game::hit_info::Miss)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::IsImmune)
			{
				spellFailed = true;
			}
			else if (victimStates[i] == game::victim_state::Normal)
			{
				if (resists[i] == 100.0f)
				{
					spellFailed = true;
				}
				else
				{
					totalPoints = calculateEffectBasePoints(effect);
				}
			}

			if (spellFailed)
			{
				// TODO send fail packet
				sendPacketFromCaster(caster,
					std::bind(game::server_write::spellNonMeleeDamageLog, std::placeholders::_1,
						targetUnit->getGuid(),
						caster.getGuid(),
						m_spell.id(),
						1,
						school,
						0,
						1,	//resisted
						false,
						0,
						false));
			}
			else if (targetUnit->isAlive())
			{
				UInt32 auraDispelType = effect.miscvaluea();
				for (UInt32 i = 0; i < totalPoints; i++)
				{
					bool positive = caster.isHostileTo(*targetUnit);
					Aura *aura = targetUnit->getAuras().popBack(auraDispelType, positive);
					if (aura)
					{
						aura->misapplyAura();
					}
					else
					{
						break;
					}
				}
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectSummon(const proto::SpellEffect &effect)
	{
		const auto *entry = m_cast.getExecuter().getProject().units.getById(effect.summonunit());
		if (!entry)
		{
			WLOG("Can't summon anything - missing entry");
			return;
		}

		GameUnit &executer = m_cast.getExecuter();
		auto *world = executer.getWorldInstance();
		if (!world)
		{
			WLOG("Could not find world instance!");
			return;
		}

		float o = executer.getOrientation();
		math::Vector3 location(executer.getLocation());

		auto spawned = world->spawnSummonedCreature(*entry, location, o);
		if (!spawned)
		{
			ELOG("Could not spawn creature!");
			return;
		}

		spawned->setUInt64Value(unit_fields::SummonedBy, executer.getGuid());
		world->addGameObject(*spawned);

		if (executer.getVictim())
		{
			spawned->threaten(*executer.getVictim(), 0.0001f);
		}
	}

	void SingleCastState::spellEffectSummonPet(const proto::SpellEffect & effect)
	{
		GameUnit &executer = m_cast.getExecuter();

		// Check if caster already have a pet
		UInt64 petGUID = executer.getUInt64Value(unit_fields::Summon);
		if (petGUID != 0)
		{
			// Already have a pet!
			return;
		}

		// Get the pet unit entry
		const auto *entry = executer.getProject().units.getById(effect.miscvaluea());
		if (!entry)
		{
			WLOG("Can't summon pet - missing entry");
			return;
		}

		auto *world = executer.getWorldInstance();
		if (!world)
		{
			return;
		}

		float o = executer.getOrientation();
		math::Vector3 location(executer.getLocation());

		auto spawned = world->spawnSummonedCreature(*entry, location, o);
		if (!spawned)
		{
			ELOG("Could not spawn creature!");
			return;
		}

		spawned->setUInt64Value(unit_fields::SummonedBy, executer.getGuid());
		spawned->setFactionTemplate(executer.getFactionTemplate());
		spawned->setLevel(executer.getLevel());		// TODO
		executer.setUInt64Value(unit_fields::Summon, spawned->getGuid());
		spawned->setUInt32Value(unit_fields::CreatedBySpell, m_spell.id());
		spawned->setUInt64Value(unit_fields::CreatedBy, executer.getGuid());
		spawned->setUInt32Value(unit_fields::NpcFlags, 0);
		spawned->setUInt32Value(unit_fields::Bytes1, 0);
		spawned->setUInt32Value(unit_fields::PetNumber, guidLowerPart(spawned->getGuid()));
		world->addGameObject(*spawned);

		if (m_hitResults.find(spawned->getGuid()) == m_hitResults.end())
		{
			HitResult procInfo(m_attackerProc, m_victimProc, game::hit_info::NoAction, game::victim_state::Normal);
			m_hitResults.emplace(spawned->getGuid(), procInfo);
		}
		else
		{
			HitResult &procInfo = m_hitResults.at(spawned->getGuid());
			procInfo.add(game::hit_info::NoAction, game::victim_state::Normal);
		}
	}

	void SingleCastState::spellEffectCharge(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		if (!targets.empty())
		{
			GameUnit &firstTarget = *targets[0];
			m_affectedTargets.insert(firstTarget.shared_from_this());

			// TODO: Error checks and limit max path length
			auto &mover = caster.getMover();
			mover.moveTo(firstTarget.getLocation(), 25.0f);

			if (m_hitResults.find(firstTarget.getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[0], victimStates[0], resists[0]);
				m_hitResults.emplace(firstTarget.getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(firstTarget.getGuid());
				procInfo.add(hitInfos[0], victimStates[0], resists[0]);
			}
		}
	}

	void SingleCastState::spellEffectAttackMe(const proto::SpellEffect &effect)
	{
		GameUnit &caster = m_cast.getExecuter();
		std::vector<GameUnit *> targets;
		std::vector<game::VictimState> victimStates;
		std::vector<game::HitInfo> hitInfos;
		std::vector<float> resists;
		m_attackTable.checkSpell(&caster, m_target, m_spell, effect, targets, victimStates, hitInfos, resists);

		for (UInt32 i = 0; i < targets.size(); i++)
		{
			GameUnit *targetUnit = targets[i];
			m_affectedTargets.insert(targetUnit->shared_from_this());

			GameUnit *topThreatener = targetUnit->getTopThreatener().value();
			if (topThreatener)
			{
				float addThread = targetUnit->getThreat(*topThreatener).value();
				addThread -= targetUnit->getThreat(caster).value();
				if (addThread > 0.0f) {
					targetUnit->threaten(caster, addThread);
				}
			}

			if (m_hitResults.find(targetUnit->getGuid()) == m_hitResults.end())
			{
				HitResult procInfo(m_attackerProc, m_victimProc, hitInfos[i], victimStates[i], resists[i]);
				m_hitResults.emplace(targetUnit->getGuid(), procInfo);
			}
			else
			{
				HitResult &procInfo = m_hitResults.at(targetUnit->getGuid());
				procInfo.add(hitInfos[i], victimStates[i], resists[i]);
			}
		}
	}

	void SingleCastState::spellEffectScript(const proto::SpellEffect &effect)
	{
		switch (m_spell.id())
		{
			case 20271:	// Judgment
						// aura = get active seal from aura_container (Dummy-effect)
						// m_cast.getExecuter().castSpell(target, aura.getBasePoints(), -1, 0, false);
				break;
			default:
				break;
		}
	}
}
