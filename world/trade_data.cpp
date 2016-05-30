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
#include "player.h"
#include "game/game_world_object.h"


namespace wowpp
{	
	TradeData::TradeData(Player *player, Player *trader)
		: m_player(player)
		, m_trader(trader)
		, m_gold(0)
		, m_accepted(false)
	{
		m_items.resize(7);
	}

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
		m_gold = money;
	}

	void TradeData::setTradeAcceptState(bool state)
	{
		m_accepted = state;
	}

	UInt32 TradeData::getGold()
	{
		return m_gold;
	}

	bool TradeData::isAccepted()
	{
		return m_accepted;
	}

	void TradeData::setItem(std::shared_ptr<GameItem> item, UInt16 slot)
	{
		// Replace item at slot
		m_items[slot] = item;
	}

	/*std::vector<Item_Data> TradeData::getItem()
	{
		return(m_item_data);
	}*/
}
