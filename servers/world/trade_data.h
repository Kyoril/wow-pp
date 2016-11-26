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
	class GameItem;

	/// This class manages trade data of a player.
	class TradeData
	{
	public:

		/// Initializes the trade data object by providing two player connection instances.
		/// @param player The owning player whose trade infos are saved in this object.
		/// @param trader The other player, with which the owner is trading.
		TradeData(Player *player, Player *trader);
		
		/// Gets the initiator instance of this trade.
		Player* getPlayer();
		/// Gets the other player instance.
		Player* getTrader();
		/// Updates the amount of gold of this players trade.
		/// @param money The amount of money, where 110 means 1 silver and 10 copper.
		void setGold(UInt32 money);
		/// Gets the amount of gold this player wants to trade to the other player.
		UInt32 getGold();
		/// Updates the owners accept state of this trade.
		/// @param state True if the owner accepts this trade, false otherwise.
		void setTradeAcceptState(bool state);
		/// Determines whether this trade is accepted by the owner.
		bool isAccepted();
		/// Sets a new item at a specific trade slot.
		void setItem(std::shared_ptr<GameItem> item, UInt16 slot);
		std::vector<std::shared_ptr<GameItem>> &getItem();

		///
		void setAbsSlot(UInt16 slot, UInt8 tradeSlot);
		///
		UInt16 getAbsSlot(int i) { return absSlot[i]; };
	
	private:
		Player *m_player;
		Player *m_trader;
		UInt32 m_gold;
		bool m_accepted;
		std::vector<std::shared_ptr<GameItem>> m_items;
		std::vector<UInt16> absSlot;
	};
}
