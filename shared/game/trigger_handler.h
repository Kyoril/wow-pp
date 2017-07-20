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

#include "common/typedefs.h"

namespace wowpp
{
	class GameObject;
	class GameUnit;
	namespace proto
	{
		class TriggerEntry;
	}

	namespace game
	{
		struct TriggerContext
		{
			/// Owner of the trigger or nullptr if none.
			GameObject *owner;
			/// Id of the spell that triggered this trigger with it's hit.
			UInt32 spellHitId;
			/// Unit that raised this trigger by raising an event or nullptr if not suitable for this trigger event.
			std::weak_ptr<GameUnit> triggeringUnit;

			explicit TriggerContext(GameObject *owner_, GameUnit* triggering);
		};

		struct ITriggerHandler
		{
		private:

			ITriggerHandler(const ITriggerHandler &Other) = delete;
			ITriggerHandler &operator=(const ITriggerHandler &) = delete;

		public:

			ITriggerHandler() {};
			virtual ~ITriggerHandler() {};

			/// Executes a unit trigger.
			/// @param entry The trigger to execute.
			/// @param actionOffset The action to execute.
			/// @param owner The executing owner. TODO: Replace by context object
			virtual void executeTrigger(const proto::TriggerEntry &entry, TriggerContext context, UInt32 actionOffset = 0, bool ignoreProbability = false) = 0;
		};
	}
}
