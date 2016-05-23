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

namespace wowpp
{
	class Player;

	struct Item_Data
	{
	        Item_Data();
		UInt32 m_stack_count;
		UInt64 m_guid_value;
		proto::ItemEntry m_item;
	};

	class TradeData
	{
	public:
		TradeData();
		TradeData(Player *_player, Player *_trader); //trader = other, player = this
		
		Player* getPlayer();
		Player* getTrader();
		void setGold(UInt32 money);
		UInt32 getGold();
		void setacceptTrade(bool state);
		bool isAccepted();
		void setItem(proto::ItemEntry item, UInt16 slot, UInt64 guid_value, UInt32 stack_count);
		std::vector<Item_Data> getItem();
	
	private:
		Player *m_player;
		Player *m_trader;
		UInt32 gold;
		bool acceptTrade;
		std::vector<Item_Data> m_item_data;
	};

	
}
