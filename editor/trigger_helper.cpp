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
#include "trigger_helper.h"
#include "proto_data/trigger_helper.h"

namespace wowpp
{
	namespace editor
	{
		QString getTriggerEventData(const proto::TriggerEvent &e, UInt32 i, bool link/* = false*/)
		{
			QString temp = (link ? "<a href=\"event-data-%2\" style=\"color: #ffae00;\">%1</a>" : "%1");

			if (static_cast<int>(i) >= e.data_size())
				return temp.arg(0).arg(i);

			return temp.arg(e.data(i)).arg(i);
		}

		QString getTriggerEventText(const proto::TriggerEvent &e, bool withLinks/* = false*/)
		{
			switch (e.type())
			{
			case trigger_event::OnAggro:
				return "Owning unit enters combat";
			case trigger_event::OnAttackSwing:
				return "Owning unit executes auto attack swing";
			case trigger_event::OnDamaged:
				return "Owning unit received damage";
			case trigger_event::OnDespawn:
				return "Owner despawned";
			case trigger_event::OnHealed:
				return "Owning unit received heal";
			case trigger_event::OnKill:
				return "Owning unit killed someone";
			case trigger_event::OnKilled:
				return "Owning unit was killed";
			case trigger_event::OnSpawn:
				return "Owner spawned";
			case trigger_event::OnReset:
				return "Owning unit resets";
			case trigger_event::OnReachedHome:
				return "Owning unit reached home after reset";
			case trigger_event::OnInteraction:
				return "Player interacted with owner";
			case trigger_event::OnHealthDroppedBelow:
				return QString("Owning units health dropped below %1%")
					.arg(getTriggerEventData(e, 0, withLinks));
			default:
				return "(INVALID EVENT)";
			}
		}

		QString getTriggerTargetName(const proto::TriggerAction &action, bool link/* = false*/)
		{
			QString temp = (link ? "<a href=\"target\" style=\"color: #ffae00;\">%1</a>" : "%1");

			switch (action.target())
			{
			case trigger_action_target::None:
				return temp.arg("(NONE)");
			case trigger_action_target::OwningObject:
				return temp.arg("(Owner)");
			case trigger_action_target::OwningUnitVictim:
				return temp.arg("(Owning Unit's Target)");
			case trigger_action_target::RandomUnit:
				return temp.arg("(Random Nearby Unit)");
			case trigger_action_target::NamedCreature:
				return temp.arg(QString("(Creature: '%1')").arg(action.targetname().c_str()));
			case trigger_action_target::NamedWorldObject:
				return temp.arg(QString("(World Object: '%1')").arg(action.targetname().c_str()));
			default:
				return temp.arg("(INVALID)");
			}
		}

		QString getTriggerActionString(const proto::TriggerAction &action, UInt32 i, bool link/* = false*/)
		{
			QString temp = (link ? "<a href=\"text-%2\" style=\"color: #ffae00;\">%1</a>" : "%1");

			if (static_cast<int>(i) >= action.texts_size())
				return temp.arg("(INVALID TEXT)").arg(i);

			return temp.arg(action.texts(i).c_str()).arg(i);
		}

		QString getTriggerActionText(const proto::Project &project, const proto::TriggerAction &action, bool withLinks/* = false*/)
		{
			switch (action.action())
			{
			case trigger_actions::Say:
				return QString("Unit - Make %1 say %2 and play sound %3")
					.arg(getTriggerTargetName(action, withLinks)).arg(getTriggerActionString(action, 0, withLinks)).arg(getTriggerActionData(action, 0, withLinks));
			case trigger_actions::Yell:
				return QString("Unit - Make %1 yell %2 and play sound %3")
					.arg(getTriggerTargetName(action, withLinks)).arg(getTriggerActionString(action, 0, withLinks)).arg(getTriggerActionData(action, 0, withLinks));
			case trigger_actions::CastSpell:
				return QString("Unit - Make %1 cast spell %2 on %3")
					.arg(getTriggerTargetName(action, withLinks))
					.arg(actionDataEntry(project.spells, action, 0, withLinks))
					.arg(getTriggerActionData(action, 1, withLinks));
			case trigger_actions::SetSpawnState:
				return QString("Unit - Set spawn state of %1 to %2")
					.arg(getTriggerTargetName(action, withLinks)).arg(getTriggerActionData(action, 0, withLinks));
			case trigger_actions::SetRespawnState:
				return QString("Unit - Set respawn state of %1 to %2")
					.arg(getTriggerTargetName(action, withLinks)).arg(getTriggerActionData(action, 0, withLinks));
			case trigger_actions::SetWorldObjectState:
				return QString("Object - Set state of %1 to %2")
					.arg(getTriggerTargetName(action, withLinks)).arg(getTriggerActionData(action, 0, withLinks));
			case trigger_actions::Trigger:
				return QString("Common - Execute trigger %1")
					.arg(actionDataEntry(project.triggers, action, 0, withLinks));
			case trigger_actions::Delay:
				return QString("Common - Delay execution for %1 ms")
					.arg(getTriggerActionData(action, 0, withLinks));
			case trigger_actions::MoveTo:
				return QString("Unit - Make %1 move to location (%2, %3, %4)")
					.arg(getTriggerTargetName(action, withLinks))
					.arg(getTriggerActionData(action, 0, withLinks))
					.arg(getTriggerActionData(action, 1, withLinks))
					.arg(getTriggerActionData(action, 2, withLinks))
					;
			case trigger_actions::SetCombatMovement:
				return QString("Unit - Set combat movement of %1 to (%2)")
					.arg(getTriggerTargetName(action, withLinks))
					.arg(getTriggerActionData(action, 0, withLinks));
			default:
				return QString("UNKNOWN TRIGGER ACTION");
			}
		}

		QString getTriggerActionData(const proto::TriggerAction &action, UInt32 i, bool link/* = false*/)
		{
			QString temp = (link ? "<a href=\"data-%2\" style=\"color: #ffae00;\">%1</a>" : "%1");

			if (action.action() == trigger_actions::CastSpell && i == 1)
			{
				UInt32 target = (i >= UInt32(action.data_size()) ? 0 : action.data(i));
				if (target >= trigger_spell_cast_target::Invalid)
				{
					return temp.arg("(INVALID TARGET)").arg(i);
				}
				else
				{
					static std::array<QString, trigger_spell_cast_target::Count_> entries = {
						QString("(Caster)"),
						QString("(Casters Target)")
					};

					return temp.arg(entries[target]).arg(i);
				}
			}

			if (static_cast<int>(i) >= action.data_size())
				return temp.arg(0).arg(i);

			return temp.arg(action.data(i)).arg(i);
		}
	}
}
