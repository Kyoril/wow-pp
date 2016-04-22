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

namespace wowpp
{
	class Player;

	/// Manages all connected players.
	class PlayerManager : public boost::noncopyable
	{
	public:

		typedef std::vector<std::unique_ptr<Player>> Players;

	public:

		/// Initializes a new instance of the player manager class.
		/// @param playerCapacity The maximum number of connections that can be connected at the same time.
		explicit PlayerManager(
		    size_t playerCapacity
		);
		~PlayerManager();

		/// Notifies the manager that a player has been disconnected which will
		/// delete the player instance.
		void playerDisconnected(Player &player);
		/// Gets a dynamic array containing all connected player instances.
		const Players &getPlayers() const;
		/// Determines whether the player capacity limit has been reached.
		bool hasPlayerCapacityBeenReached() const;
		/// Adds a new player instance to the manager.
		void addPlayer(std::unique_ptr<Player> added);
		/// Gets a player by his account name.
		Player *getPlayerByAccountName(const String &accountName);
		/// 
		Player *getPlayerByAccountID(UInt32 accountId);

	private:

		Players m_players;
		size_t m_playerCapacity;
	};
}
