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

#include "pch.h"
#include "trade_data.h"
#include "game/game_creature.h"
#include "Player.h"
#include "game/game_world_object.h"


namespace wowpp
{
	TradeData::TradeData() {}
	TradeData::TradeData(Player *_player, Player *_trader) :
	m_player(_player),
	m_trader(_trader),
	gold(0) {}

	Player* TradeData::getPlayer()
	{
		return m_player;
	}

	Player* TradeData::getTrader()
	{
		return m_trader;
	}

	void TradeData::setGold(UInt32 money)
	{
		gold = money;
	}

	void TradeData::setacceptTrade(bool state)
	{
		acceptTrade = state;
	}

	UInt32 TradeData::getGold()
	{
		return gold;
	}

	bool TradeData::isAccepted()
	{
		return acceptTrade;
	}

	void TradeData::setItem(const proto::ItemEntry &item, UInt16 slot, UInt64 guid_value, UInt32 stack_count)
	{
		/*Item_Data item_data;
		int _slot = slot + 1;
		item_data.m_item = item;
		item_data.m_stack_count = stack_count;
		item_data.m_guid_value = guid_value;
		m_item_data.insert(m_item_data.begin() + _slot, item_data);*/
	}
	/*std::vector<Item_Data> TradeData::getItem()
	{
		return(m_item_data);
	}*/
}
