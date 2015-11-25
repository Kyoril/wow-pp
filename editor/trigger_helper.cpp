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

#include "trigger_helper.h"

namespace wowpp
{
	namespace editor
	{
		QString getTriggerEventText(UInt32 e)
		{
			switch (e)
			{
			case trigger_event::OnAggro:
				return "Triggering unit enters combat";
			case trigger_event::OnAttackSwing:
				return "Triggering unit executes auto attack swing";
			case trigger_event::OnDamaged:
				return "Triggering unit received damage";
			case trigger_event::OnDespawn:
				return "Triggering object despawned";
			case trigger_event::OnHealed:
				return "Triggering unit received heal";
			case trigger_event::OnKill:
				return "Triggering unit killed someone";
			case trigger_event::OnKilled:
				return "Triggering unit was killed";
			case trigger_event::OnSpawn:
				return "Triggering object spawned";
			case trigger_event::OnReset:
				return "Triggering unit resets";
			case trigger_event::OnReachedHome:
				return "Triggering unit reached home after reset";
			default:
				return "(INVALID EVENT)";
			}
		}

		QString getTriggerTargetName(const TriggerEntry::TriggerAction &action, bool link/* = false*/)
		{
			QString temp = (link ? "<a href=\"target\" style=\"color: #ffae00;\">%1</a>" : "%1");

			switch (action.target)
			{
			case trigger_action_target::None:
				return temp.arg("(NONE)");
			case trigger_action_target::OwningUnit:
				return temp.arg("(Triggering Unit)");
			case trigger_action_target::OwningUnitVictim:
				return temp.arg("(Triggering Unit's Target)");
			case trigger_action_target::RandomUnit:
				return temp.arg("(Random Nearby Unit)");
			case trigger_action_target::NamedCreature:
				return temp.arg(QString("(Creature Named '%1')").arg(action.targetName.c_str()));
			case trigger_action_target::NamedWorldObject:
				return temp.arg(QString("(Object Named '%1')").arg(action.targetName.c_str()));
			default:
				return temp.arg("(INVALID)");
			}
		}

		QString getTriggerActionString(const TriggerEntry::TriggerAction &action, UInt32 i, bool link/* = false*/)
		{
			QString temp = (link ? "<a href=\"text-%2\" style=\"color: #ffae00;\">%1</a>" : "%1");

			if (i >= action.texts.size())
				return temp.arg("(INVALID TEXT)").arg(i);

			return temp.arg(action.texts[i].c_str()).arg(i);
		}

		QString getTriggerActionText(const Project &project, const TriggerEntry::TriggerAction &action, bool withLinks/* = false*/)
		{
			switch (action.action)
			{
			case trigger_actions::Say:
				return QString("Unit - Make %1 say %2 and play sound %3")
					.arg(getTriggerTargetName(action, withLinks)).arg(getTriggerActionString(action, 0, withLinks)).arg(getTriggerActionData(action, 0, withLinks));
			case trigger_actions::Yell:
				return QString("Unit - Make %1 yell %2 and play sound %3")
					.arg(getTriggerTargetName(action, withLinks)).arg(getTriggerActionString(action, 0, withLinks)).arg(getTriggerActionData(action, 0, withLinks));
			case trigger_actions::CastSpell:
				return QString("Unit - Make %1 cast spell %2 on %3")
					.arg(getTriggerTargetName(action, withLinks)).arg(actionDataEntry(project.spells, action, 0, withLinks).arg(getTriggerTargetName(action, withLinks)));
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
			default:
				return QString("UNKNOWN TRIGGER ACTION");
			}
		}

		QString getTriggerActionData(const TriggerEntry::TriggerAction &action, UInt32 i, bool link/* = false*/)
		{
			QString temp = (link ? "<a href=\"data-%2\" style=\"color: #ffae00;\">%1</a>" : "%1");

			if (i >= action.data.size())
				return temp.arg(0).arg(i);

			return temp.arg(action.data[i]).arg(i);
		}
	}
}
