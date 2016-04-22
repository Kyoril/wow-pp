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

namespace wowpp
{
	namespace trigger_event
	{
		enum Type
		{
			/// Executed when the unit is spawned.
			/// Data: NONE;
			OnSpawn = 0,
			/// Executed when the unit will despawn.
			/// Data: NONE;
			OnDespawn = 1,
			/// Executed when the unit enters the combat.
			/// Data: NONE;
			OnAggro = 2,
			/// Executed when the unit was killed.
			/// Data: NONE;
			OnKilled = 3,
			/// Executed when the unit killed another unit.
			/// Data: NONE;
			OnKill = 4,
			/// Executed when the unit was damaged.
			/// Data: NONE;
			OnDamaged = 5,
			/// Executed when the unit was healed.
			/// Data: NONE;
			OnHealed = 6,
			/// Executed when the unit made an auto attack swing.
			/// Data: NONE;
			OnAttackSwing = 7,
			/// Executed when the unit resets.
			/// Data: NONE;
			OnReset = 8,
			/// Executed when the unit reached it's home point after reset.
			/// Data: NONE;
			OnReachedHome = 9,
			/// Executed when a player is interacting with this object. Only works on GameObjects right now, but could also be used
			/// for npc interaction.
			/// Data: NONE;
			OnInteraction = 10,
			/// Executed when a units health drops below a certain percentage.
			/// Data: <HEALTH_PERCENTAGE:0-100>;
			OnHealthDroppedBelow = 11,
			/// Executed when a unit reaches it's target point for a move that was triggered by a trigger.
			/// Data: NONE;
			OnReachedTriggeredTarget = 12,
			/// Executed when a unit was hit by a specific spell.
			/// Data: <SPELL-ID>;
			OnSpellHit = 13,
			/// Executed when a spell aura is removed.
			/// Data: <SPELL-ID>;
			OnSpellAuraRemoved = 14,
			/// Executed when a unit is target of a specific emote.
			/// Data: <EMOTE-ID>;
			OnEmote = 15,

			Invalid,
			Count_ = Invalid
		};
	}

	namespace trigger_actions
	{
		enum Type
		{
			/// Execute another trigger.
			/// Targets: NONE; Data: <TRIGGER-ID>; Texts: NONE;
			Trigger = 0,
			/// Makes a unit say a text.
			/// Targets: UNIT; Data: <SOUND-ID>,<LANGUAGE>; Texts: <TEXT>;
			Say = 1,
			/// Makes a unit say a text.
			/// Targets: UNIT; Data: <SOUND-ID>,<LANGUAGE>; Texts: <TEXT>;
			Yell = 2,
			/// Sets the state of a world object.
			/// Targets: NAMED_OBJECT; Data: <NEW-STATE>; Texts: NONE;
			SetWorldObjectState = 3,
			/// Activates or deactivates a creature or object spawner.
			/// Targets: NAMED_CREATURE/NAMED_OBJECT; Data: <0/1>; Texts: NONE;
			SetSpawnState = 4,
			/// Activates or deactivates respawn of a creature or object spawner.
			/// Targets: NAMED_CREATURE/NAMED_OBJECT; Data: <0/1>; Texts: NONE;
			SetRespawnState = 5,
			/// Casts a spell.
			/// Targets: UNIT; Data: <SPELL-ID>; Texts: NONE;
			CastSpell = 6,
			/// Delays the following actions.
			/// Targets: NONE; Data: <DELAY-TIME-MS>; Texts: NONE;
			Delay = 7,
			/// Makes a unit move towards a specified position.
			/// Targets: UNIT; Data: <X>, <Y>, <Z>; Texts: NONE;
			MoveTo = 8,
			/// Enables or disables a units combat movement.
			/// Targets: UNIT; Data: <0/1>; Texts: NONE;
			SetCombatMovement = 9,
			/// Stops auto attacking the current victim.
			/// Targets: UNIT; Data: NONE; Texts: NONE;
			StopAutoAttack = 10,
			/// Cancels the current cast (if any).
			/// Targets: UNIT; Data: NONE; Texts: NONE;
			CancelCast = 11,
			/// Updates the target units stand state.
			/// Targets: UNIT; Data: <STAND-STATE>; Texts: NONE;
			SetStandState = 12,
			/// Updates the target units virtual equipment slot.
			/// Targets: UNIT; Data: <SLOT:0-2>, <ITEM-ENTRY>; Texts: NONE;
			SetVirtualEquipmentSlot = 13,
			/// Updates the target creatures AI combat phase.
			/// Targets: UNIT; Data: <PHASE>; Texts: NONE;
			SetPhase = 14,
			/// Sets spell cooldown for a unit.
			/// Targets: UNIT; Data: <SPELL-ID>,<TIME-MS>; Texts: NONE;
			SetSpellCooldown = 15,


			Invalid,
			Count_ = Invalid
		};
	}

	namespace trigger_variables
	{
		enum Type
		{
			/// Current creature AI phase. Defaults to 0 at combat beginning.
			/// Will be reset automatically when combat ends.
			Phase = 0,
			/// Current health as absolute number.
			Health = 1,
			/// Current health as percentage (0-100).
			HealthPct = 2,
			/// Current mana as absolute number.
			Mana = 3,
			/// Current mana as percentage (0-100).
			ManaPct = 4,
			/// Whether the unit is in combat (0-1).
			IsInCombat = 5,

			Invalid,
			Count_ = Invalid
		};
	}

	namespace trigger_operator
	{
		enum Type
		{
			Equal = 0,
			Not = 1,
			Lesser = 2,
			Greater = 3,
			LesserEqual = 4,
			GreaterEqual = 5,

			Invalid,
			Count_ = Invalid
		};
	}

	namespace trigger_action_target
	{
		enum Type
		{
			/// No target. May be invalid for some actions.
			None = 0,
			/// Unit which owns this trigger. May be invalid for some triggers.
			OwningObject = 1,
			/// Current victim of the unit which owns this trigger. May be invalid.
			OwningUnitVictim = 2,
			/// Random unit in the map instance.
			RandomUnit = 3,
			/// Named world object.
			NamedWorldObject = 4,
			/// Named creature.
			NamedCreature = 5,

			Invalid,
			Count_ = Invalid
		};
	}

	namespace trigger_spell_cast_target
	{
		enum Type
		{
			/// Target is the casting unit.
			Caster = 0,
			/// Target is the casting units target. Will fail if the unit does not have a current target.
			CurrentTarget = 1,
			
			Invalid,
			Count_ = Invalid
		};
	}
}
