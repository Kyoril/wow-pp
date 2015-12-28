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

#include "templates/basic_template.h"
#include "game/defines.h"
#include "class_entry.h"

namespace wowpp
{
	namespace spell_attributes
	{
		enum Type
		{
			/// Unknown currently
			Unknown_0 = 0x00000001,
			/// Spell requires ammo.
			Ranged = 0x00000002,
			/// Spell is executed on next weapon swing.
			OnNextSwing = 0x00000004,
			/// 
			IsReplenishment = 0x00000008,
			/// Spell is a player ability.
			Ability = 0x00000010,
			/// 
			TradeSpell = 0x00000020,
			/// Spell is a passive spell-
			Passive = 0x00000040,
			/// Spell does not appear in the players spell book.
			HiddenClientSide = 0x00000080,
			/// Spell won't display cast time.
			HiddenCastTime = 0x00000100,
			/// Client will automatically target the mainhand item.
			TargetMainhandItem = 0x00000200,
			/// 
			OnNextSwing_2 = 0x00000400,
			/// 
			Unknown_4 = 0x00000800,
			/// Spell is only executable at day.
			DaytimeOnly = 0x00001000,
			/// Spell is only executable at night
			NightOnly = 0x00002000,
			/// Spell is only executable while indoor.
			IndoorOnly = 0x00004000,
			/// Spell is only executable while outdoor.
			OutdoorOnly = 0x00008000,
			/// Spell is only executable while not shape shifted.
			NotShapeshifted = 0x00010000,
			/// Spell is only executable while in stealth mode.
			OnlyStealthed = 0x00020000,
			/// Spell does not change the players sheath state.
			DontAffectSheathState = 0x00040000,
			/// 
			LevelDamageCalc = 0x00080000,
			/// Spell will stop auto attack.
			StopAttackTarget = 0x00100000,
			/// Spell can't be blocked / dodged / parried
			NoDefense = 0x00200000,
			/// Executer will always look at target while casting this spell.
			CastTrackTarget = 0x00400000,
			/// Spell is usable while caster is dead.
			CastableWhileDead = 0x00800000,
			/// Spell is usable while caster is mounted.
			CastableWhileMounted = 0x01000000,
			/// 
			DisabledWhileActive = 0x02000000,
			/// 
			Negative = 0x04000000,
			/// Cast is usable while caster is sitting.
			CastableWhileSitting = 0x08000000,
			/// Cast is not usable while caster is in combat.
			NotInCombat = 0x10000000,
			/// Spell is usable even on invulnerable targets.
			IgnoreInvulnerability = 0x20000000,
			/// Aura of this spell will break on damage.
			BreakableByDamage = 0x40000000,
			/// Aura can't be cancelled by player.
			CantCancel = 0x80000000
		};
	}

	typedef spell_attributes::Type SpellAttributes;

	namespace spell_attributes_ex_a
	{
		enum Type
		{
			/// 
			DismissPet = 0x00000001,
			/// 
			DrainAllPower = 0x00000002,
			/// 
			Channeled_1 = 0x00000004,
			/// 
			CantBeRedirected = 0x00000008,
			/// 
			Unknown_1 = 0x00000010,
			/// 
			NotBreakStealth = 0x00000020,
			/// 
			Channeled_2 = 0x00000040,
			/// 
			CantBeReflected = 0x00000080,
			/// 
			TargetNotInCombat = 0x00000100,
			/// 
			MeleeCombatStart = 0x00000200,
			/// 
			NoThreat = 0x00000400,
			/// 
			Unknown_3 = 0x00000800,
			/// 
			PickPocket = 0x00001000,
			/// 
			FarSight = 0x00002000,
			/// 
			ChannelTrackTarget = 0x00004000,
			/// 
			DispelAurasOnImmunity = 0x00008000,
			/// 
			UnaffectedByImmunity = 0x00010000,
			/// 
			NoPetAutoCast = 0x00020000,
			/// 
			Unknown_5 = 0x00040000,
			/// 
			CantTargetSelf = 0x00080000,
			/// 
			ReqComboPoints_1 = 0x00100000,
			/// 
			Unknown_7 = 0x00200000,
			/// 
			ReqComboPoints_2 = 0x00400000,
			/// 
			Unknown_8 = 0x00800000,
			/// 
			IsFishing = 0x01000000,
			/// 
			Unknown_10 = 0x02000000,
			/// 
			Unknown_11 = 0x04000000,
			/// 
			NotResetSwingTimer = 0x08000000,
			/// 
			DontDisplayInAuraBar = 0x10000000,
			/// 
			ChannelDisplaySpellName = 0x20000000,
			/// 
			EnableAtDodge = 0x40000000,
			/// 
			Unknown_16 = 0x80000000
		};
	}

	typedef spell_attributes_ex_a::Type SpellAttributesExA;

	namespace spell_interrupt_flags
	{
		enum Type
		{
			None = 0x00,
			Movement = 0x01,
			PushBack = 0x02,
			Interrupt = 0x04,
			AutoAttack = 0x08,
			Damage = 0x10
		};
	}

	typedef spell_interrupt_flags::Type SpellInterruptFlags;

