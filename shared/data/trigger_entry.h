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

#pragma once

#include "templates/basic_template.h"

namespace wowpp
{
	namespace trigger_event
	{
		enum Type
		{
			/// Executed when the unit is spawned.
			OnSpawn				= 0,
			/// Executed when the unit will despawn.
			OnDespawn			= 1,
			/// Executed when the unit enters the combat.
			OnAggro				= 2,
			/// Executed when the unit was killed.
			OnKilled			= 3,
			/// Executed when the unit killed another unit.
			OnKill				= 4,
			/// Executed when the unit was damaged.
			OnDamaged			= 5,
			/// Executed when the unit was healed.
			OnHealed			= 6,
			/// Executed when the unit made an auto attack swing.
			OnAttackSwing		= 7,

			Invalid,
			Count_ = Invalid
		};
	}

	namespace trigger_actions
	{
		enum Type
		{
			/// Execute another trigger. Targets: NONE; Data: <TRIGGER-ID>; Texts: NONE;
			Trigger				= 0,
			/// Makes a unit say a text. Targets: UNIT; Data: <SOUND-ID>,<LANGUAGE>; Texts: <TEXT>;
			Say					= 1,
			/// Makes a unit say a text. Targets: UNIT; Data: <SOUND-ID>,<LANGUAGE>; Texts: <TEXT>;
			Yell				= 2,

			Invalid,
			Count_ = Invalid
		};
	}

	namespace trigger_action_target
	{
		enum Type
		{
			/// No target. May be invalid for some actions.
			None				= 0,
			/// Unit which owns this trigger. May be invalid for some triggers.
			OwningUnit			= 1,
			/// Current victim of the unit which owns this trigger. May be invalid.
			OwningUnitVictim	= 2,
			/// Random unit in the map instance.
			RandomUnit			= 3,

			Invalid,
			Count_ = Invalid
		};
	}

	struct TriggerEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		struct TriggerAction final
		{
			UInt32 action;
			UInt32 target;
			std::vector<String> texts;
			std::vector<Int32> data;

			TriggerAction()
				: action(trigger_actions::Count_)
				, target(trigger_action_target::None)
			{
			}
		};

		typedef std::vector<UInt32> TriggerEvents;
		typedef std::vector<TriggerAction> TriggerActions;

		String name;
		String path;
		TriggerEvents events;
		TriggerActions actions;

		TriggerEntry();
		bool load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
