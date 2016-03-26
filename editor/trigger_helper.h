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
#include "proto_data/project.h"
#include "proto_data/trigger_helper.h"
#include <QString>

namespace wowpp
{
	namespace editor
	{
		QString getTriggerEventText(const proto::TriggerEvent &e, bool link = false);

		QString getTriggerTargetName(const proto::TriggerAction &action, bool link = false);

		QString getTriggerActionString(const proto::TriggerAction &action, UInt32 i, bool link = false);

		QString getTriggerActionData(const proto::TriggerAction &action, UInt32 i, bool link = false);

		QString getTriggerActionText(const proto::Project &project, const proto::TriggerAction &action, bool withLinks = false);

		template<class T>
		static QString actionDataEntry(const T& manager, const proto::TriggerAction &action, UInt32 i, bool link = false)
		{
			QString temp = (link ? "<a href=\"data-%2\" style=\"color: #ffae00;\">%1</a>" : "%1");

			Int32 data = (static_cast<Int32>(i) >= action.data_size() ? 0 : action.data(i));
			const auto *entry = manager.getById(data);
			if (!entry)
				return temp.arg("(INVALID)").arg(i);

			return temp.arg(QString("(%1)").arg(entry->name().c_str())).arg(i);
		}
	}
}