	namespace spell_channel_interrupt_flags
	{
		enum Type
		{
			None = 0x0000,
			Damage = 0x0002,
			Movement = 0x0008,
			Turning = 0x0010,
			Damage2 = 0x0080,
			Delay = 0x4000
		};
	};

	typedef spell_channel_interrupt_flags::Type SpellChannelInterruptFlags;

	namespace spell_aura_interrupt_flags
	{
		enum Type
		{
			None = 0x0000000,
			/// Removed when getting hit by a negative spell.
			HitBySpell = 0x00000001,
			/// Removed by any damage.
			Damage = 0x00000002,
			/// Removed on crowd control effect.
			CrowdControl = 0x00000004,
			/// Removed by any movement.
			Move = 0x00000008,
			/// Removed by any turning.
			Turning = 0x00000010,
			/// Removed by entering combat.
			EnterCombat = 0x00000020,
			/// Removed by unmounting.
			NotMounted = 0x00000040,
			/// Removed by entering water (start swimming).
			NotAboveWater = 0x00000080,
			/// Removed by leaving water.
			NotUnderWater = 0x00000100,
			/// Removed by unsheathing.
			NotSheathed = 0x00000200,
			/// Removed when talking to an npc or loot a creature.
			Talk = 0x00000400,
			/// Removed when mining/using/opening/interact with game object.
			Use = 0x00000800,
			/// Removed by attacking.
			Attack = 0x00001000,
			/// TODO
			Cast = 0x00002000,
			/// TODO
			Unknown_14 = 0x00004000,
			/// Removed on transformation.
			Transform = 0x00008000,
			/// TODO
			Unknown_16 = 0x00010000,
			/// Removed when mounting.
			Mount = 0x00020000,
			/// Removed when standing up.
			NotSeated = 0x00040000,
			/// Removed when leaving the map.
			ChangeMap = 0x00080000,
			/// TODO
			Unattackable = 0x00100000,
			/// TODO
			Unknown_21 = 0x00200000,
			/// Removed when teleported.
			Teleported = 0x00400000,
			/// Removed by entering pvp combat.
			EnterPvPCombat = 0x00800000,
			/// Removed by any direct damage.
			DirectDamage = 0x01000000,
			/// TODO
			NotVictim = (HitBySpell | Damage | DirectDamage)
		};
	}

	typedef spell_aura_interrupt_flags::Type SpellAuraInterruptFlags;

	struct DataLoadContext;
	struct SkillEntry;
	struct UnitEntry;

	struct SpellEntry : BasicTemplate<UInt32>
	{
		/// Represents data needed by a spell effect.
		struct Effect
		{
			UInt8 index;
			game::SpellEffect type;
			Int32 basePoints;
			Int32 dieSides;
			Int32 baseDice;
			float dicePerLevel;
			float pointsPerLevel;
			UInt32 mechanic;
			UInt32 targetA;
			UInt32 targetB;
			float radius;
			UInt32 auraName;
			Int32 amplitude;
			float multipleValue;
			UInt32 chainTarget;
			UInt32 itemType;
			Int32 miscValueA;
			Int32 miscValueB;
			const SpellEntry *triggerSpell;
			const UnitEntry *summonEntry;
			Int32 pointsPerComboPoint;

			Effect()
				: index(0)
				, type(game::spell_effects::Invalid_)
				, basePoints(0)
				, dieSides(0)
				, baseDice(0)
				, dicePerLevel(0.0f)
				, pointsPerLevel(0.0f)
				, mechanic(0)
				, targetA(0)
				, targetB(0)
				, radius(0.0f)
				, auraName(0)
				, amplitude(0)
				, multipleValue(0.0f)
				, chainTarget(0)
				, itemType(0)
				, miscValueA(0)
				, miscValueB(0)
				, triggerSpell(nullptr)
				, summonEntry(nullptr)
				, pointsPerComboPoint(0)
			{
			}
		};

		typedef BasicTemplate<UInt32> Super;
		typedef std::vector<Effect> SpellEffects;

		UInt32 attributes;
		std::array<UInt32, 6> attributesEx;
		String name;
		SpellEffects effects;
		GameTime cooldown;
		UInt32 castTimeIndex;
		PowerType powerType;
		UInt32 cost;
		UInt32 costPct;
		Int32 maxLevel;
		Int32 baseLevel;
		Int32 spellLevel;
		float speed;
		UInt32 schoolMask;
		UInt32 dmgClass;
		Int32 itemClass;
		UInt32 itemSubClassMask;
		std::vector<const SkillEntry*> skillsOnLearnSpell;
		UInt32 facing;
		Int32 duration;
		Int32 maxDuration;
		UInt32 interruptFlags;
		UInt32 channelInterruptFlags;
		UInt32 auraInterruptFlags;
		float minRange;
		float maxRange;
		UInt32 rangeType;
		UInt32 targetMap;
		float targetX, targetY, targetZ, targetO;
		UInt32 maxTargets;
		UInt32 talentCost;		// This value will be dynamically set during load of talent entries
		UInt32 procFlags;
		UInt32 procChance;
		UInt32 procCharges;

		SpellEntry();
		bool load(DataLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
