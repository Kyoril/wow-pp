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
#include <vector>

namespace wowpp
{
	/// Manages the world server configuration.
	struct Configuration final
	{
		/// This is a struct that stores realm connection settings for one realm
		/// the world node should connect to.
		struct RealmConfiguration final
		{
			/// The port to be used by the realm server for worlds to log in.
			NetPort realmPort;
			/// Maximum number of players this world can handle from this realm.
			size_t maxPlayers;
			/// IP/Domain of the realm server.
			String realmAddress;
			/// List of map ids hosted on this world server instance for the specified realm.
			std::vector<UInt32> hostedMaps;
		};

		/// Config file version: used to detect new configuration files
		static const UInt32 WorldConfigVersion;

		/// Path to the client data
		String dataPath;

		/// Contains all realms this world node should connect to.
		std::vector<RealmConfiguration> realms;

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

		explicit Configuration();
		bool load(const String &fileName);
		bool save(const String &fileName);
	};
}
