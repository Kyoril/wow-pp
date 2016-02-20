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
#include "templates/basic_template.h"

namespace wowpp
{
	struct SkillEntry;
	struct FactionTemplate;
	struct ItemEntry;

	struct QuestEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		template<typename T>
		struct DataValue
		{
			const T *entry;
			UInt32 value;

			DataValue()
				: entry(nullptr)
				, value(0)
			{
			}

			const bool isValid() const {
				return entry != nullptr;
			}
		};

		typedef DataValue<SkillEntry> SkillValue;
		typedef DataValue<FactionTemplate> RepValue;
		typedef DataValue<ItemEntry> ItemValue;

		String name;
		Int32 minLvl;
		Int32 questLvl;
		Int32 method;
		Int32 zone;
		Int32 type;
		UInt32 reqClasses;
		UInt32 reqRaces;
		SkillValue reqSkill;
		RepValue reqObj;
		RepValue reqMinRep;
		RepValue reqMaxRep;
		Int32 suggestedPlayers;
		UInt32 timeLimit;
		UInt32 questFlags;
		UInt32 specialFlags;
		UInt32 charTitle;
		const QuestEntry *prevQuest;
		const QuestEntry *nextQuest;
		UInt32 exclusiveGroup;
		const QuestEntry *nextQuestInChain;
		ItemValue srcItem;
		String details;
		String objective;
		String reward;
		String request;
		String end;

		QuestEntry();
		bool load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
