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
	class TeamServer;

	/// Manages all connected team servers.
	class TeamServerManager : public boost::noncopyable
	{
	public:

		typedef std::vector<std::unique_ptr<TeamServer>> TeamServers;

	public:

		/// Initializes a new instance of the team server manager class.
		/// @param capacity The maximum number of connections that can be connected at the same time.
		explicit TeamServerManager(
		    size_t capacity
		);
		~TeamServerManager();

		/// Notifies the manager that a team server has been disconnected which will
		/// delete the team server instance.
		void teamServerDisconnected(TeamServer &realm);
		/// Gets a dynamic array containing all connected team server instances.
		const TeamServers &getTeamServers() const;
		/// Determines whether the capacity limit has been reached.
		bool hasCapacityBeenReached() const;
		/// Adds a new team server instance to the manager.
		void addTeamServer(std::unique_ptr<TeamServer> added);

		/// Gets a realm by its internal name if available.
		TeamServer *getTeamServerByInternalName(const String &name);

	private:

		TeamServers m_teamServers;
		size_t m_capacity;
	};
}
