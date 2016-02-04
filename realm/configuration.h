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

#include "simple_file_format/sff_write_table.h"
#include "common/typedefs.h"

namespace wowpp
{
	/// Manages the realm server configuration.
	struct Configuration
	{
		/// Config file version: used to detect new configuration files
		static const UInt32 RealmConfigVersion;

		/// The port to be used by the login server for realms to log in.
		NetPort loginPort;
		/// Maximum number of player connections.
		size_t maxPlayers;
		/// IP/Domain of the login server.
		String loginAddress;

		/// The port to be used by the realm server for worlds to log in.
		NetPort worldPort;
		/// Maximum number of world node connections.
		size_t maxWorlds;

		/// The internal name of this realm used to log in at the login server.
		String internalName;
		/// The password to log in at the login server.
		String password;
		/// The display name of this realm at the login server.
		String visibleName;
		/// The ip address/dns used by game clients to log in.
		String playerHost;
		/// The port to be used by game clients to log in.
		NetPort playerPort;

		/// Path to the client data
		String dataPath;

		/// The port to be used for a mysql connection.
		NetPort mysqlPort;
		/// The mysql server host address (ip or dns).
		String mysqlHost;
		/// The mysql user to be used.
		String mysqlUser;
		/// The mysql user password to be used.
		String mysqlPassword;
		/// The mysql database to be used.
		String mysqlDatabase;

		/// Indicates whether or not file logging is enabled.
		bool isLogActive;
		/// File name of the log file.
		String logFileName;
		/// If enabled, the log contents will be buffered before they are written to
		/// the file, which could be more efficient..
		bool isLogFileBuffering;

		/// Message of the day which will be displayed to all players which enter the world.
		String messageOfTheDay;

		/// The port to be used for a web connection.
		NetPort webPort;
		/// The port to be used for an ssl web connection.
		NetPort webSSLPort;
		/// The user name of the web user.
		String webUser;
		/// The password for the web user.
		String webPassword;

		/// Initializes a new instance of the Configuration class using the default
		/// values.
		explicit Configuration();
		
		/// Loads the configuration values from a specific file.
		/// @param fileName Name of the configuration file.
		/// @returns true if the config file was loaded successfully, false otherwise.
		bool load(const String &fileName);
		/// Saves the configuration values to a specific file. Overwrites the whole
		/// file!
		/// @param fileName Name of the configuration file.
		/// @returns true if the config file was saved successfully, false otherwise.
		bool save(const String &fileName);
	};
}
