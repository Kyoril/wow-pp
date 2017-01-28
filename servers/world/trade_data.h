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
#include "common/simple.hpp"
#include "proto_data/project.h"

namespace wowpp
{
	class Player;
	class GameItem;

	/// This class represents a trade session between two player characters.
	class TradeData final
	{
	private:

		// Make this class noncopyable

		TradeData(const TradeData &other) = delete;
		TradeData &operator=(const TradeData &other) = delete;

	public:

		/// Maximum number of item slots per player
		static constexpr size_t MaxTradeSlots = 7;

		/// Represents a shared pointer of an item.
		typedef std::shared_ptr<GameItem> ItemPtr;
		/// Holds the item data per trade slot.
		typedef std::array<ItemPtr, MaxTradeSlots> TradeSlotItems;

		/// Holds player-specific data.
		struct PlayerData
		{
			/// Amount of gold this player wants to send to the other player.
			UInt32 gold;
			/// Items this player wants to send to the other player.
			TradeSlotItems items;
			/// Whether the player has accepted the trade.
			bool accepted;
		};

		/// Enumerates player data index.
		enum Trader
		{
			/// The initiator of this trade.
			Owner	= 0,
			/// The target of this trade.
			Target	= 1,

			/// Max number of traders
			Count_	= 2
		};

	public:

		/// Executed when the gold value of one player was changed.
		simple::signal<void()> goldChanged;
		/// Executed when an item of a player was added, removed or changed.
		simple::signal<void()> itemChanged;

	public:

		/// Initializes the trade data object by providing two player connection instances.
		/// @param player The owning player whose trade infos are saved in this object.
		/// @param trader The other player, with which the owner is trading.
		TradeData(Player &player1, Player &player2);
		
		/// Gets the initiator instance of this trade.
		Player &getInitiator() { return m_initiator; }
		/// Gets the other player instance.
		Player &getOther() { return m_other; }
		/// Opens the trade windows for both players.
		void openWindows();
		/// Cancels this trade.
		void cancel();
		/// Gets the trade data of the specific player. This method is constant, because changing any of these
		/// values has to generate packets and changes the trade state.
		/// @param index Player index.
		const PlayerData &getData(Trader index) const { assert(index < Trader::Count_); return m_data[index]; }
		/// Sets the amount of gold for the given player.
		void setGold(Trader index, UInt32 gold);
		/// Sets the accept state for the given player.
		void setAcceptedState(Trader index, bool accept);
		/// Sets an item for the given player.
		void setItem(Trader Index, UInt8 tradeSlot, ItemPtr item);

	private:

		Player &m_initiator;
		Player &m_other;
		std::array<PlayerData, Trader::Count_> m_data;
	};
}
