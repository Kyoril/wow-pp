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
#include "common/big_number.h"
#include "wowpp_protocol/wowpp_realm_login.h"

namespace wowpp
{
	/// Basic interface for a database system used by the login server.
	struct IDatabase
	{
		virtual ~IDatabase();

		/// Gets a players password hash from the database.
		/// @param userName User name of the player in uppercase letters.
		/// @param out_passwordHash Reference to a string where the password hash will be stored.
		/// @param out_id Reference to a UInt32 where the account identifier will be stored.
		/// @returns false if the user name does not exist or if there was a database error.
		virtual bool getPlayerPassword(const String &userName, UInt32 &out_id, String &out_passwordHash) = 0;
		/// Gets a players cached S and V values from the database.
		/// @param userId Account identifier.
		/// @param out_S Reference to a BigNumber where S will be stored.
		/// @param out_V Reference to a BigNumber where V will be stored.
		/// @returns true if the values were read successfully, false otherwise.
		virtual bool getSVFields(const UInt32 &userId, BigNumber &out_S, BigNumber &out_V) = 0;
		/// Sets a players S and V values to store in the database.
		/// @param userId Account identifier.
		/// @param S Reference to a BigNumber representing S.
		/// @param V Reference to a BigNumber representing V.
		/// @returns true if the values were written successfully, false otherwise.
		virtual bool setSVFields(const UInt32 &userId, const BigNumber &S, const BigNumber &V) = 0;
		/// Validates a realm login try and returns a result code.
		/// @param out_id Realm identifier.
		/// @param name Internal realm name used to validate if the realm is valid.
		/// @param password Password used to validate if the realm is valid.
		/// @returns Result code based on the provided name and password and the state of the realm.
		virtual pp::realm_login::LoginResult realmLogIn(UInt32 &out_id, const String &name, const String &password) = 0;
		/// Flags all realms as offline.
		/// @returns false if an error occurred.
		virtual bool setAllRealmsOffline() = 0;
		/// Flags a specific realm as online and updates parameters like it's visible name, host and port.
		/// @param id Realm identifier.
		/// @param visibleName Name of the realm as it should appear in the realm list.
		/// @param host Host of the realm used by players to connect to the realm.
		/// @param port Port at which the realm listens for players.
		/// @returns false if an error occurred.
		virtual bool setRealmOnline(UInt32 id, const String &visibleName, const String &host, NetPort port) = 0;
		/// Flags a specific realm as offline.
		/// @param id Realm identifier.
		/// @returns false if an error occurred.
		virtual bool setRealmOffline(UInt32 id) = 0;
		/// Writes the new number of players on the realm to the database.
		/// @param id Realm identifier.
		/// @param players Number of players which are logged in to the realm.
		/// @returns false if an error occurred.
		virtual bool setRealmCurrentPlayerCount(UInt32 id, size_t players) = 0;

		virtual bool getTutorialData(UInt32 id, std::array<UInt32, 8> &out_data) = 0;
		virtual bool setTutorialData(UInt32 id, const std::array<UInt32, 8> data) = 0;
	};
}
