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

#include "faction_helper.h"
#include "common/typedefs.h"
#include "shared/proto_data/faction_templates.pb.h"

namespace wowpp
{
	bool isHostileToPlayers(const proto::FactionTemplateEntry & faction)
	{
		const UInt32 factionMaskPlayer = 1;
		return (faction.enemymask() & factionMaskPlayer) != 0;
	}

	bool isNeutralToAll(const proto::FactionTemplateEntry & faction)
	{
		return (faction.enemymask() == 0 && faction.friendmask() == 0 && faction.enemies().empty());
	}

	bool isFriendlyTo(const proto::FactionTemplateEntry & factionA, const proto::FactionTemplateEntry & factionB)
	{
		if (factionA.id() == factionB.id())
		{
			return true;
		}

		for (int i = 0; i < factionA.enemies_size(); ++i)
		{
			const auto &enemy = factionA.enemies(i);
			if (enemy && enemy == factionB.faction())
			{
				return false;
			}
		}

		for (int i = 0; i < factionA.friends_size(); ++i)
		{
			const auto &friendly = factionA.friends(i);
			if (friendly && friendly == factionB.faction())
			{
				return true;
			}
		}

		return ((factionA.friendmask() & factionB.selfmask()) != 0) || ((factionA.selfmask() & factionB.friendmask()) != 0);
	}

	bool isHostileTo(const proto::FactionTemplateEntry & factionA, const proto::FactionTemplateEntry & factionB)
	{
		if (factionA.id() == factionB.id())
		{
			return false;
		}

		for (int i = 0; i < factionA.enemies_size(); ++i)
		{
			const auto &enemy = factionA.enemies(i);
			if (enemy && enemy == factionB.faction())
			{
				return true;
			}
		}

		for (int i = 0; i < factionA.friends_size(); ++i)
		{
			const auto &friendly = factionA.friends(i);
			if (friendly && friendly == factionB.faction())
			{
				return false;
			}
		}

		return ((factionA.enemymask() & factionB.selfmask()) != 0) || ((factionA.selfmask() & factionB.enemymask()) != 0);
	}
}
