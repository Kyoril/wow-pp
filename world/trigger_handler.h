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

#include "data/trigger_entry.h"
#include "common/timer_queue.h"
#include "common/countdown.h"
#include <memory>

namespace wowpp
{
	class GameUnit;
	class Project;
	class PlayerManager;
	class WorldInstance;

	/// 
	class TriggerHandler final
	{
	public:

		/// 
		explicit TriggerHandler(Project &project, PlayerManager &playerManager, TimerQueue &timers);

		/// Fires a trigger event.
		void executeTrigger(const TriggerEntry &entry, UInt32 actionOffset = 0, GameUnit *owner = nullptr);

	private:

	private:

		UInt32 getActionData(const TriggerEntry::TriggerAction &action, UInt32 index) const;
		const String &getActionText(const TriggerEntry::TriggerAction &action, UInt32 index) const;
		WorldInstance *getWorldInstance(GameUnit *owner) const;
		GameUnit *getUnitTarget(const TriggerEntry::TriggerAction &action, GameUnit *owner);

		void handleTrigger(const TriggerEntry::TriggerAction &action, GameUnit *owner);
		void handleSay(const TriggerEntry::TriggerAction &action, GameUnit *owner);
		void handleYell(const TriggerEntry::TriggerAction &action, GameUnit *owner);
		void handleSetWorldObjectState(const TriggerEntry::TriggerAction &action, GameUnit *owner);
		void handleSetSpawnState(const TriggerEntry::TriggerAction &action, GameUnit *owner);
		void handleSetRespawnState(const TriggerEntry::TriggerAction &action, GameUnit *owner);
		void handleCastSpell(const TriggerEntry::TriggerAction &action, GameUnit *owner);

	private:

		Project &m_project;
		PlayerManager &m_playerManager;
		TimerQueue &m_timers;
		std::vector<std::unique_ptr<Countdown>> m_delays;
	};
}
