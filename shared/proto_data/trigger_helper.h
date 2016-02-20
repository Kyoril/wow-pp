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
			OnSpawn = 0,
			/// Executed when the unit will despawn.
			OnDespawn = 1,
			/// Executed when the unit enters the combat.
			OnAggro = 2,
			/// Executed when the unit was killed.
			OnKilled = 3,
			/// Executed when the unit killed another unit.
			OnKill = 4,
			/// Executed when the unit was damaged.
			OnDamaged = 5,
			/// Executed when the unit was healed.
			OnHealed = 6,
			/// Executed when the unit made an auto attack swing.
			OnAttackSwing = 7,
			/// Executed when the unit resets.
			OnReset = 8,
			/// Executed when the unit reached it's home point after reset.
			OnReachedHome = 9,
			/// Executed when a player is interacting with this object. Only works on GameObjects right now, but could also be used
			/// for npc interaction.
			OnInteraction = 10,

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
}
