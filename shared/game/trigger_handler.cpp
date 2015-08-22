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

#include "trigger_handler.h"
#include "game_unit.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	TriggerHandler::TriggerHandler(GameUnit *owningUnit)
		: m_owningUnit(owningUnit)
	{
	}

	void TriggerHandler::executeTrigger(const TriggerEntry &entry)
	{
		for (auto &action : entry.actions)
		{
			switch (action.action)
			{
				case trigger_actions::Trigger:
				{
					handleTrigger(action);
					break;
				}
				case trigger_actions::Say:
				{
					handleSay(action);
					break;
				}
				case trigger_actions::Yell:
				{
					handleYell(action);
					break;
				}
			}
		}
	}

	void TriggerHandler::handleTrigger(const TriggerEntry::TriggerAction &action)
	{
		if (action.data.size() < 1)
		{
			WLOG("TRIGGER_ACTION_TRIGGER: Missing data");
			return;
		}

		DLOG("TODO: EXECUTE NEW TRIGGER " << action.data[0]);
	}

	void TriggerHandler::handleSay(const TriggerEntry::TriggerAction &action)
	{
		if (action.texts.size() < 1)
		{
			WLOG("TRIGGER_ACTION_SAY: Missing texts");
			return;
		}

		DLOG("TODO: SAY " << action.texts[0]);
	}

	void TriggerHandler::handleYell(const TriggerEntry::TriggerAction &action)
	{
		if (action.texts.size() < 1)
		{
			WLOG("TRIGGER_ACTION_YELL: Missing texts");
			return;
		}

		DLOG("TODO: YELL " << action.texts[0]);
	}
}
