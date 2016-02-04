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
#include <map>

namespace wowpp
{
	/// Enumerates possible states of an action bar button.
	namespace action_button_update_state
	{
		enum Enum
		{
			/// 
			Unchanged = 0,
			/// 
			Changed = 1,
			/// 
			New = 2,
			/// 
			Deleted = 3
		};
	}

	typedef action_button_update_state::Enum ActionButtonUpdateState;

	/// Defines data of an action button.
	struct ActionButton final
	{
		UInt16 action;
		UInt8 type;
		UInt8 misc;
		ActionButtonUpdateState state;

		ActionButton()
			: action(0)
			, type(0)
			, misc(0)
			, state(action_button_update_state::New)
		{
		}
		ActionButton(UInt16 action_, UInt8 type_, UInt8 misc_)
			: action(action_)
			, type(type_)
			, misc(misc_)
			, state(action_button_update_state::New)
		{
		}
	};

	typedef std::map<UInt8, ActionButton> ActionButtons;
}
