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

#include "game_unit.h"
#include "log/default_log_levels.h"
#include "world_instance.h"
#include "each_tile_in_sight.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include "game_character.h"
#include <cassert>

namespace wowpp
{
	GameUnit::GameUnit(
		TimerQueue &timers,
		DataLoadContext::GetRace getRace,
		DataLoadContext::GetClass getClass,
		DataLoadContext::GetLevel getLevel,
		DataLoadContext::GetSpell getSpell)
		: GameObject()
		, m_timers(timers)
		, m_getRace(getRace)
		, m_getClass(getClass)
		, m_getLevel(getLevel)
		, m_getSpell(getSpell)
		, m_raceEntry(nullptr)
		, m_classEntry(nullptr)
		, m_despawnCountdown(timers)
		, m_victim(nullptr)
		, m_attackSwingCountdown(timers)
		, m_lastAttackSwing(0)
		, m_regenCountdown(timers)
		, m_lastManaUse(0)
		, m_auras(*this)
	{
		// Resize values field
		m_values.resize(unit_fields::UnitFieldCount);
		m_valueBitset.resize((unit_fields::UnitFieldCount + 31) / 32);

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
		setUInt32Value(unit_fields::BoundingRadius, 0x3e54fdf4);		//UNIT_FIELD_BOUNDINGRADIUS		(TODO: Float)
		setUInt32Value(unit_fields::CombatReach, 0xf3c00000);			//UNIT_FIELD_COMBATREACH		(TODO: Float)
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
	}

	void GameUnit::raceUpdated()
	{
		m_raceEntry = m_getRace(getRace());
		assert(m_raceEntry);

		// Update faction template
		setUInt32Value(unit_fields::FactionTemplate, m_raceEntry->factionID);	//UNIT_FIELD_FACTIONTEMPLATE
	}

