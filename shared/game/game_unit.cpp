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
#include "game_unit.h"
#include "log/default_log_levels.h"
#include "world_instance.h"
#include "each_tile_in_sight.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include "game_character.h"
#include "proto_data/project.h"
#include "experience.h"
#include "unit_mover.h"
#include "common/make_unique.h"
#include "game_creature.h"

namespace wowpp
{
	GameUnit::GameUnit(
	    proto::Project &project,
	    TimerQueue &timers)
		: GameObject(project)
		, m_timers(timers)
		, m_raceEntry(nullptr)
		, m_classEntry(nullptr)
		, m_despawnCountdown(timers)
		, m_victim(nullptr)
		, m_attackSwingCountdown(timers)
		, m_lastMainHand(0)
		, m_lastOffHand(0)
		, m_weaponAttack(game::weapon_attack::BaseAttack)
		, m_regenCountdown(timers)
		, m_factionTemplate(nullptr)
		, m_lastManaUse(0)
		, m_auras(*this)
		, m_mechanicImmunity(0)
		, m_isStealthed(false)
		, m_state(unit_state::Default)
		, m_standState(unit_stand_state::Stand)
	{
		// Resize values field
		m_values.resize(unit_fields::UnitFieldCount, 0);
		m_valueBitset.resize((unit_fields::UnitFieldCount + 31) / 32, 0);

		// Reset unit speed
		m_speedBonus.fill(1.0f);

		// Setup unit mover
		m_mover = make_unique<UnitMover>(*this);

		// Create spell caster
		m_spellCast.reset(new SpellCast(m_timers, *this));

		// Setup despawn countdown
		m_despawnCountdown.ended.connect(
		    std::bind(&GameUnit::onDespawnTimer, this));
		m_attackSwingCountdown.ended.connect(
		    std::bind(&GameUnit::onAttackSwing, this));
		m_regenCountdown.ended.connect(
		    std::bind(&GameUnit::onRegeneration, this));
	}

	GameUnit::~GameUnit()
	{
		// Despawn all assigned world objects
		for (auto it : m_worldObjects)
		{
			it->getWorldInstance()->removeGameObject(*it);
		}
	}

