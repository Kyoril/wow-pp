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

#include "quest_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	QuestEntry::QuestEntry()
		: Super()
		, minLvl(0)
		, questLvl(0)
		, method(0)
		, zone(0)
		, type(0)
		, reqClasses(0)
		, reqRaces(0)
		, suggestedPlayers(0)
		, timeLimit(0)
		, questFlags(0)
		, specialFlags(0)
		, charTitle(0)
		, prevQuest(nullptr)
		, nextQuest(nullptr)
		, exclusiveGroup(0)
		, nextQuestInChain(nullptr)
	{
	}

	bool QuestEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		wrapper.table.tryGetString("name", name);

		return true;
	}

	void QuestEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) {
			context.table.addKey("name", name);
		}
	}
}
