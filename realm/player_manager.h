//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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
#include <boost/noncopyable.hpp>
#include <vector>
#include <memory>

namespace wowpp
{
	// Forwards
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
		const Players &getPlayers() const { return m_players; }
		/// Determines whether the player capacity limit has been reached.
		bool hasPlayerCapacityBeenReached() const;
		/// Adds a new player instance to the manager.
		void addPlayer(std::unique_ptr<Player> added);
		/// Gets a player by his account name.
		Player *getPlayerByAccountName(const String &accountName);
		/// Gets a player by his account name.
		Player *getPlayerByCharacterId(DatabaseId id);
		/// Gets a player by his account name.
		Player *getPlayerByCharacterGuid(UInt64 id);
		/// 
		Player *getPlayerByCharacterName(const String &name);
		
		/// Executes a function for every connected player.
		template<class F>
		void foreachPlayer(F functor)
		{
			for (auto &player : m_players)
			{
				functor(*player);
			}
		}

	private:

		Players m_players;
		size_t m_playerCapacity;
	};
}