	void GameUnit::initialize()
	{
		GameObject::initialize();

		// Initialize unit mods
		for (size_t i = 0; i < unit_mods::End; ++i)
		{
			m_unitMods[i][unit_mod_type::BaseValue] = 0.0f;
			m_unitMods[i][unit_mod_type::TotalValue] = 0.0f;
			m_unitMods[i][unit_mod_type::BasePct] = 1.0f;
			m_unitMods[i][unit_mod_type::TotalPct] = 1.0f;
		}

		// Initialize some values
		setUInt32Value(object_fields::Type, 9);							//OBJECT_FIELD_TYPE				(TODO: Flags)
		setFloatValue(object_fields::ScaleX, 1.0f);						//OBJECT_FIELD_SCALE_X			(Float)

		// Set unit values
		setUInt32Value(unit_fields::Health, 60);						//UNIT_FIELD_HEALTH
		setUInt32Value(unit_fields::MaxHealth, 60);						//UNIT_FIELD_MAXHEALTH

		setUInt32Value(unit_fields::Power1, 100);						//UNIT_FIELD_POWER1		Mana
		setUInt32Value(unit_fields::Power2, 0);							//UNIT_FIELD_POWER2		Rage
		setUInt32Value(unit_fields::Power3, 100);						//UNIT_FIELD_POWER3		Focus
		setUInt32Value(unit_fields::Power4, 100);						//UNIT_FIELD_POWER4		Energy
		setUInt32Value(unit_fields::Power5, 0);							//UNIT_FIELD_POWER5		Happiness

		setUInt32Value(unit_fields::MaxPower1, 100);					//UNIT_FIELD_MAXPOWER1	Mana
		setUInt32Value(unit_fields::MaxPower2, 1000);					//UNIT_FIELD_MAXPOWER2	Rage
		setUInt32Value(unit_fields::MaxPower3, 100);					//UNIT_FIELD_MAXPOWER3	Focus
		setUInt32Value(unit_fields::MaxPower4, 100);					//UNIT_FIELD_MAXPOWER4	Energy
		setUInt32Value(unit_fields::MaxPower5, 100);					//UNIT_FIELD_MAXPOWER4	Happiness

		setUInt32Value(unit_fields::UnitFlags, 0x00001000);				//UNIT_FIELD_FLAGS				(TODO: Flags)	UNIT_FLAG_PVP_ATTACKABLE
		setUInt32Value(unit_fields::BaseAttackTime, 2000);				//UNIT_FIELD_BASEATTACKTIME
		setUInt32Value(unit_fields::BaseAttackTime + 1, 2000);			//UNIT_FIELD_OFFHANDATTACKTIME
		setUInt32Value(unit_fields::RangedAttackTime, 2000);			//UNIT_FIELD_RANGEDATTACKTIME
		setFloatValue(unit_fields::BoundingRadius, 0.388999998569489f);	//UNIT_FIELD_BOUNDINGRADIUS		(TODO: Float)
		setFloatValue(unit_fields::CombatReach, 1.5f);					//UNIT_FIELD_COMBATREACH		(TODO: Float)
		//setUInt32Value(unit_fields::Bytes1, 0x00110000);				//UNIT_FIELD_BYTES_1

		setFloatValue(unit_fields::ModCastSpeed, 1.0f);					//UNIT_MOD_CAST_SPEED
		//setUInt32Value(unit_fields::Bytes2, 0x00002800);				//UNIT_FIELD_BYTES_2
		setUInt32Value(unit_fields::AttackPower, 29);					//UNIT_FIELD_ATTACK_POWER
		setUInt32Value(unit_fields::RangedAttackPower, 11);				//UNIT_FIELD_RANGED_ATTACK_POWER
		setUInt32Value(unit_fields::MinRangedDamage, 0x40249249);		//UNIT_FIELD_MINRANGEDDAMAGE	(TODO: Float)
		setUInt32Value(unit_fields::MaxRangedDamage, 0x40649249);		//UNIT_FIELD_MAXRANGEDDAMAGE	(TODO: Float)

		// Init some values
		setRace(1);
		setClass(1);
		setGender(game::gender::Male);
		setLevel(1);

		// Not in fight
		removeFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);
		addFlag(unit_fields::UnitFlags, game::unit_flags::PvP);
	}

	void GameUnit::raceUpdated()
	{
		m_raceEntry = getProject().races.getById(getRace());// m_getRace(getRace());
		assert(m_raceEntry);

		const auto *factionTemplate = getProject().factionTemplates.getById(m_raceEntry->faction());
		assert(factionTemplate);

		setFactionTemplate(*factionTemplate);
	}

	void GameUnit::classUpdated()
	{
		m_classEntry = m_project.classes.getById(getClass());
		assert(m_classEntry);

		// Update power type
		setByteValue(unit_fields::Bytes0, 3, m_classEntry->powertype());

		// Unknown what this does...
		if (m_classEntry->powertype() == static_cast<wowpp::proto::ClassEntry_PowerType>(game::power_type::Rage) ||
		        m_classEntry->powertype() == static_cast<wowpp::proto::ClassEntry_PowerType>(game::power_type::Mana))

		{
			setByteValue(unit_fields::Bytes1, 1, 0xEE);
		}
		else
		{
			setByteValue(unit_fields::Bytes1, 1, 0x00);
		}
	}

	void GameUnit::threaten(GameUnit & threatener, float amount)
	{
		onThreat(threatener, amount);
		threatened(threatener, amount);
	}

	void GameUnit::onThreat(GameUnit & threatener, float amount)
	{
		// Nothing special here
	}

	void GameUnit::setRace(UInt8 raceId)
	{
		setByteValue(unit_fields::Bytes0, 0, raceId);
		raceUpdated();
	}

	void GameUnit::setClass(UInt8 classId)
	{
		setByteValue(unit_fields::Bytes0, 1, classId);
		classUpdated();
	}

	void GameUnit::setGender(game::Gender gender)
	{
		setByteValue(unit_fields::Bytes0, 2, gender);
		updateDisplayIds();
	}

	void GameUnit::updateDisplayIds()
	{
		//UNIT_FIELD_DISPLAYID && UNIT_FIELD_NATIVEDISPLAYID
		if (getGender() == game::gender::Male)
		{
			setUInt32Value(unit_fields::DisplayId, m_raceEntry->malemodel());
			setUInt32Value(unit_fields::NativeDisplayId, m_raceEntry->malemodel());
		}
		else
		{
			setUInt32Value(unit_fields::DisplayId, m_raceEntry->femalemodel());
			setUInt32Value(unit_fields::NativeDisplayId, m_raceEntry->femalemodel());
		}
	}

	void GameUnit::setLevel(UInt8 level)
	{
		UInt32 prevLevel = getLevel();
		setUInt32Value(unit_fields::Level, level);

		// Get level information
		const auto *levelInfo = getProject().levels.getById(level);
		if (!levelInfo) {
			return;    // Creatures can have a level higher than 70
		}

		// Level info changed
		levelChanged(*levelInfo);

		// Get old level information
		const auto *oldLevel = getProject().levels.getById(prevLevel);
		if (oldLevel)
		{
			const auto raceIt = levelInfo->stats().find(getRace());
			if (raceIt == levelInfo->stats().end()) {
				return;
			}
			const auto classIt = raceIt->second.stats().find(getClass());
			if (classIt == raceIt->second.stats().end()) {
				return;
			}

			const auto raceItOld = oldLevel->stats().find(getRace());
			if (raceItOld == oldLevel->stats().end()) {
				return;
			}
			const auto classItOld = raceItOld->second.stats().find(getClass());
			if (classItOld == raceItOld->second.stats().end()) {
				return;
			}

			// Calculate difference
			const auto &oldStats = classItOld->second;
			const auto &newStats = classIt->second;

			// Prevent overflow
			if (static_cast<int>(prevLevel) > getClassEntry()->levelbasevalues_size() ||
			        level > getClassEntry()->levelbasevalues_size())
			{
				return;
			}

			auto oldBase = getClassEntry()->levelbasevalues(prevLevel - 1);
			auto newBase = getClassEntry()->levelbasevalues(level - 1);

			// Fire signal
			levelGained(
			    prevLevel,
			    newBase.health() - oldBase.health(),
			    newBase.mana() - oldBase.mana(),
			    newStats.stat1() - oldStats.stat1(),
			    newStats.stat2() - oldStats.stat2(),
			    newStats.stat3() - oldStats.stat3(),
			    newStats.stat4() - oldStats.stat4(),
			    newStats.stat5() - oldStats.stat5());
		}
	}

	void GameUnit::addWorldObject(std::shared_ptr<WorldObject> object)
	{
		// Store a pointer
		m_worldObjects.push_back(object);
	}

	void GameUnit::setFlightMode(bool enable)
	{
		if (enable)
		{
			setByteValue(unit_fields::Bytes1, 3, getByteValue(unit_fields::Bytes1, 3) | 2);
			m_movementInfo.moveFlags = game::MovementFlags(m_movementInfo.moveFlags | game::movement_flags::Flying | game::movement_flags::Flying2);
		}
		else
		{
			setByteValue(unit_fields::Bytes1, 3, getByteValue(unit_fields::Bytes1, 3) & ~2);
			m_movementInfo.moveFlags = game::MovementFlags(m_movementInfo.moveFlags & ~(game::movement_flags::Flying | game::movement_flags::Flying2));
		}
	}

	void GameUnit::levelChanged(const proto::LevelEntry &levelInfo)
	{
		// Get race and class
		const auto race = getRace();
		const auto cls = getClass();

		const auto raceIt = levelInfo.stats().find(race);
		if (raceIt == levelInfo.stats().end()) {
			return;
		}

		const auto classIt = raceIt->second.stats().find(cls);
		if (classIt == raceIt->second.stats().end()) {
			return;
		}

		// Update stats based on level information
		const auto &stats = classIt->second;
		for (UInt32 i = 0; i < 5; ++i)
		{
			const UnitMods mod = getUnitModByStat(i);
			switch (i)
			{
			case 0:
				setModifierValue(mod, unit_mod_type::BaseValue, stats.stat1());
				break;
			case 1:
				setModifierValue(mod, unit_mod_type::BaseValue, stats.stat2());
				break;
			case 2:
				setModifierValue(mod, unit_mod_type::BaseValue, stats.stat3());
				break;
			case 3:
				setModifierValue(mod, unit_mod_type::BaseValue, stats.stat4());
				break;
			case 4:
				setModifierValue(mod, unit_mod_type::BaseValue, stats.stat5());
				break;
			}
		}
	}

	void GameUnit::castSpell(SpellTargetMap target, UInt32 spellId, Int32 basePoints, GameTime castTime, bool isProc, UInt64 itemGuid, SpellSuccessCallback callback)
	{
		// Resolve spell
		const auto *spell = getProject().spells.getById(spellId);
		if (!spell)
		{
			WLOG("Could not find spell " << spellId);
			return;
		}

		auto result = m_spellCast->startCast(*spell, std::move(target), basePoints, castTime, isProc, itemGuid);
		if (result.first == game::spell_cast_result::CastOkay)
		{
			if (!isProc) {
				startedCasting(*spell);
			}
		}

		// Reset auto attack timer if requested
		if (result.first == game::spell_cast_result::CastOkay &&
		        m_attackSwingCountdown.running &&
		        result.second)
		{
			if (!(spell->attributes(1) & game::spell_attributes_ex_a::NotResetSwingTimer) && !isProc)
			{
				// Register for casts ended-event
				if (castTime > 0)
				{
					// Pause auto attack during spell cast
					m_attackSwingCountdown.cancel();
					result.second->ended.connect(
					    std::bind(&GameUnit::onSpellCastEnded, this, std::placeholders::_1));
				}
				else
				{
					// Cast already finished since it was an instant cast
					onSpellCastEnded(true);
				}
			}
		}

		if (callback)
		{
			callback(result.first);
		}
	}

	void GameUnit::onDespawnTimer()
	{
		if (m_worldInstance)
		{
			m_worldInstance->removeGameObject(*this);
		}
	}

	void GameUnit::setVictim(GameUnit *victim)
	{
		if (m_victim && !victim)
		{
			// Stop auto attack
			stopAttack();
		}

		m_victim = victim;

		// Update target value
		setUInt64Value(unit_fields::Target, m_victim ? m_victim->getGuid() : 0);
		if (m_victim)
		{
			m_victimDied = m_victim->killed.connect(
			                   std::bind(&GameUnit::onVictimKilled, this, std::placeholders::_1));
			m_victimDespawned = m_victim->despawned.connect(
			                        std::bind(&GameUnit::onVictimDespawned, this));
		}

	}

	void GameUnit::triggerDespawnTimer(GameTime despawnDelay)
	{
		// Start despawn countdown (may override previous countdown)
		m_despawnCountdown.setEnd(
		    getCurrentTime() + despawnDelay);
	}

	void GameUnit::cancelCast(game::SpellInterruptFlags reason, UInt64 interruptCooldown/* = 0*/)
	{
		// Stop attack swing callback in case there is one
		if (m_swingCallback) {
			m_swingCallback = AttackSwingCallback();
		}

		// Interrupt cooldown
		m_spellCast->stopCast(reason, interruptCooldown);
	}

	void GameUnit::startAttack()
	{
		if (isFeared() || isStunned())
			return;

		// No victim?
		if (!m_victim) {
			return;
		}

		// Check if the target is alive
		if (!m_victim->isAlive())
		{
			autoAttackError(attack_swing_error::TargetDead);
			return;
		}

		// Already attacking?
		if (m_attackSwingCountdown.running) {
			return;
		}

		TileIndex2D tileIndex;
		if (!getTileIndex(tileIndex))
		{
			// We can't attack since we do not belong to a world
			return;
		}

		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket packet(sink);
		game::server_write::attackStart(packet, getGuid(), m_victim->getGuid());

		// Notify all tile subscribers about this event
		forEachSubscriberInSight(
		    m_worldInstance->getGrid(),
		    tileIndex,
		    [&packet, &buffer](ITileSubscriber & subscriber)
		{
			subscriber.sendPacket(packet, buffer);
		});

		triggerNextAutoAttack();
		startedAttacking();
	}

	void GameUnit::stopAttack()
	{
		// Stop auto attack countdown
		m_attackSwingCountdown.cancel();

		// Reset onSwing callback
		if (m_swingCallback)
		{
			m_swingCallback = AttackSwingCallback();
		}

		// We need to have a valid victim
		if (!m_victim)
		{
			return;
		}

		TileIndex2D tileIndex;
		if (!getTileIndex(tileIndex))
		{
			return;
		}

		// Notify all subscribers
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket packet(sink);
		game::server_write::attackStop(packet, getGuid(), m_victim->getGuid());

		// Notify all tile subscribers about this event
		forEachSubscriberInSight(
		    m_worldInstance->getGrid(),
		    tileIndex,
		    [&packet, &buffer](ITileSubscriber & subscriber)
		{
			subscriber.sendPacket(packet, buffer);
		});
	}

	void GameUnit::onAttackSwing()
	{
		// Check if we still have a victim
		if (!m_victim || !isAlive())
		{
			return;
		}

		// Save pointer to victim because there are many places where m_victim may
		// become invalid during this function
		GameUnit *victim = m_victim;
		if (getTypeId() != object_type::Character)
		{
			// Turn to target if npc
			setOrientation(getAngle(*victim));
		}

		// Remember this weapon swing
		GameTime now = getCurrentTime();
		switch (m_weaponAttack)
		{
			case game::weapon_attack::BaseAttack:
				m_lastMainHand = now;
				break;
			case game::weapon_attack::OffhandAttack:
				m_lastOffHand = now;
				break;
			default:
				break;
		}

		// Used to jump to next weapon swing trigger
		do
		{
			// Get target location
			const math::Vector3 & location = victim->getLocation();

			// Distance check
			const float distance = getDistanceTo(*victim);
			const float combatRange = getMeleeReach() + victim->getMeleeReach();
			if (distance > combatRange)
			{
				autoAttackError(attack_swing_error::OutOfRange);

				// Reset attack swing callback
				if (m_swingCallback)
				{
					m_swingCallback = AttackSwingCallback();
				}

				// Don't reset auto attack
				GameTime nextAutoAttack = now + (constants::OneSecond / 2);
				m_attackSwingCountdown.setEnd(nextAutoAttack);
				return;
			}

			// Check if that target is in front of us
			if (!isInArc(2.0f * 3.1415927f / 3.0f, location.x, location.y))
			{
				autoAttackError(attack_swing_error::WrongFacing);

				// Reset attack swing callback
				if (m_swingCallback)
				{
					m_swingCallback = AttackSwingCallback();
				}

				// Don't reset auto attack
				GameTime nextAutoAttack = now + (constants::OneSecond / 2);
				m_attackSwingCountdown.setEnd(nextAutoAttack);
				return;
			}

			bool result = false;
			if (m_swingCallback)
			{
				// Execute onSwing callback (used by some melee spells which are executed on next attack swing and
				// replace auto attack)
				result = m_swingCallback();

				// Reset attack swing callback
				m_swingCallback = AttackSwingCallback();
			}

			if (!result)
			{
				TileIndex2D tileIndex;
				if (!getTileIndex(tileIndex))
				{
					return;
				}

				std::vector<GameUnit *> targets;
				std::vector<game::VictimState> victimStates;
				std::vector<game::HitInfo> hitInfos;
				std::vector<float> resists;
				AttackTable attackTable;
				UInt8 school = game::spell_school_mask::Normal;	// may vary
				attackTable.checkMeleeAutoAttack(this, victim, school, targets, victimStates, hitInfos, resists);

				for (int i = 0; i < targets.size(); i++)
				{
					GameUnit *targetUnit = targets[i];

					UInt32 totalDamage = 0;
					UInt32 blocked = 0;
					bool crit = false;
					UInt32 resisted = 0;
					UInt32 absorbed = 0;
					if (victimStates[i] == game::victim_state::IsImmune)
					{
						totalDamage = 0;
					}
					else if (hitInfos[i] == game::hit_info::Miss)
					{
						totalDamage = 0;
					}
					else if (victimStates[i] == game::victim_state::Dodge)
					{
						totalDamage = 0;
					}
					else if (victimStates[i] == game::victim_state::Parry)
					{
						totalDamage = 0;
						//TODO accelerate next m_victim autohit
					}
					else
					{
						const auto minDmgField = (m_weaponAttack == game::weapon_attack::BaseAttack ? unit_fields::MinDamage : unit_fields::MinOffHandDamage);
						const auto maxDmgField = (m_weaponAttack == game::weapon_attack::BaseAttack ? unit_fields::MaxDamage : unit_fields::MaxOffHandDamage);

						// Calculate damage between minimum and maximum damage
						std::uniform_real_distribution<float> distribution(getFloatValue(minDmgField), getFloatValue(maxDmgField) + 1.0f);
						totalDamage = victim->calculateArmorReducedDamage(getLevel(), UInt32(distribution(randomGenerator)));

						// Apply damage bonus
						applyDamageDoneBonus(school, 1, totalDamage);
						targetUnit->applyDamageTakenBonus(school, 1, totalDamage);

						if (hitInfos[i] == game::hit_info::Glancing)
						{
							bool attackerIsCaster = false;	//TODO check it
							float attackerRating = getLevel() * 5.0f;	//TODO get real rating
							float victimRating = targetUnit->getLevel() * 5.0f;
							float ratingDiff = victimRating - attackerRating;

							float minFactor = 1.3f - (0.05f * ratingDiff);
							if (attackerIsCaster)
							{
								minFactor -= 0.7f;
								if (minFactor > 0.6f) {
									minFactor = 0.6f;
								}
							}
							else
							{
								if (minFactor > 0.91f) {
									minFactor = 0.91f;
								}
							}
							if (minFactor < 0.01f) {
								minFactor = 0.01f;
							}

							float maxFactor = 1.2f - (0.03f * ratingDiff);
							if (attackerIsCaster) {
								maxFactor -= 0.3f;
							}
							if (maxFactor < 0.2f) {
								maxFactor = 0.2f;
							}
							if (maxFactor > 0.99f) {
								maxFactor = 0.99f;
							}

							std::uniform_real_distribution<float> glanceDice(minFactor, maxFactor);
							totalDamage *= glanceDice(randomGenerator);
						}
						else if (victimStates[i] == game::victim_state::Blocks)
						{
							UInt32 blockValue = 50;	//TODO get from m_victim
							if (blockValue >= totalDamage)	//avoid negative damage when blockValue is high
							{
								blocked = totalDamage;
								totalDamage = 0;
							}
							else
							{
								totalDamage -= blockValue;
								blocked = blockValue;
							}
						}
						else if (hitInfos[i] == game::hit_info::CriticalHit)
						{
							crit = true;
							totalDamage *= 2.0f;
						}
						else if (hitInfos[i] == game::hit_info::Crushing)
						{
							totalDamage *= 1.5f;
						}

						resisted = totalDamage * (resists[i] / 100.0f);
						absorbed = targetUnit->consumeAbsorb(totalDamage - resisted, school);
						if (absorbed > 0 && absorbed == totalDamage)
						{
							hitInfos[i] = static_cast<game::HitInfo>(hitInfos[i] | game::hit_info::Absorb);
						}
					}

					// Make the animation look like an off hand attack if off hand weapon is used
					if (m_weaponAttack == game::weapon_attack::OffhandAttack)
					{
						if (hitInfos[i] & game::hit_info::NormalSwing2)
						{
							hitInfos[i] = game::HitInfo(hitInfos[i] & ~game::hit_info::NormalSwing2);
							hitInfos[i] = game::HitInfo(hitInfos[i] | game::hit_info::LeftSwing);
						}
					}

					// Notify all subscribers
					std::vector<char> buffer;
					io::VectorSink sink(buffer);
					game::Protocol::OutgoingPacket packet(sink);
					game::server_write::attackStateUpdate(packet, getGuid(), victim->getGuid(), hitInfos[i], totalDamage, absorbed, resisted, blocked, victimStates[i], m_weaponAttack, 1);

					// Notify all tile subscribers about this event
					forEachSubscriberInSight(
					    m_worldInstance->getGrid(),
					    tileIndex,
					    [&packet, &buffer](ITileSubscriber & subscriber)
					{
						subscriber.sendPacket(packet, buffer);
					});

					// Check if we need to give rage
					if (getByteValue(unit_fields::Bytes0, 3) == game::power_type::Rage)
					{
						const float weaponSpeedHitFactor = (getUInt32Value(unit_fields::BaseAttackTime) / 1000.0f) * 3.5f;
						const UInt32 level = getLevel();
						const float rageconversion = ((0.0091107836f * level * level) + 3.225598133f * level) + 4.2652911f;
						float addRage = ((totalDamage / rageconversion * 7.5f + weaponSpeedHitFactor) / 2.0f) * 10.0f;

						UInt32 currentRage = getUInt32Value(unit_fields::Power2);
						UInt32 maxRage = getUInt32Value(unit_fields::MaxPower2);
						if (currentRage + addRage > maxRage)
						{
							currentRage = maxRage;
						}
						else
						{
							currentRage += addRage;
						}
						setUInt32Value(unit_fields::Power2, currentRage);
					}

					// Deal damage (Note: m_victim can become nullptr, if the target dies)
					if (totalDamage > 0)
					{
						victim->takenMeleeAutoAttack(this);
						victim->dealDamage(totalDamage - resisted - absorbed, (1 << 0), this, totalDamage - resisted - absorbed);

						// Trigger auto attack procs
						doneMeleeAutoAttack(victim);
					}
					else
					{
						// Still add threat to enter combat in case of miss etc.
						victim->threaten(*this, 0.0f);
					}
				}
			}
		}
		while (false);

		// Do we still have an attack target? If so, trigger the next attack swing
		if (m_victim)
		{
			// Notify about successful auto attack to reset last error message
			autoAttackError(attack_swing_error::Success);

			triggerNextAutoAttack();
		}
	}

	void GameUnit::onVictimKilled(GameUnit *killer)
	{
		setVictim(nullptr);
	}

	void GameUnit::onVictimDespawned()
	{
		setVictim(nullptr);
	}

	void GameUnit::startRegeneration()
	{
		if (!m_regenCountdown.running)
		{
			m_regenCountdown.setEnd(
			    getCurrentTime() + (constants::OneSecond * 2));
		}
	}

	void GameUnit::stopRegeneration()
	{
		m_regenCountdown.cancel();
	}

	void GameUnit::onRegeneration()
	{
		// Dead units don't regenerate
		if (!isAlive())
		{
			return;
		}

		// Do this only while not in combat
		if (!isInCombat())
		{
			regenerateHealth();
			if (!m_auras.hasAura(game::aura_type::InterruptRegen))
			{
				regeneratePower(game::power_type::Rage);
			}
		}

		regeneratePower(game::power_type::Energy);
		regeneratePower(game::power_type::Mana);

		// Restart regeneration timer
		startRegeneration();
	}

	void GameUnit::regeneratePower(game::PowerType power)
	{
		float modPower = 0.0f;
		switch (power)
		{
		case game::power_type::Mana:
			{
				if (isGameCharacter())
				{
					if ((m_lastManaUse + constants::OneSecond * 5) < getCurrentTime())
					{
						modPower = getFloatValue(character_fields::ModManaRegen) * 2.0f;
					}
					else
					{
						modPower = getFloatValue(character_fields::ModManaRegenInterrupt) * 2.0f;
					}
				}
				else
				{
					if ((m_lastManaUse + constants::OneSecond * 5) < getCurrentTime())
					{
						modPower = 2.0f * getLevel();
					}
				}
				break;
			}

		case game::power_type::Energy:
			{
				// 20 energy per tick
				modPower = 20.0f;
				break;
			}

		case game::power_type::Rage:
			{
				// Take 3 rage per tick
				modPower = -30.0f;
				break;
			}

		default:
			break;
		}
		addPower(power, modPower);
	}

	void GameUnit::notifyManaUse()
	{
		m_lastManaUse = getCurrentTime();
	}

	void GameUnit::updateAllStats()
	{
		for (UInt8 stat = 0; stat < 5; ++stat)
		{
			updateStats(stat);
		}

		updateDamage();
		updateArmor();
		updateMaxHealth();
		for (size_t i = 0; i < game::power_type::Happiness; ++i)
		{
			updateMaxPower(static_cast<game::PowerType>(i));
		}
		for (UInt8 i = 0; i < 6; ++i)
		{
			updateResistance(i);
		}

		updateManaRegen();
	}

	void GameUnit::updateMaxHealth()
	{
		float baseVal = getUInt32Value(unit_fields::BaseHealth);
		baseVal += getHealthBonusFromStamina();

		const auto mod = unit_mods::Health;

		const float basePct = getModifierValue(mod, unit_mod_type::BasePct);
		const float totalVal = getModifierValue(mod, unit_mod_type::TotalValue);
		const float totalPct = getModifierValue(mod, unit_mod_type::TotalPct);
		float value = ((baseVal * basePct) + totalVal) * totalPct;

		setUInt32Value(unit_fields::MaxHealth, UInt32(value));
	}

	void GameUnit::updateMaxPower(game::PowerType power)
	{
		UInt32 createPower = 0;
		switch (power)
		{
		case game::power_type::Mana:
			createPower = getUInt32Value(unit_fields::BaseMana);
			break;
		case game::power_type::Rage:
			createPower = 1000;
			break;
		case game::power_type::Energy:
			createPower = 100;
			break;
		default:	// Make compiler happy
			break;
		}

		float powerBonus = (power == game::power_type::Mana && createPower > 0) ? getManaBonusFromIntellect() : 0;

		float baseVal = createPower;
		baseVal += powerBonus;

		const auto mod = UnitMods(unit_mods::PowerStart + power);
		const float basePct = getModifierValue(mod, unit_mod_type::BasePct);
		const float totalVal = getModifierValue(mod, unit_mod_type::TotalValue);
		const float totalPct = getModifierValue(mod, unit_mod_type::TotalPct);
		float value = ((baseVal * basePct) + totalVal) * totalPct;

		setUInt32Value(unit_fields::MaxPower1 + static_cast<UInt16>(power), UInt32(value));
	}

	void GameUnit::updateArmor()
	{
		Int32 baseArmor = getModifierValue(unit_mods::Armor, unit_mod_type::BaseValue);
		Int32 totalArmor = getModifierValue(unit_mods::Armor, unit_mod_type::TotalValue);

		// Add armor from agility
		baseArmor += getUInt32Value(unit_fields::Stat1) * 2;
		baseArmor += totalArmor;

		if (baseArmor < 0) {
			baseArmor = 0;
		}

		setUInt32Value(unit_fields::Resistances, baseArmor);
		setUInt32Value(unit_fields::ResistancesBuffModsPositive, totalArmor);
	}

	void GameUnit::updateDamage()
	{
		// Nothing to do here
	}

	void GameUnit::updateManaRegen()
	{
		// Nothing to do here
	}

	void GameUnit::updateResistance(UInt8 resistance)
	{
		if (resistance > 0)
		{
			UnitMods mod = getUnitModByResistance(resistance);

			// Calculate values
			const float baseVal = getModifierValue(mod, unit_mod_type::BaseValue);
			const float basePct = getModifierValue(mod, unit_mod_type::BasePct);
			const float totalVal = getModifierValue(mod, unit_mod_type::TotalValue);
			const float totalPct = getModifierValue(mod, unit_mod_type::TotalPct);
			float value = ((baseVal * basePct) + totalVal) * totalPct;

			setUInt32Value(unit_fields::Resistances + resistance, UInt32(value));
			setUInt32Value(unit_fields::ResistancesBuffModsPositive + resistance, UInt32(totalVal));
		}
		else
		{
			updateArmor();
		}
	}

	void GameUnit::applyDamageDoneBonus(UInt32 schoolMask, UInt32 tickCount, UInt32 & damage)
	{
		getAuras().forEachAuraOfType(game::aura_type::ModDamageDone, [&damage, schoolMask, tickCount](Aura &aura) -> bool {
			if (aura.getEffect().miscvaluea() & schoolMask)
			{
				Int32 bonus = aura.getBasePoints();
				if (tickCount > 1)
				{
					bonus /= tickCount;
				}

				if (bonus < 0 && -bonus >= damage)
				{
				}
				else
				{
					damage += bonus;
				}
			}

			return true;
		});

		getAuras().forEachAuraOfType(game::aura_type::ModDamagePercentDone, [&damage, schoolMask, tickCount](Aura &aura) -> bool {
			if (aura.getEffect().miscvaluea() & schoolMask)
			{
				Int32 bonus = aura.getBasePoints();
				if (tickCount > 1)
				{
					bonus /= tickCount;
				}

				if (bonus != 0)
				{
					damage = UInt32(damage * (float(100.0f + bonus) / 100.0f));
				}
			}

			return true;
		});
	}

	void GameUnit::applyDamageTakenBonus(UInt32 schoolMask, UInt32 tickCount, UInt32 & damage)
	{
		getAuras().forEachAuraOfType(game::aura_type::ModDamageTaken, [&damage, schoolMask, tickCount](Aura &aura) -> bool {
			if (aura.getEffect().miscvaluea() & schoolMask)
			{
				Int32 bonus = aura.getBasePoints();
				if (tickCount > 1)
				{
					bonus /= tickCount;
				}

				if (bonus < 0 && -bonus >= damage)
				{
				}
				else
				{
					damage += bonus;
				}
			}

			return true;
		});

		getAuras().forEachAuraOfType(game::aura_type::ModMeleeDamageTakenPct, [&damage, schoolMask, tickCount](Aura &aura) -> bool {
			if (aura.getEffect().miscvaluea() & schoolMask)
			{
				Int32 bonus = aura.getBasePoints();
				if (tickCount > 1)
				{
					bonus /= tickCount;
				}

				if (bonus != 0)
				{
					damage = UInt32(damage * (float(100.0f + bonus) / 100.0f));
				}
			}

			return true;
		});
	}

	void GameUnit::applyHealingTakenBonus(UInt32 tickCount, UInt32 & healing)
	{
		Int32 bonus = getAuras().getTotalBasePoints(game::aura_type::ModHealing);
		if (tickCount > 1)
		{
			bonus /= tickCount;
		}

		if (bonus < 0 && -bonus >= healing)
		{
		}
		else
		{
			healing += bonus;
		}

		getAuras().forEachAuraOfType(game::aura_type::ModHealingPct, [&healing, tickCount](Aura &aura) -> bool {
			Int32 bonus = aura.getBasePoints();
			if (tickCount > 1)
			{
				bonus /= tickCount;
			}

			if (bonus != 0)
			{
				healing = UInt32(healing * (float(100.0f + bonus) / 100.0f));
			}
			return true;
		});
	}

	void GameUnit::applyHealingDoneBonus(UInt32 tickCount, UInt32 & healing)
	{
		Int32 bonus = getAuras().getTotalBasePoints(game::aura_type::ModHealingDone);
		if (tickCount > 1)
		{
			bonus /= tickCount;
		}

		if (bonus < 0 && -bonus >= healing)
		{
		}
		else
		{
			healing += bonus;
		}

		getAuras().forEachAuraOfType(game::aura_type::ModHealingDonePct, [&healing, tickCount](Aura &aura) -> bool {
			Int32 bonus = aura.getBasePoints();
			if (tickCount > 1)
			{
				bonus /= tickCount;
			}

			if (bonus != 0)
			{
				healing = UInt32(healing * (float(100.0f + bonus) / 100.0f));
			}
			return true;
		});
	}

	bool GameUnit::hasCooldown(UInt32 spellId) const
	{
		auto it = m_spellCooldowns.find(spellId);
		if (it == m_spellCooldowns.end())
		{
			return false;
		}

		return it->second > getCurrentTime();
	}

	UInt32 GameUnit::getCooldown(UInt32 spellId) const
	{
		auto it = m_spellCooldowns.find(spellId);
		if (it == m_spellCooldowns.end()) {
			return 0;
		}

		GameTime currentTime = getCurrentTime();
		return (it->second > currentTime ? it->second - currentTime : 0);
	}

	void GameUnit::setCooldown(UInt32 spellId, UInt32 timeInMs)
	{
		if (timeInMs == 0) {
			m_spellCooldowns.erase(spellId);
		}
		else {
			m_spellCooldowns[spellId] = getCurrentTime() + timeInMs;
		}
	}

	float GameUnit::getHealthBonusFromStamina() const
	{
		float stamina = float(getUInt32Value(unit_fields::Stat2));

		float base = stamina < 20.0f ? stamina : 20.0f;
		float bonus = stamina - base;

		return base + (bonus * 10.0f);
	}

	float GameUnit::getManaBonusFromIntellect() const
	{

		float intellect = float(getUInt32Value(unit_fields::Stat3));

		float base = intellect < 20.0f ? intellect : 20.0f;
		float bonus = intellect - base;

		return base + (bonus * 15.0f);
	}

	float GameUnit::getModifierValue(UnitMods mod, UnitModType type) const
	{
		return m_unitMods[mod][type];
	}

	void GameUnit::setModifierValue(UnitMods mod, UnitModType type, float value)
	{
		m_unitMods[mod][type] = value;
	}

	void GameUnit::updateModifierValue(UnitMods mod, UnitModType type, float amount, bool apply)
	{
		if (mod >= unit_mods::End || type >= unit_mod_type::End)
		{
			return;
		}

		switch (type)
		{
		case unit_mod_type::BaseValue:
		case unit_mod_type::TotalValue:
			{
				m_unitMods[mod][type] += (apply ? amount : -amount);
				break;
			}

		case unit_mod_type::BasePct:
		case unit_mod_type::TotalPct:
			{
				if (amount == -100.0f) {
					amount = -99.99f;
				}
				m_unitMods[mod][type] *= (apply ? (100.0f + amount) / 100.0f : 100.0f / (100.0f + amount));
				break;
			}

		default:
			break;
		}

		// Update stats
		switch (mod)
		{
		case unit_mods::StatStrength:
		case unit_mods::StatAgility:
		case unit_mods::StatStamina:
		case unit_mods::StatIntellect:
		case unit_mods::StatSpirit:
			{
				updateStats(getStatByUnitMod(mod));
				break;
			}

		case unit_mods::Armor:
			{
				updateArmor();
				break;
			}

		case unit_mods::Health:
			{
				updateMaxHealth();
				break;
			}

		case unit_mods::Mana:
		case unit_mods::Rage:
		case unit_mods::Focus:
		case unit_mods::Energy:
		case unit_mods::Happiness:
			{
				updateMaxPower(getPowerTypeByUnitMod(mod));
				break;
			}

		case unit_mods::ResistanceHoly:
		case unit_mods::ResistanceFire:
		case unit_mods::ResistanceNature:
		case unit_mods::ResistanceFrost:
		case unit_mods::ResistanceShadow:
		case unit_mods::ResistanceArcane:
			{
				updateResistance(getResistanceByUnitMod(mod));
				break;
			}

		case unit_mods::AttackPower:
		case unit_mods::AttackPowerRanged:
		case unit_mods::DamageMainHand:
		case unit_mods::DamageOffHand:
		case unit_mods::DamageRanged:
			{
				updateDamage();
				break;
			}

		default:
			break;
		}
	}

	void GameUnit::updateStats(UInt8 stat)
	{
		// Validate stat
		if (stat > 4) {
			return;
		}

		// Determine unit mod
		const UnitMods mod = getUnitModByStat(stat);

		// Calculate values
		const float baseVal = getModifierValue(mod, unit_mod_type::BaseValue);
		const float basePct = getModifierValue(mod, unit_mod_type::BasePct);
		const float totalVal = getModifierValue(mod, unit_mod_type::TotalValue);
		const float totalPct = getModifierValue(mod, unit_mod_type::TotalPct);

		float value = ((baseVal * basePct) + totalVal) * totalPct;
		setInt32Value(unit_fields::Stat0 + stat, Int32(value));
		setInt32Value(unit_fields::PosStat0 + stat, Int32(totalVal));

		// Update values which are related to the stat change
		switch (stat)
		{
		case 0:
			updateDamage();
			break;
		case 1:
			updateArmor();
			updateDamage();
			break;
		case 2:
			updateMaxHealth();
			break;
		case 3:
			updateMaxPower(game::power_type::Mana);
			updateManaRegen();
			break;
		case 4:
			updateManaRegen();
			break;

		default:
			break;
		}
	}

	wowpp::UInt8 GameUnit::getStatByUnitMod(UnitMods mod)
	{
		UInt8 stat = 0;

		switch (mod)
		{
		case unit_mods::StatStrength:
			stat = 0;
			break;
		case unit_mods::StatAgility:
			stat = 1;
			break;
		case unit_mods::StatStamina:
			stat = 2;
			break;
		case unit_mods::StatIntellect:
			stat = 3;
			break;
		case unit_mods::StatSpirit:
			stat = 4;
			break;

		default:
			break;
		}

		return stat;
	}

	wowpp::game::PowerType GameUnit::getPowerTypeByUnitMod(UnitMods mod)
	{
		switch (mod)
		{
		case unit_mods::Rage:
			return game::power_type::Rage;
		case unit_mods::Focus:
			return game::power_type::Focus;
		case unit_mods::Energy:
			return game::power_type::Energy;
		case unit_mods::Happiness:
			return game::power_type::Happiness;

		default:
			break;
		}

		return game::power_type::Mana;
	}

	wowpp::UnitMods GameUnit::getUnitModByStat(UInt8 stat)
	{
		switch (stat)
		{
		case 1:
			return unit_mods::StatAgility;
		case 2:
			return unit_mods::StatStamina;
		case 3:
			return unit_mods::StatIntellect;
		case 4:
			return unit_mods::StatSpirit;

		default:
			break;
		}

		return unit_mods::StatStrength;
	}

	wowpp::UnitMods GameUnit::getUnitModByPower(game::PowerType power)
	{
		switch (power)
		{
		case game::power_type::Rage:
			return unit_mods::Rage;
		case game::power_type::Energy:
			return unit_mods::Energy;
		case game::power_type::Happiness:
			return unit_mods::Happiness;
		case game::power_type::Focus:
			return unit_mods::Focus;

		default:
			break;
		}

		return unit_mods::Mana;
	}

	wowpp::UInt8 GameUnit::getResistanceByUnitMod(UnitMods mod)
	{
		switch (mod)
		{
		case unit_mods::ResistanceHoly:
			return 1;
		case unit_mods::ResistanceFire:
			return 2;
		case unit_mods::ResistanceNature:
			return 3;
		case unit_mods::ResistanceFrost:
			return 4;
		case unit_mods::ResistanceShadow:
			return 5;
		case unit_mods::ResistanceArcane:
			return 6;

		default:
			break;
		}

		return 0;
	}

	wowpp::UnitMods GameUnit::getUnitModByResistance(UInt8 res)
	{
		switch (res)
		{
		case 0:
			return unit_mods::Armor;
		case 1:
			return unit_mods::ResistanceHoly;
		case 2:
			return unit_mods::ResistanceFire;
		case 3:
			return unit_mods::ResistanceNature;
		case 4:
			return unit_mods::ResistanceFrost;
		case 5:
			return unit_mods::ResistanceShadow;
		case 6:
			return unit_mods::ResistanceArcane;

		default:
			break;
		}

		return unit_mods::Armor;
	}

	bool GameUnit::isSitting() const
	{
		return (m_standState == unit_stand_state::Sit ||
		        m_standState == unit_stand_state::SitChair ||
		        m_standState == unit_stand_state::SitHighChair ||
		        m_standState == unit_stand_state::SitMediumChais);
	}

	void GameUnit::setStandState(UnitStandState state)
	{
		if (m_standState != state)
		{
			const bool wasSitting = isSitting();

			m_standState = state;
			setByteValue(unit_fields::Bytes1, 0, m_standState);
			standStateChanged(m_standState);

			// Not sitting any more, remove auras that require the unit to be sitting
			if (wasSitting && !isSitting())
			{
				m_auras.removeAllAurasDueToInterrupt(game::spell_aura_interrupt_flags::NotSeated);
			}
		}
	}

	bool GameUnit::dealDamage(UInt32 damage, UInt32 school, GameUnit *attacker, float threat)
	{
		UInt32 health = getUInt32Value(unit_fields::Health);
		if (health < 1)
		{
			return false;
		}

		// Add threat
		if (attacker)
		{
			if (attacker->isGameCharacter())
			{
				reinterpret_cast<GameCharacter*>(attacker)->applyThreatMod(school, threat);
			}

			threaten(*attacker, threat);
		}

		if (health < damage) {
			health = 0;
		}
		else {
			health -= damage;
		}

		setUInt32Value(unit_fields::Health, health);
		takenDamage(attacker, damage);

		UInt32 maxHealth = getUInt32Value(unit_fields::MaxHealth);
		float healthPct = float(health) / float(maxHealth);
		if (healthPct < 0.35)
		{
			addFlag(unit_fields::AuraState, game::aura_state::HealthLess35Percent);
		}
		if (healthPct < 0.2)
		{
			addFlag(unit_fields::AuraState, game::aura_state::HealthLess20Percent);
		}

		if (health < 1)
		{
			// Call function and signal
			onKilled(attacker);
			killed(attacker);
		}
		return true;
	}

	bool GameUnit::heal(UInt32 amount, GameUnit *healer, bool noThreat /*= false*/)
	{
		UInt32 health = getUInt32Value(unit_fields::Health);
		if (health < 1)
		{
			return false;
		}

		const UInt32 maxHealth = getUInt32Value(unit_fields::MaxHealth);
		if (health + amount >= maxHealth) {
			health = maxHealth;
		}
		else {
			health += amount;
		}

		setUInt32Value(unit_fields::Health, health);

		float healthPct = float(health) / float(maxHealth);
		if (healthPct >= 0.35)
		{
			removeFlag(unit_fields::AuraState, game::aura_state::HealthLess35Percent);
		}
		if (healthPct >= 0.2)
		{
			removeFlag(unit_fields::AuraState, game::aura_state::HealthLess20Percent);
		}

		if (healer && !noThreat)
		{
			// TODO: Add threat to all units who are in fight with the healed target, but only
			// if the units are not friendly towards the healer!
		}
		return true;
	}

	Int32 GameUnit::addPower(game::PowerType power, Int32 amount)
	{
		Int32 current = getUInt32Value(unit_fields::Power1 + static_cast<Int8>(power));
		Int32 max = getUInt32Value(unit_fields::MaxPower1 + static_cast<Int8>(power));

		Int32 addPower = amount;
		if (addPower + current < 0)
		{
			addPower = 0 - current;
		}
		else if (addPower + current > max)
		{
			addPower = max - current;
		}

		setUInt32Value(unit_fields::Power1 + static_cast<Int8>(power), current + addPower);
		return addPower;
	}

	void GameUnit::revive(UInt32 health, UInt32 mana)
	{
		if (isAlive()) {
			return;
		}

		float o = getOrientation();
		math::Vector3 location(getLocation());

		const UInt32 maxHealth = getUInt32Value(unit_fields::MaxHealth);
		if (health > maxHealth) {
			health = maxHealth;
		}
		setUInt32Value(unit_fields::Health, health);

		if (mana > 0)
		{
			const UInt32 maxMana = getUInt32Value(unit_fields::MaxPower1);
			if (mana > maxMana) {
				mana = maxMana;
			}
			setUInt32Value(unit_fields::Power1, mana);
		}

		// No longer marked for execute and stuff
		removeFlag(unit_fields::AuraState, game::aura_state::HealthLess35Percent);
		removeFlag(unit_fields::AuraState, game::aura_state::HealthLess20Percent);

		startRegeneration();

		// Raise moved event for aggro etc.
		location.z -= 0.1f;
		moved(*this, location, o);
	}

	void GameUnit::rewardExperience(GameUnit *victim, UInt32 experience)
	{
		// Nothing to do here
	}

	bool GameUnit::isImmune(UInt8 school)
	{
		// TODO: check auras and mobtype
		if (isCreature())
		{
			GameCreature& self = reinterpret_cast<GameCreature&>(*this);
			return (self.getEntry().schoolimmunity() & school) != 0;
		}
		return false;
	}

	void GameUnit::notifyStealthChanged()
	{
		const bool wasStealthed = m_isStealthed;
		m_isStealthed = m_auras.hasAura(game::aura_type::ModStealth);
		if (wasStealthed && !m_isStealthed)
		{
			stealthStateChanged(false);
		}
		else if (!wasStealthed && m_isStealthed)
		{
			stealthStateChanged(true);
		}
	}

	bool GameUnit::canDetectStealth(GameUnit &target)
	{
		// Can't detect anything if stunned
		if (isStunned() || isConfused() || isFeared()) {
			return false;
		}

		// Determine distance and check cap
		const float dist = getDistanceTo(target);
		if (dist < 0.24f) {
			return true;
		}

		// Target has to be in front of us
		auto targetPos = target.getLocation();
		if (!isInArc(3.1415927f, targetPos.x, targetPos.y)) {
			return false;
		}

		float angleModifier = 1.0f;
		if (!isInArc(3.1415927f * 0.5f, targetPos.x, targetPos.y))
		{
			angleModifier = 0.5f;
		}

		// Always visible
		if (m_auras.hasAura(game::aura_type::DetectStealth)) {
			return true;
		}

		// TODO: Check if target has aura "ModStalked" with us as caster (Hunter mark for example)

		// Base distance
		float visibleDistance = 7.5f * angleModifier;
		const float MaxPlayerStealthDetectRange = 45.0f;
		// Visible distance is modified by -Level Diff (every level diff = 1.0f in visible distance)
		visibleDistance += float(getLevel()) - target.getAuras().getTotalBasePoints(game::aura_type::ModStealth) / 5.0f;
		// -Stealth Mod(positive like Master of Deception) and Stealth Detection(negative like paranoia)
		// based on wowwiki every 5 mod we have 1 more level diff in calculation
		// TODO: Cache these stealth values maybe
		visibleDistance += (float)(m_auras.getTotalBasePoints(game::aura_type::ModStealthDetect) - target.getAuras().getTotalBasePoints(game::aura_type::ModStealthLevel)) / 5.0f;
		visibleDistance = visibleDistance > MaxPlayerStealthDetectRange ? MaxPlayerStealthDetectRange : visibleDistance;
		return dist < visibleDistance;
	}

	float GameUnit::getMissChance(GameUnit &attacker, UInt8 school, bool isWhiteDamage)
	{
		float chance;
		if (school == game::spell_school::Normal)	// melee
		{
			float attackerRating = attacker.getLevel() * 5;	//TODO get real rating
			float victimRating = getLevel() * 5;
			if (isWhiteDamage && attacker.hasOffHandWeapon())
			{
				chance = 24.0f;
			}
			else
			{
				chance = 5.0f;
			}
			if (getTypeId() == wowpp::object_type::Character)	// hit an character
			{
				if (victimRating > attackerRating)
				{
					chance += (victimRating - attackerRating) * 0.04f;
				}
				else if (victimRating < attackerRating)
				{
					chance -= (attackerRating - victimRating) * 0.02f;
				}
			}
			else	// hit an creature
			{
				if (victimRating > attackerRating + 10.0f)
				{
					chance += 2.0f + ((victimRating - attackerRating - 10.0f) * 0.4f);
				}
				else
				{
					chance += (victimRating - attackerRating) * 0.1f;
				}
			}
			if (chance < 0.0f) {
				chance = 0.0f;
			}
		}
		else	// spell
		{
			float levelModificator = static_cast<float>(attacker.getLevel()) - static_cast<float>(getLevel());
			if (levelModificator < -2.0f)
			{
				if (getTypeId() == wowpp::object_type::Character)
				{
					levelModificator = (levelModificator * 7.0f) + 12.0f;	//pvp calculation
				}
				else
				{
					levelModificator = (levelModificator * 11.0f) + 20.0f;	//pve calculation
				}
			}
			chance = 4.0f - levelModificator;
			if (chance < 1.0f) {
				chance = 1.0f;
			}
			if (chance > 99.0f) {
				chance = 99.0f;
			}
		}
		return chance;
	}

	float GameUnit::getDodgeChance(GameUnit &attacker)
	{
		return isStunned() ? 0.0f : 5.0f;
	}

	float GameUnit::getParryChance(GameUnit &attacker)
	{
		return isStunned() ? 0.0f : 5.0f;
	}

	float GameUnit::getGlancingChance(GameUnit &attacker)
	{
		UInt32 attackerLevel = attacker.getLevel();
		UInt32 victimLevel = getLevel();
		if (attacker.getTypeId() == wowpp::object_type::Character && this->getTypeId() == wowpp::object_type::Unit && attackerLevel <= victimLevel)
		{
			float attackerRating = attackerLevel * 5.0f;	//TODO get real rating
			float victimRating = victimLevel * 5.0f;
			return 10.0f + victimRating - attackerRating;
		}
		else
		{
			return 0.0f;
		}
	}

	float GameUnit::getBlockChance()
	{
		if (canBlock() && !isStunned())
		{
			return 5.0f;
		}
		else
		{
			return 0.0f;
		}
	}

	float GameUnit::getCrushChance(GameUnit &attacker)
	{
		if (attacker.getTypeId() == wowpp::object_type::Unit)
		{
			float attackerRating = attacker.getLevel() * 5.0f;	//TODO get real rating
			float victimRating = getLevel() * 5.0f;
			float crushChance = (2.0f * std::abs(attackerRating - victimRating)) - 15.0f;
			if (crushChance < 0.0f) {
				crushChance = 0.0f;
			}
			return crushChance;
		}
		else
		{
			return 0.0f;
		}
	}

	float GameUnit::getResiPercentage(UInt8 school, UInt32 attackerLevel, bool isBinary)
	{
		if (school <= 1)
		{
			return 0.0f;
		}

		std::uniform_real_distribution<float> resiDistribution(0.0f, 99.9f);
		UInt32 spellPen = 0;
		UInt32 resiOffset = static_cast<UInt32>(log2(school));
		UInt32 baseResi = getUInt32Value(unit_fields::Resistances + resiOffset);
		UInt32 casterLevel = attackerLevel;
		UInt32 victimLevel = getLevel();
		float levelBasedResistance = 0.0f;
		if (victimLevel > casterLevel)
		{
			levelBasedResistance = (victimLevel - casterLevel) * 5.0f;
		}
		float effectiveResistance = levelBasedResistance + baseResi - std::min(spellPen, baseResi);
		if (isBinary)
		{
			float reductionPct = (effectiveResistance / (casterLevel * 5.0f)) * 75.0f;
			if (resiDistribution(randomGenerator) > reductionPct)
			{
				return 0.0f;
			}
			else
			{
				return 100.0f;
			}
		}
		else
		{
			return 0.0f;
		}
	}

	float GameUnit::getCritChance(GameUnit &attacker, UInt8 school)
	{
		return 10.0f;
	}

	UInt32 GameUnit::getBonus(UInt8 school)
	{
		UInt32 bonus = 0;
		if (isGameCharacter())
		{
			bonus = getUInt32Value(character_fields::ModDamageDonePos + log2(school));
		}
		return bonus;
	}

	UInt32 GameUnit::getBonusPct(UInt8 school)
	{
		return 0;
	}

	UInt32 GameUnit::consumeAbsorb(UInt32 damage, UInt8 school)
	{
		return m_auras.consumeAbsorb(damage, school);
	}

	UInt32 GameUnit::calculateArmorReducedDamage(UInt32 attackerLevel, UInt32 damage)
	{
		UInt32 newDamage = 0;
		float armor = float(this->getUInt32Value(unit_fields::Resistances));

		// TODO: Armor reduction mods

		// Cap armor
		if (armor < 0.0f) {
			armor = 0.0f;
		}

		float tmp = 0.0f;
		if (attackerLevel < 60)
		{
			tmp = armor / (armor + 400.0f + 85.0f * attackerLevel);
		}
		else if (attackerLevel < 70)
		{
			tmp = armor / (armor - 22167.5f + 467.5f * attackerLevel);
		}
		else
		{
			tmp = armor / (armor + 10557.5f);
		}

		// Hard caps
		if (tmp < 0.0f) {
			tmp = 0.0f;
		}
		if (tmp > 0.75f) {
			tmp = 0.75f;
		}

		newDamage = UInt32(damage - (damage * tmp));
		return (newDamage > 1 ? newDamage : 1);
	}

	void GameUnit::onKilled(GameUnit *killer)
	{
		cancelCast(game::spell_interrupt_flags::None);

		// Stop auto attack
		stopAttack();
		setVictim(nullptr);
		stopRegeneration();

		// No longer in combat
		removeFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);

		m_auras.handleTargetDeath();

		if (killer)
		{
			// Check if this target should reward honor points / xp
			UInt32 killerGreyLevel = xp::getGrayLevel(killer->getLevel());
			if (getLevel() > killerGreyLevel)
			{
				killer->procKilledTarget(*this);
			}
		}
	}

	float GameUnit::getMeleeReach() const
	{
		float reach = getFloatValue(unit_fields::CombatReach);
		return reach > 2.0f ? reach : 2.0f;
	}

	void GameUnit::onSpellCastEnded(bool succeeded)
	{
		// Check if we need to trigger auto attack again
		if (m_victim)
		{
			m_lastMainHand = m_lastOffHand = getCurrentTime();
			triggerNextAutoAttack();
		}
	}

	void GameUnit::triggerNextAutoAttack()
	{
		// We can't attack when stunned or feared!
		if (isStunned() || isFeared())
			return;

		// Start auto attack timer (immediatly start to attack our target)
		GameTime now = getCurrentTime();
		GameTime nextAttackSwing = getCurrentTime();
		GameTime mainHandCooldown = m_lastMainHand + getUInt32Value(unit_fields::BaseAttackTime);
		GameTime offHandCooldown = hasOffHandWeapon() ? m_lastOffHand + getUInt32Value(unit_fields::BaseAttackTime + 1) : 0;
		if (offHandCooldown > 0)
		{
			// Choose next attack type
			m_weaponAttack = mainHandCooldown < offHandCooldown ? game::weapon_attack::BaseAttack : game::weapon_attack::OffhandAttack;
			GameTime cd = mainHandCooldown < offHandCooldown ? mainHandCooldown : offHandCooldown;
			if (cd > nextAttackSwing)
			{
				nextAttackSwing = cd;
			}
		}
		else
		{
			m_weaponAttack = game::weapon_attack::BaseAttack;
			if (mainHandCooldown > nextAttackSwing)
			{
				nextAttackSwing = mainHandCooldown;
			}
		}

		if (nextAttackSwing < now + 200)
		{
			nextAttackSwing += 200;
		}

		// Trigger next auto attack
		m_attackSwingCountdown.setEnd(nextAttackSwing);
	}

	void GameUnit::triggerNextFearMove()
	{
		// Stop when not feared
		if (!isFeared() && !isConfused())
			return;

		const float dist =
			isFeared() ? 25.0f : 2.5f;
		const math::Vector3 loc =
			isFeared() ? getMover().getCurrentLocation() : m_confusedLoc;

		auto *world = getWorldInstance();
		if (world)
		{
			auto *mapData = world->getMapData();
			if (mapData)
			{
				math::Vector3 targetPoint;
				if (mapData->getRandomPointOnGround(loc, dist, targetPoint))
				{
					getMover().moveTo(targetPoint);
					return;
				}
			}
		}
	}

	void GameUnit::setAttackSwingCallback(AttackSwingCallback callback)
	{
		m_swingCallback = std::move(callback);
	}

	const String &GameUnit::getName() const
	{
		static const String name = "UNNAMED";
		return name;
	}

	const proto::FactionTemplateEntry &GameUnit::getFactionTemplate() const
	{
		assert(m_factionTemplate);
		return *m_factionTemplate;
	}

	void GameUnit::setFactionTemplate(const proto::FactionTemplateEntry &faction)
	{
		m_factionTemplate = &faction;
		setUInt32Value(unit_fields::FactionTemplate, m_factionTemplate->id());

		factionChanged(*this);
	}

	bool GameUnit::isHostileToPlayers()
	{
		const UInt32 factionMaskPlayer = 1;
		return (m_factionTemplate->enemymask() & factionMaskPlayer) != 0;
	}

	bool GameUnit::isNeutralToAll()
	{
		return (m_factionTemplate->enemymask() == 0 && m_factionTemplate->friendmask() == 0 && m_factionTemplate->enemies().empty());
	}

	bool GameUnit::isFriendlyTo(const proto::FactionTemplateEntry &faction)
	{
		if (m_factionTemplate->id() == faction.id())
		{
			return true;
		}

		for (int i = 0; i < m_factionTemplate->enemies_size(); ++i)
		{
			const auto &enemy = m_factionTemplate->enemies(i);
			if (enemy && enemy == faction.faction())
			{
				return false;
			}
		}

		for (int i = 0; i < m_factionTemplate->friends_size(); ++i)
		{
			const auto &friendly = m_factionTemplate->friends(i);
			if (friendly && friendly == faction.faction())
			{
				return true;
			}
		}

		return ((m_factionTemplate->friendmask() & faction.selfmask()) != 0) || ((m_factionTemplate->selfmask() & faction.friendmask()) != 0);
	}

	bool GameUnit::isFriendlyTo(GameUnit &unit)
	{
		return isFriendlyTo(unit.getFactionTemplate());
	}

	bool GameUnit::isHostileTo(const proto::FactionTemplateEntry &faction)
	{
		if (m_factionTemplate->id() == faction.id())
		{
			return false;
		}

		for (int i = 0; i < m_factionTemplate->enemies_size(); ++i)
		{
			const auto &enemy = m_factionTemplate->enemies(i);
			if (enemy && enemy == faction.faction())
			{
				return true;
			}
		}

		for (int i = 0; i < m_factionTemplate->friends_size(); ++i)
		{
			const auto &friendly = m_factionTemplate->friends(i);
			if (friendly && friendly == faction.faction())
			{
				return false;
			}
		}

		return ((m_factionTemplate->enemymask() & faction.selfmask()) != 0) || ((m_factionTemplate->selfmask() & faction.enemymask()) != 0);
	}

	bool GameUnit::isHostileTo(GameUnit &unit)
	{
		return isHostileTo(unit.getFactionTemplate());
	}

	bool GameUnit::isInCombat() const
	{
		return ((getUInt32Value(unit_fields::UnitFlags) & game::unit_flags::InCombat) != 0);
	}

	bool GameUnit::isInLineOfSight(GameObject &other)
	{
		// TODO: Determine unit's height based on unit model for correct line of sight calculation
		return isInLineOfSight(other.getLocation() + math::Vector3(0.0f, 0.0f, 2.0f));
	}

	bool GameUnit::isInLineOfSight(const math::Vector3 &position)
	{
		if (!m_worldInstance) {
			return false;
		}

		auto *map = m_worldInstance->getMapData();
		if (!map) {
			return false;
		}

		return map->isInLineOfSight(m_position + math::Vector3(0.0f, 0.0f, 2.0f), position);
	}

	void GameUnit::addAttackingUnit(GameUnit &attacker)
	{
		// Add attacking unit to the list of attackers
		assert(!m_attackingUnits.contains(&attacker));
		m_attackingUnits.add(&attacker);

		// If wasn't in combat, remove auras which should be removed when entering combat
		if (!isInCombat())
		{
			m_auras.removeAllAurasDueToInterrupt(game::spell_aura_interrupt_flags::EnterCombat);
		}

		// Flag us for combat
		addFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);
	}

	void GameUnit::removeAttackingUnit(GameUnit &removed)
	{
		// Remove attacking unit
		assert(m_attackingUnits.contains(&removed));
		m_attackingUnits.remove(&removed);

		// Remove combat flag for player characters if no attacking unit is left
		if (isGameCharacter())
		{
			// Maybe not the best idea to do a cast here... but works
			GameCharacter* character = reinterpret_cast<GameCharacter*>(this);
			if (!character->isInPvPCombat())
			{
				if (m_attackingUnits.empty())
				{
					removeFlag(unit_fields::UnitFlags, game::unit_flags::InCombat);
				}
			}
		}
	}

	bool GameUnit::hasAttackingUnits() const
	{
		return !m_attackingUnits.empty();
	}

	io::Writer &operator<<(io::Writer &w, GameUnit const &object)
	{
		w
		        << reinterpret_cast<GameObject const &>(object);

		// Write spell cooldowns
		{
			// Counter placeholder (active cooldown count will be determined)
			const size_t cooldownCountPos = w.sink().position();
			w
			        << io::write<NetUInt32>(0);

			// Write cooldown data of active cooldowns and count
			UInt32 activeCooldownCount = 0;
			auto t = getCurrentTime();
			for (const auto &cooldown : object.m_spellCooldowns)
			{
				if (cooldown.second > t)
				{
					activeCooldownCount++;
					w
					        << io::write<NetUInt32>(cooldown.first)
					        << io::write<NetUInt32>(cooldown.second);
				}
			}

			// Overwrite active cooldown count
			w.sink().overwrite(cooldownCountPos, reinterpret_cast<const char *>(&activeCooldownCount), sizeof(UInt32));
		}

		return w;
	}

	io::Reader &operator>>(io::Reader &r, GameUnit &object)
	{
		// Read values
		r
		        >> reinterpret_cast<GameObject &>(object);

		// Read spell cooldowns
		{
			UInt32 cooldownCount = 0;
			r >> io::read<NetUInt32>(cooldownCount);

			for (UInt32 i = 0; i < cooldownCount; ++i)
			{
				UInt32 spellId = 0, endTime = 0;
				r
				        >> io::read<NetUInt32>(spellId)
				        >> io::read<NetUInt32>(endTime);
				object.m_spellCooldowns[spellId] = endTime;
			}
		}

		// Update internals based on received values
		object.raceUpdated();
		object.classUpdated();
		object.updateDisplayIds();

		return r;
	}

	void GameUnit::notifyStunChanged()
	{
		const bool wasStunned = isStunned();
		const bool isStunned = m_auras.hasAura(game::aura_type::ModStun);
		if (isStunned)
		{
			m_state |= unit_state::Stunned;
		}
		else
		{
			m_state &= ~unit_state::Stunned;
		}

		if (wasStunned && !isStunned)
		{
			unitStateChanged(unit_state::Stunned, false);
		}
		else if (!wasStunned && isStunned)
		{
			cancelCast(game::spell_interrupt_flags::None);
			stopAttack();
			m_mover->stopMovement();

			unitStateChanged(unit_state::Stunned, true);
		}
	}

	void GameUnit::notifyRootChanged()
	{
		const bool wasRooted = isRooted();
		const bool isRooted = m_auras.hasAura(game::aura_type::ModRoot);
		if (isRooted)
		{
			m_state |= unit_state::Rooted;
		}
		else
		{
			m_state &= ~unit_state::Rooted;
		}

		if (wasRooted && !isRooted)
		{
			unitStateChanged(unit_state::Rooted, false);
		}
		else if (!wasRooted && isRooted)
		{
			m_mover->stopMovement();
			unitStateChanged(unit_state::Rooted, true);
		}
	}

	void GameUnit::notifyFearChanged()
	{
		const bool wasFeared = isFeared();
		const bool isFeared = m_auras.hasAura(game::aura_type::ModFear);
		if (isFeared)
		{
			m_state |= unit_state::Feared;
		}
		else
		{
			m_state &= ~unit_state::Feared;
		}

		if (wasFeared && !isFeared)
		{
			unitStateChanged(unit_state::Feared, false);

			// Stop movement in every case
			getMover().stopMovement();
			m_fearMoved.disconnect();

		}
		else if (!wasFeared && isFeared)
		{
			m_mover->stopMovement();
			unitStateChanged(unit_state::Feared, true);

			// Stop attacking (just in case)
			stopAttack();
			setVictim(nullptr);

			// Stop movement in every case
			getMover().stopMovement();

			m_fearMoved = getMover().targetReached.connect(std::bind(&GameUnit::triggerNextFearMove, this));
			triggerNextFearMove();

		}
	}

	void GameUnit::notifyConfusedChanged()
	{
		const bool wasFeared = isFeared();
		const bool isFeared = m_auras.hasAura(game::aura_type::ModConfuse);
		if (isFeared)
		{
			m_state |= unit_state::Confused;
		}
		else
		{
			m_state &= ~unit_state::Confused;
		}

		if (wasFeared && !isFeared)
		{
			unitStateChanged(unit_state::Confused, false);

			// Stop movement in every case
			getMover().stopMovement();
			m_fearMoved.disconnect();
		}
		else if (!wasFeared && isFeared)
		{
			m_confusedLoc = getMover().getCurrentLocation();

			m_mover->stopMovement();
			unitStateChanged(unit_state::Confused, true);

			// Stop attacking (just in case)
			stopAttack();
			setVictim(nullptr);

			// Stop movement in every case
			getMover().stopMovement();

			m_fearMoved = getMover().targetReached.connect(std::bind(&GameUnit::triggerNextFearMove, this));
			triggerNextFearMove();
		}
	}

	void GameUnit::notifySpeedChanged(MovementType type)
	{
		const float oldBonus = m_speedBonus[type];

		float speed = 1.0f;
		bool mounted = isMounted();

		// Apply speed buffs
		{
			Int32 mainSpeedMod = 0;
			float stackBonus = 1.0f, nonStackBonus = 1.0f;
			switch (type)
			{
			case movement_type::Run:
				if (mounted) {
					mainSpeedMod = m_auras.getMaximumBasePoints(game::aura_type::ModIncreaseMountedSpeed);
				}
				else {
					mainSpeedMod = m_auras.getMaximumBasePoints(game::aura_type::ModIncreaseSpeed);
				}
				stackBonus = m_auras.getTotalMultiplier(game::aura_type::ModSpeedAlways);
				nonStackBonus = (100.0f + static_cast<float>(m_auras.getMaximumBasePoints(game::aura_type::ModSpeedNotStack))) / 100.0f;
				break;
			case movement_type::Swim:
				mainSpeedMod = m_auras.getMaximumBasePoints(game::aura_type::ModIncreaseSwimSpeed);
				break;
			case movement_type::Flight:
				mainSpeedMod = m_auras.getMaximumBasePoints(game::aura_type::ModFlightSpeed);
				stackBonus = m_auras.getTotalMultiplier(game::aura_type::ModFlightSpeedStacking);
				nonStackBonus = (100.0f + static_cast<float>(m_auras.getMaximumBasePoints(game::aura_type::ModFlightSpeedNotStacking))) / 100.0f;
				break;
			default:
				break;
			}

			float bonus = nonStackBonus > stackBonus ? nonStackBonus : stackBonus;
			speed = mainSpeedMod ? bonus * (100.0f + static_cast<float>(mainSpeedMod)) / 100.0f : bonus;
		}

		// Apply slow buffs
		{
			Int32 slow = m_auras.getMinimumBasePoints(game::aura_type::ModDecreaseSpeed);
			Int32 slowNonStack = m_auras.getMinimumBasePoints(game::aura_type::ModSpeedNotStack);
			slow = slow < slowNonStack ? slow : slowNonStack;

			// Slow has to be <= 0
			assert(slow <= 0);
			if (slow)
			{
				speed += (speed * static_cast<float>(slow) / 100.0f);
			}
		}

		if (oldBonus != speed)
		{
			// Now store the speed bonus value
			m_speedBonus[type] = speed;

			// Send packets to all listeners around
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::changeSpeed(packet, type, getGuid(), speed * getBaseSpeed(type));

			// Notify all tile subscribers about this event
			TileIndex2D tileIndex;
			if (getTileIndex(tileIndex))
			{
				forEachSubscriberInSight(
				    m_worldInstance->getGrid(),
				    tileIndex,
				    [&packet, &buffer](ITileSubscriber & subscriber)
				{
					subscriber.sendPacket(packet, buffer);
				});
			}

			// Notify the unit mover about this change
			m_mover->onMoveSpeedChanged(type);

			// Raise signal
			speedChanged(type);
		}
	}

	float GameUnit::getSpeed(MovementType type) const
	{
		const float baseSpeed = getBaseSpeed(type);
		return baseSpeed * m_speedBonus[type];
	}

	float GameUnit::getBaseSpeed(MovementType type) const
	{
		switch (type)
		{
		case movement_type::Walk:
			return 2.5f;
		case movement_type::Run:
			return 7.0f;
		case movement_type::Backwards:
			return 4.5f;
		case movement_type::Swim:
			return 4.75f;
		case movement_type::SwimBackwards:
			return 2.5f;
		case movement_type::Turn:
			return 3.1415927f;
		case movement_type::Flight:
			return 7.0f;
		case movement_type::FlightBackwards:
			return 4.5f;
		default:
			return 0.0f;
		}
	}

	void GameUnit::addMechanicImmunity(UInt32 mechanic)
	{
		m_mechanicImmunity |= mechanic;
	}

	void GameUnit::removeMechanicImmunity(UInt32 mechanic)
	{
		m_mechanicImmunity &= ~mechanic;
	}

	bool GameUnit::isImmuneAgainstMechanic(UInt32 mechanic) const
	{
		if (mechanic == 0)
			return false;

		if (m_mechanicImmunity == 0)
			return false;

		return (m_mechanicImmunity & (1 << mechanic)) != 0;
	}
}
