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

#include "faction_template_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"
#include "log/default_log_levels.h"
#include "faction_entry.h"

namespace wowpp
{
	FactionTemplateEntry::FactionTemplateEntry()
		: Super()
		, flags(0)
		, selfMask(0)
		, friendMask(0)
		, enemyMask(0)
		, faction(nullptr)
	{
	}

	bool FactionTemplateEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		UInt32 factionId = 0;
		if (!wrapper.table.tryGetInteger("faction", factionId) || factionId == 0)
		{
			std::ostringstream strm;
			strm << "No valid faction id provided for faction template " << id;
			context.onError(strm.str());
			return false;
		}

		faction = context.getFaction(factionId);
		if (!faction)
		{
			context.onError("Could not find provided faction referenced in faction template");
			return false;
		}

		wrapper.table.tryGetInteger("flags", flags);
		wrapper.table.tryGetInteger("self_mask", selfMask);
		wrapper.table.tryGetInteger("friend_mask", friendMask);
		wrapper.table.tryGetInteger("hostile_mask", enemyMask);

		// Read friend factions
		if (const sff::read::tree::Array<DataFileIterator> *const friendArray = wrapper.table.getArray("friend_factions"))
		{
			friends.resize(friendArray->getSize(), nullptr);

			for (size_t j = 0; j < friendArray->getSize(); ++j)
			{
				UInt32 friendFaction = friendArray->getInteger(j, 0);
				if (friendFaction == 0)
				{
					context.onError("Friend faction id 0 is not allowed in faction template list.");
					return false;
				}

				context.loadLater.push_back([&context, friendFaction, j, this]() -> bool
				{
					friends[j] = context.getFaction(friendFaction);
					if (!friends[j])
					{
						ELOG("Could not find faction template by id " << friendFaction);
						return false;
					}

					return true;
				});
			}
		}

		// Read enemy factions
		if (const sff::read::tree::Array<DataFileIterator> *const enemyArray = wrapper.table.getArray("enemy_factions"))
		{
			enemies.resize(enemyArray->getSize(), nullptr);

			for (size_t j = 0; j < enemyArray->getSize(); ++j)
			{
				UInt32 enemyFaction = enemyArray->getInteger(j, 0);
				if (enemyFaction == 0)
				{
					context.onError("Enemy faction id 0 is not allowed in faction template list.");
					return false;
				}

				context.loadLater.push_back([&context, enemyFaction, j, this]() -> bool
				{
					enemies[j] = context.getFaction(enemyFaction);
					if (!enemies[j])
					{
						ELOG("Could not find faction template by id " << enemyFaction);
						return false;
					}

					return true;
				});
			}
		}

		return true;
	}

	void FactionTemplateEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (faction != nullptr) context.table.addKey("faction", faction->id);
		if (flags != 0) context.table.addKey("flags", flags);
		if (selfMask != 0) context.table.addKey("self_mask", selfMask);
		if (friendMask != 0) context.table.addKey("friend_mask", friendMask);
		if (enemyMask != 0) context.table.addKey("hostile_mask", enemyMask);

		sff::write::Array<char> enemyArray(context.table, "enemy_factions", sff::write::Comma);
		{
			for (const auto &faction : enemies)
			{
				if (faction) enemyArray.addElement(faction->id);
			}
		}
		enemyArray.finish();

		sff::write::Array<char> friendArray(context.table, "friend_factions", sff::write::Comma);
		{
			for (const auto &faction : friends)
			{
				if (faction) friendArray.addElement(faction->id);
			}
		}
		friendArray.finish();
	}

	bool FactionTemplateEntry::isFriendlyTo(const FactionTemplateEntry &entry) const
	{
		if (id == entry.id)
		{
			return true;
		}

		for (const auto *enemy : enemies)
		{
			if (enemy && enemy == entry.faction)
			{
				return false;
			}
		}

		for (const auto *friendly : friends)
		{
			if (friendly && friendly == entry.faction)
			{
				return true;
			}
		}

		return ((friendMask & entry.selfMask) != 0) || ((selfMask && entry.friendMask) != 0);
	}

	bool FactionTemplateEntry::isHostileTo(const FactionTemplateEntry &entry) const
	{
		if (id == entry.id)
		{
			return false;
		}

		for (const auto *enemy : enemies)
		{
			if (enemy && enemy == entry.faction)
			{
				return true;
			}
		}

		for (const auto *friendly : friends)
		{
			if (friendly && friendly == entry.faction)
			{
				return false;
			}
		}

		return ((enemyMask & entry.selfMask) != 0) || ((selfMask && entry.enemyMask) != 0);
	}

	bool FactionTemplateEntry::isHostileToPlayers() const
	{
		const UInt32 factionMaskPlayer = 1;
		return (enemyMask & factionMaskPlayer) != 0;
	}

	bool FactionTemplateEntry::isNeutralToAll() const
	{
		return (enemyMask == 0 && friendMask == 0 && enemies.empty());
	}

}
