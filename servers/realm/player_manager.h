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
	// Forwards
	class Player;
	class TimerQueue;

	/// Manages all connected players.
	class PlayerManager final
	{
	private:

		PlayerManager(const PlayerManager &Other) = delete;
		PlayerManager &operator=(const PlayerManager &Other) = delete;

	public:

		typedef std::shared_ptr<Player> PlayerPtr;
		typedef std::vector<PlayerPtr> Players;

	public:

		/// Initializes a new instance of the player manager class.
		/// @param timers Timer queue to use.
		/// @param realmID Used to convert player GUIDs for cross realm support.
		/// @param playerCapacity The maximum number of connections that can be connected at the same time.
		explicit PlayerManager(
			TimerQueue &timers,
			UInt16 realmID,
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
		void addPlayer(PlayerPtr added);
		/// Gets a player by his account name.
		PlayerPtr getPlayerByAccountName(const String &accountName);
		/// Gets a player by his character database id.
		PlayerPtr getPlayerByCharacterId(DatabaseId id);
		/// Gets a player by his character guid.
		PlayerPtr getPlayerByCharacterGuid(UInt64 id);
		/// Gets a player by his character name.
		PlayerPtr getPlayerByCharacterName(const String &name);
		/// 
		TimerQueue &getTimers() { return m_timers; }
		/// Converts a given guid to a cross-realm guid.
		void getCrossRealmGUID(UInt64 &guid);

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

		TimerQueue &m_timers;
		Players m_players;
		UInt16 m_realmID;
		size_t m_playerCapacity;
	};
}
