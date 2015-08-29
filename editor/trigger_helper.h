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

#include "common/typedefs.h"
#include "data/trigger_entry.h"
#include "data/project.h"
#include <QString>

namespace wowpp
{
	namespace editor
	{
		QString getTriggerEventText(UInt32 e);

		QString getTriggerTargetName(const TriggerEntry::TriggerAction &action, bool link = false);

		QString getTriggerActionString(const TriggerEntry::TriggerAction &action, UInt32 i, bool link = false);

		QString getTriggerActionData(const TriggerEntry::TriggerAction &action, UInt32 i, bool link = false);

		QString getTriggerActionText(const Project &project, const TriggerEntry::TriggerAction &action, bool withLinks = false);

		template<class T>
		static QString actionDataEntry(const T& manager, const TriggerEntry::TriggerAction &action, UInt32 i, bool link = false)
		{
			QString temp = (link ? "<a href=\"data-%2\" style=\"color: #ffae00;\">%1</a>" : "%1");

			Int32 data = (i >= action.data.size() ? 0 : action.data[i]);
			const auto *entry = manager.getById(data);
			if (!entry)
				return temp.arg("(INVALID)").arg(i);

			return temp.arg(QString("(%1)").arg(entry->name.c_str())).arg(i);
		}
	}
}