	void GameUnit::classUpdated()
	{
		m_classEntry = m_getClass(getClass());
		assert(m_classEntry);

		// Update power type
		setByteValue(unit_fields::Bytes0, 3, m_classEntry->powerType);

		// Unknown what this does...
		if (m_classEntry->powerType == power_type::Rage ||
			m_classEntry->powerType == power_type::Mana)
		{
			setByteValue(unit_fields::Bytes1, 1, 0xEE);
		}
		else
		{
			setByteValue(unit_fields::Bytes1, 1, 0x00);
		}
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
			setUInt32Value(unit_fields::DisplayId, m_raceEntry->maleModel);
			setUInt32Value(unit_fields::NativeDisplayId, m_raceEntry->maleModel);
		}
		else
		{
			setUInt32Value(unit_fields::DisplayId, m_raceEntry->femaleModel);
			setUInt32Value(unit_fields::NativeDisplayId, m_raceEntry->femaleModel);
		}
	}

	void GameUnit::setLevel(UInt8 level)
	{
		UInt32 prevLevel = getLevel();
		setUInt32Value(unit_fields::Level, level);

		// Get level information
		const auto *levelInfo = m_getLevel(level);
		if (!levelInfo) return;	// Creatures can have a level higher than 70
		
		// Level info changed
		levelChanged(*levelInfo);

		// Get old level information
		const auto *oldLevel = m_getLevel(prevLevel);
		if (oldLevel)
		{
			const auto raceIt = levelInfo->stats.find(getRace());
			if (raceIt == levelInfo->stats.end()) return;
			const auto classIt = raceIt->second.find(getClass());
			if (classIt == raceIt->second.end()) return;

			const auto raceItOld = oldLevel->stats.find(getRace());
			if (raceItOld == oldLevel->stats.end()) return;
			const auto classItOld = raceItOld->second.find(getClass());
			if (classItOld == raceItOld->second.end()) return;

			// Calculate difference
			const LevelEntry::StatArray &oldStats = classItOld->second;
			const LevelEntry::StatArray &newStats = classIt->second;

			auto &levelBaseValues = getClassEntry()->levelBaseValues;
			auto oldBase = levelBaseValues.find(prevLevel);
			auto newBase = levelBaseValues.find(level);
			if (oldBase == levelBaseValues.end() || newBase == levelBaseValues.end())
				return;

			// Fire signal
			levelGained(
				prevLevel,
				newBase->second.health - oldBase->second.health,
				newBase->second.mana - oldBase->second.mana,
				newStats[0] - oldStats[0],
				newStats[1] - oldStats[1],
				newStats[2] - oldStats[2],
				newStats[3] - oldStats[3],
				newStats[4] - oldStats[4]);
		}
	}

	void GameUnit::levelChanged(const LevelEntry &levelInfo)
	{

		// Get race and class
		const auto race = getRace();
		const auto cls = getClass();

		const auto raceIt = levelInfo.stats.find(race);
		if (raceIt == levelInfo.stats.end()) return;

		const auto classIt = raceIt->second.find(cls);
		if (classIt == raceIt->second.end()) return;

		// Update stats based on level information
		const LevelEntry::StatArray &stats = classIt->second;
		for (UInt32 i = 0; i < stats.size(); ++i)
		{
			const UnitMods mod = getUnitModByStat(i);
			setModifierValue(mod, unit_mod_type::BaseValue, stats[i]);
		}
	}

	void GameUnit::castSpell(SpellTargetMap target, UInt32 spellId, GameTime castTime, const SpellSuccessCallback &callback)
	{
		// Resolve spell
		const auto *spell = m_getSpell(spellId);
		if (!spell)
		{
			return;
		}

		auto result = m_spellCast->startCast(*spell, std::move(target), castTime, false);
		if (callback)
		{
			callback(result.first);
		}

		// Reset auto attack timer if requested
		if (result.first == game::spell_cast_result::CastOkay &&
			m_attackSwingCountdown.running &&
			result.second != nullptr)
		{
			if (!(spell->attributesEx[0] & spell_attributes_ex_a::NotResetSwingTimer))
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
	}

	void GameUnit::onDespawnTimer()
	{
		if (m_worldInstance)
		{
			m_worldInstance->removeGameObject(*this);
		}
	}

	void GameUnit::triggerDespawnTimer(GameTime despawnDelay)
	{
		// Start despawn countdown (may override previous countdown)
		m_despawnCountdown.setEnd(
			getCurrentTime() + despawnDelay);
	}

	void GameUnit::cancelCast()
	{
		// Stop attack swing callback in case there is one
		if (m_swingCallback) m_swingCallback = AttackSwingCallback();
		m_spellCast->stopCast();
	}

	void GameUnit::startAttack(GameUnit &target)
	{
		// Check if we already are attacking that unit...
		if (m_victim && m_victim == &target)
		{
			return;
		}

		// Check if the target is alive
		if (!target.isAlive())
		{
			autoAttackError(attack_swing_error::TargetDead);
			return;
		}

		// Target victim
		setUInt64Value(unit_fields::Target, target.getGuid());

		TileIndex2D tileIndex;
		if (!getTileIndex(tileIndex))
		{
			// We can't attack since we do not belong to a world
			return;
		}
		
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket packet(sink);
		game::server_write::attackStart(packet, getGuid(), target.getGuid());

		// Notify all tile subscribers about this event
		forEachSubscriberInSight(
			m_worldInstance->getGrid(),
			tileIndex,
			[&packet, &buffer](ITileSubscriber &subscriber)
		{
			subscriber.sendPacket(packet, buffer);
		});

		// Update victim
		m_victim = &target;
		m_victimDied = target.killed.connect(
			std::bind(&GameUnit::onVictimKilled, this, std::placeholders::_1));
		m_victimDespawned = target.despawned.connect(
			std::bind(&GameUnit::onVictimDespawned, this));

		// Start auto attack timer (immediatly start to attack our target)
		GameTime nextAttackSwing = getCurrentTime();
		GameTime attackSwingCooldown = m_lastAttackSwing + getUInt32Value(unit_fields::BaseAttackTime);
		if (attackSwingCooldown > nextAttackSwing) nextAttackSwing = attackSwingCooldown;

		// Trigger next auto attack
		m_attackSwingCountdown.setEnd(nextAttackSwing);
	}

	void GameUnit::stopAttack()
	{
		// Check if we are attacking any victim right now
		if (!m_victim)
		{
			return;
		}

		// Untarget victim
		setUInt64Value(unit_fields::Target, 0);

		// Get victim guid
		UInt64 victimGUID = m_victim->getGuid();

		// Stop auto attack countdown
		m_attackSwingCountdown.cancel();

		// No longer listen to these events
		m_victimDespawned.disconnect();
		m_victimDied.disconnect();

		// Reset victim
		m_victim = nullptr;

		TileIndex2D tileIndex;
		if (!getTileIndex(tileIndex))
		{
			return;
		}

		// Reset onSwing callback
		if (m_swingCallback)
		{
			m_swingCallback = AttackSwingCallback();
		}

		// Notify all subscribers
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket packet(sink);
		game::server_write::attackStop(packet, getGuid(), victimGUID);

		// Notify all tile subscribers about this event
		forEachSubscriberInSight(
			m_worldInstance->getGrid(),
			tileIndex,
			[&packet, &buffer](ITileSubscriber &subscriber)
		{
			subscriber.sendPacket(packet, buffer);
		});
	}

	void GameUnit::onAttackSwing()
	{
		// Check if we still have a victim
		if (!m_victim)
		{
			return;
		}

		if (!isAlive())
		{
			return;
		}

		if (getTypeId() != object_type::Character)
		{
			// Turn to target if npc
			setOrientation(getAngle(*m_victim));
		}

		// Remember this weapon swing
		m_lastAttackSwing = getCurrentTime();

		// Used to jump to next weapon swing trigger
		do
		{
			// Get target location
			float vX, vY, vZ, vO;
			m_victim->getLocation(vX, vY, vZ, vO);

			// Distance check
			const float distance = getDistanceTo(*m_victim);
			const float combatRange = getMeleeReach() + m_victim->getMeleeReach();
			if (distance > combatRange)
			{
				autoAttackError(attack_swing_error::OutOfRange);

				// Reset attack swing callback
				if (m_swingCallback)
				{
					m_swingCallback = AttackSwingCallback();
				}

				// Don't reset auto attack
				GameTime nextAutoAttack = m_lastAttackSwing + (constants::OneSecond / 2);
				m_attackSwingCountdown.setEnd(nextAutoAttack);
				return;
			}

			// Check if that target is in front of us
			if (!isInArc(2.0f * 3.1415927f / 3.0f, vX, vY))
			{
				autoAttackError(attack_swing_error::WrongFacing);
				
				// Reset attack swing callback
				if (m_swingCallback)
				{
					m_swingCallback = AttackSwingCallback();
				}

				// Don't reset auto attack
				GameTime nextAutoAttack = m_lastAttackSwing + (constants::OneSecond / 2);
				m_attackSwingCountdown.setEnd(nextAutoAttack);
				return;
			}

			if (m_swingCallback)
			{
				// Execute onSwing callback (used by some melee spells which are executed on next attack swing and 
				// replace auto attack)
				m_swingCallback();

				// Reset attack swing callback
				m_swingCallback = AttackSwingCallback();
			}
			else
			{
				TileIndex2D tileIndex;
				if (!getTileIndex(tileIndex))
				{
					return;
				}

				game::HitInfo hitInfo = game::hit_info::NormalSwing2;

				// Calculate damage between minimum and maximum damage
				std::uniform_real_distribution<float> distribution(getFloatValue(unit_fields::MinDamage), getFloatValue(unit_fields::MaxDamage) + 1.0f);
				const UInt32 damage = calculateArmorReducedDamage(getLevel(), *m_victim, UInt32(distribution(randomGenerator)));

				// Notify all subscribers
				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				game::Protocol::OutgoingPacket packet(sink);
				game::server_write::attackStateUpdate(packet, getGuid(), m_victim->getGuid(), hitInfo, damage, 0, 0, 0, game::victim_state::Normal, game::weapon_attack::BaseAttack, 1);

				// Notify all tile subscribers about this event
				forEachSubscriberInSight(
					m_worldInstance->getGrid(),
					tileIndex,
					[&packet, &buffer](ITileSubscriber &subscriber)
				{
					subscriber.sendPacket(packet, buffer);
				});

				// Check if we need to give rage
				if (getByteValue(unit_fields::Bytes0, 3) == power_type::Rage)
				{
					const float weaponSpeedHitFactor = (getUInt32Value(unit_fields::BaseAttackTime) / 1000.0f) * 3.5f;
					const UInt32 level = getLevel();
					const float rageconversion = ((0.0091107836f * level * level) + 3.225598133f * level) + 4.2652911f;
					float addRage = ((damage / rageconversion * 7.5f + weaponSpeedHitFactor) / 2.0f) * 10.0f;

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

				// Deal damage
				m_victim->dealDamage(damage, 0, this);
			}
		} while (false);

		// Notify about successful auto attack to reset last error message
		autoAttackError(attack_swing_error::Success);

		// Trigger next auto attack swing
		GameTime nextAutoAttack = m_lastAttackSwing + getUInt32Value(unit_fields::BaseAttackTime);
		m_attackSwingCountdown.setEnd(nextAutoAttack);
	}

	void GameUnit::onVictimKilled(GameUnit *killer)
	{
		// Stop attacking our target
		stopAttack();
	}

	void GameUnit::onVictimDespawned()
	{
		// Stop attacking our target
		stopAttack();
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
		if (getUInt32Value(unit_fields::Health) == 0)
		{
			return;
		}

		//TODO: Do this only while not in combat
		if (!m_attackSwingCountdown.running)
		{
			regenerateHealth();
			if (!m_auras.hasAura(game::aura_type::InterruptRegen))
			{
				regeneratePower(power_type::Rage);
			}
		}

		regeneratePower(power_type::Energy);
		regeneratePower(power_type::Mana);

		// Restart regeneration timer
		startRegeneration();
	}

	void GameUnit::regenerateHealth()
	{
		// TODO
	}

	void GameUnit::regeneratePower(PowerType power)
	{
		UInt32 current = getUInt32Value(unit_fields::Power1 + static_cast<Int8>(power));
		UInt32 max = getUInt32Value(unit_fields::MaxPower1 + static_cast<Int8>(power));

		float addPower = 0.0f;
		switch (power)
		{
			case power_type::Mana:
			{
				if (getTypeId() == object_type::Character)
				{
					if ((m_lastManaUse + constants::OneSecond * 5) < getCurrentTime())
					{
						addPower = getFloatValue(character_fields::ModManaRegen) * 2.0f;
					}
					else
					{
						addPower = getFloatValue(character_fields::ModManaRegenInterrupt) * 2.0f;
					}
				}
				else
				{
					// TODO: Unit mana reg
				}
				break;
			}

			case power_type::Energy:
			{
				// 20 energy per tick
				addPower = 20.0f;
				break;
			}

			case power_type::Rage:
			{
				// Take 3 rage per tick
				addPower = 30.0f;
				break;
			}

			default:
				break;
		}

		if (power != power_type::Rage)
		{
			current += UInt32(addPower);
			if (current > max) current = max;
		}
		else
		{
			if (current <= UInt32(addPower))
			{
				current = 0;
			}
			else
			{
				current -= UInt32(addPower);
			}
		}

		setUInt32Value(unit_fields::Power1 + static_cast<Int8>(power), current);
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
		for (size_t i = 0; i < power_type::Happiness; ++i)
		{
			updateMaxPower(static_cast<PowerType>(i));
		}
		for (UInt8 i = 0; i < 6; ++i)
		{
			updateResistance(i);
		}

		updateManaRegen();
	}

	void GameUnit::updateMaxHealth()
	{
		float value = getUInt32Value(unit_fields::BaseHealth);
		value += getHealthBonusFromStamina();

		setUInt32Value(unit_fields::MaxHealth, UInt32(value));
	}

	void GameUnit::updateMaxPower(PowerType power)
	{
		UInt32 createPower = 0;
		switch (power)
		{
		case power_type::Mana:		createPower = getUInt32Value(unit_fields::BaseMana); break;
		case power_type::Rage:		createPower = 1000; break;
		case power_type::Energy:    createPower = 100; break;
		default:	// Make compiler happy
			break;
		}

		float powerBonus = (power == power_type::Mana && createPower > 0) ? getManaBonusFromIntellect() : 0;

		float value = createPower;
		value += powerBonus;

		setUInt32Value(unit_fields::MaxPower1 + static_cast<UInt16>(power), UInt32(value));
	}

	void GameUnit::updateArmor()
	{
		Int32 baseArmor = getModifierValue(unit_mods::Armor, unit_mod_type::BaseValue);
		Int32 totalArmor = getModifierValue(unit_mods::Armor, unit_mod_type::TotalValue);

		// Add armor from agility
		baseArmor += getUInt32Value(unit_fields::Stat1) * 2;
		baseArmor += totalArmor;

		if (baseArmor < 0) baseArmor = 0;

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
				if (amount == -100.0f)
					amount = -99.99f;
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
		if (stat > 4)
			return;

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
			updateMaxPower(power_type::Mana);
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
		case unit_mods::StatStrength:	stat = 0;	break;
		case unit_mods::StatAgility:	stat = 1;	break;
		case unit_mods::StatStamina:	stat = 2;	break;
		case unit_mods::StatIntellect:	stat = 3;	break;
		case unit_mods::StatSpirit:		stat = 4;	break;

		default:
			break;
		}

		return stat;
	}

	wowpp::PowerType GameUnit::getPowerTypeByUnitMod(UnitMods mod)
	{
		switch (mod)
		{
		case unit_mods::Rage:		return power_type::Rage;
		case unit_mods::Focus:		return power_type::Focus;
		case unit_mods::Energy:		return power_type::Energy;
		case unit_mods::Happiness:	return power_type::Happiness;

		default:
			break;
		}

		return power_type::Mana;
	}

	wowpp::UnitMods GameUnit::getUnitModByStat(UInt8 stat)
	{
		switch (stat)
		{
		case 1:		return unit_mods::StatAgility;
		case 2:		return unit_mods::StatStamina;
		case 3:		return unit_mods::StatIntellect;
		case 4:		return unit_mods::StatSpirit;

		default:
			break;
		}

		return unit_mods::StatStrength;
	}

	wowpp::UnitMods GameUnit::getUnitModByPower(PowerType power)
	{
		switch (power)
		{
		case power_type::Rage:		return unit_mods::Rage;
		case power_type::Energy:	return unit_mods::Energy;
		case power_type::Happiness:	return unit_mods::Happiness;
		case power_type::Focus:		return unit_mods::Focus;
			
		default:
			break;
		}

		return unit_mods::Mana;
	}

	wowpp::UInt8 GameUnit::getResistanceByUnitMod(UnitMods mod)
	{
		switch (mod)
		{
		case unit_mods::ResistanceHoly:		return 1;
		case unit_mods::ResistanceFire:		return 2;
		case unit_mods::ResistanceNature:	return 3;
		case unit_mods::ResistanceFrost:	return 4;
		case unit_mods::ResistanceShadow:	return 5;
		case unit_mods::ResistanceArcane:	return 6;

		default:
			break;
		}

		return 0;
	}

	wowpp::UnitMods GameUnit::getUnitModByResistance(UInt8 res)
	{
		switch (res)
		{
		case 0:		return unit_mods::Armor;
		case 1:		return unit_mods::ResistanceHoly;
		case 2:		return unit_mods::ResistanceFire;
		case 3:		return unit_mods::ResistanceNature;
		case 4:		return unit_mods::ResistanceFrost;
		case 5:		return unit_mods::ResistanceShadow;
		case 6:		return unit_mods::ResistanceArcane;

		default:
			break;
		}

		return unit_mods::Armor;
	}
	
	void GameUnit::dealDamage(UInt32 damage, UInt32 school, GameUnit *attacker, bool noThreat/* = false*/)
	{
		UInt32 health = getUInt32Value(unit_fields::Health);
		if (health == 0)
		{
			return;
		}

		if (health < damage)
			health = 0;
		else
			health -= damage;

		setUInt32Value(unit_fields::Health, health);

		if (health == 0)
		{
			// Call function and signal
			onKilled(attacker);
			killed(attacker);
		}
		else
		{
			// Add threat
			if (attacker && !noThreat)
			{
				addThreat(*attacker, static_cast<float>(damage));
			}
		}
	}

	void GameUnit::heal(UInt32 amount, GameUnit *healer, bool noThreat /*= false*/)
	{
		UInt32 health = getUInt32Value(unit_fields::Health);
		if (health == 0)
		{
			return;
		}

		const UInt32 maxHealth = getUInt32Value(unit_fields::MaxHealth);
		const UInt32 healed = std::min(amount, maxHealth - health);
		if (health + amount >= maxHealth)
			health = maxHealth;
		else
			health += amount;
		setUInt32Value(unit_fields::Health, health);

		if (healer && !noThreat)
		{
			// TODO: Add threat to all units who are in fight with the healed target, but only
			// if the units are not friendly towards the healer!
		}
	}

	void GameUnit::revive(UInt32 health, UInt32 mana)
	{
		if (isAlive())
			return;

		const UInt32 maxHealth = getUInt32Value(unit_fields::MaxHealth);
		if (health > maxHealth) health = maxHealth;
		setUInt32Value(unit_fields::Health, health);

		if (mana > 0)
		{
			const UInt32 maxMana = getUInt32Value(unit_fields::MaxPower1);
			if (mana > maxMana) mana = maxMana;
			setUInt32Value(unit_fields::Power1, mana);
		}

		startRegeneration();
	}

	void GameUnit::rewardExperience(GameUnit *victim, UInt32 experience)
	{
		// Nothing to do here
	}
	
	bool GameUnit::isImmune(UInt8 school)
	{
		// TODO: check auras and mobtype
		return false;
	}

	void GameUnit::onKilled(GameUnit *killer)
	{
		// Stop auto attack
		stopAttack();

		m_auras.handleTargetDeath();
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
			m_lastAttackSwing = getCurrentTime();
			GameTime nextAttackSwing = m_lastAttackSwing + getUInt32Value(unit_fields::BaseAttackTime);
			m_attackSwingCountdown.setEnd(nextAttackSwing);
		}
	}

	void GameUnit::setAttackSwingCallback(AttackSwingCallback callback)
	{
		m_swingCallback = std::move(callback);
	}

	const String & GameUnit::getName() const
	{
		static const String name = "UNNAMED";
		return name;
	}

	void GameUnit::addThreat(GameUnit &threatening, float threat)
	{
		// Nothing to do here...
	}

	void GameUnit::resetThreat()
	{

	}

	void GameUnit::resetThreat(GameUnit &threatening)
	{

	}

	io::Writer & operator<<(io::Writer &w, GameUnit const& object)
	{
		w << reinterpret_cast<GameObject const&>(object);

		return w;
	}

	io::Reader & operator>>(io::Reader &r, GameUnit& object)
	{
		// Read values
		r
			>> reinterpret_cast<GameObject &>(object);

		// Update internals based on received values
		object.raceUpdated();
		object.classUpdated();
		object.updateDisplayIds();

		return r;
	}

	UInt32 calculateArmorReducedDamage(UInt32 attackerLevel, const GameUnit &victim, UInt32 damage)
	{
		UInt32 newDamage = 0;
		float armor = float(victim.getUInt32Value(unit_fields::Resistances));

		// TODO: Armor reduction mods

		// Cap armor
		if (armor < 0.0f) armor = 0.0f;

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
		if (tmp < 0.0f) tmp = 0.0f;
		if (tmp > 0.75f) tmp = 0.75f;

		newDamage = UInt32(damage - (damage * tmp));
		return (newDamage > 1 ? newDamage : 1);
	}

}
