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

#include "trigger_entry.h"
#include "templates/basic_template_save_context.h"
#include "common/make_unique.h"

namespace wowpp
{
	TriggerEntry::TriggerEntry()
	{
	}

	bool TriggerEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		// Load name and directory
		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetString("path", path);

		events.clear();

		const sff::read::tree::Array<DataFileIterator> *eventsArray = wrapper.table.getArray("events");
		if (eventsArray)
		{
			for (size_t j = 0, d = eventsArray->getSize(); j < d; ++j)
			{
				events.push_back(eventsArray->getInteger<Int32>(j, 0));
			}
		}

		const sff::read::tree::Array<DataFileIterator> *conditionsArray = wrapper.table.getArray("conditions");
		if (conditionsArray)
		{
			for (size_t j = 0, d = conditionsArray->getSize(); j < d; ++j)
			{
			}
		}

		actions.clear();

		const sff::read::tree::Array<DataFileIterator> *actionsArray = wrapper.table.getArray("actions");
		if (actionsArray)
		{
			for (size_t j = 0, d = actionsArray->getSize(); j < d; ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const actionTable = actionsArray->getTable(j);
				if (!actionTable)
				{
					context.onError("Invalid trigger action table");
					return false;
				}

				TriggerAction action;
				if (!actionTable->tryGetInteger("action", action.action))
				{
					context.onError("Could not find action parameter");
					return false;
				}
				if (action.action >= trigger_actions::Count_)
				{
					context.onError("Invalid trigger action parameter");
					return false;
				}

				actionTable->tryGetInteger("target", action.target);
				actionTable->tryGetString("target_name", action.targetName);

				const sff::read::tree::Array<DataFileIterator> *textsArray = actionTable->getArray("texts");
				if (textsArray)
				{
					for (size_t j = 0, d = textsArray->getSize(); j < d; ++j)
					{
						action.texts.push_back(textsArray->getString(j));
					}
				}

				const sff::read::tree::Array<DataFileIterator> *dataArray = actionTable->getArray("data");
				if (dataArray)
				{
					for (size_t j = 0, d = dataArray->getSize(); j < d; ++j)
					{
						action.data.push_back(dataArray->getInteger<Int32>(j, 0));
					}
				}

				actions.push_back(std::move(action));
			}
		}

		return true;
	}

	void TriggerEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);
		if (!path.empty()) context.table.addKey("path", path);

		if (!events.empty())
		{
			sff::write::Array<char> eventsArray(context.table, "events", sff::write::Comma);
			{
				for (auto &e : events)
				{
					eventsArray.addElement(e);
				}
			}
			eventsArray.finish();
		}

		/*
		sff::write::Array<char> conditionsArray(context.table, "conditions", sff::write::MultiLine);
		{
		}
		conditionsArray.finish();
		*/

		if (!actions.empty())
		{
			sff::write::Array<char> actionsArray(context.table, "actions", sff::write::MultiLine);
			{
				for (auto &action : actions)
				{
					sff::write::Table<char> actionTable(actionsArray, sff::write::Comma);
					
					actionTable.addKey("action", action.action);
					if (action.target != 0) actionTable.addKey("target", action.target);
					if (!action.targetName.empty()) actionTable.addKey("target_name", action.targetName);

					// NOTE: unique_ptr<sff::write::Arra<char>> is used below, since MSVC++ seems to have a bug
					// The stack gets corrupted or something, and causes m_hasMembers of the array to be true,
					// thus leading to wrong formatting. This only happens in release mode, so it seems it has 
					// something to do with optimization.

					if (!action.texts.empty())
					{
						auto actionTextArray = make_unique<sff::write::Array<char>>(actionTable, "texts", sff::write::Comma);
						for (auto &text : action.texts)
						{
							actionTextArray->addElement(text);
						}
						actionTextArray->finish();
					}

					// Data
					if (!action.data.empty())
					{
						auto actionDataArray = make_unique<sff::write::Array<char>>(actionTable, "data", sff::write::Comma);
						for (auto &data : action.data)
						{
							actionDataArray->addElement(data);
						}
						actionDataArray->finish();
					}

					actionTable.finish();
				}
			}
			actionsArray.finish();
		}
	}
}
