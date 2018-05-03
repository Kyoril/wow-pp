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

#include "web_services/web_service.h"

namespace wowpp
{
	class PlayerManager;
	class WorldManager;
	struct IDatabase;
	class AsyncDatabase;
	namespace proto
	{
		class Project;
	}

	class WebService 
		: public web::WebService
	{
	public:

		explicit WebService(
		    boost::asio::io_service &service,
		    UInt16 port,
		    String password,
		    PlayerManager &playerManager,
			WorldManager &worldManager,
			IDatabase &database,
			AsyncDatabase &asyncDatabase,
			proto::Project &project
		);

		PlayerManager &getPlayerManager() const { return m_playerManager; }
		WorldManager &getWorldManager() const { return m_worldManager; }
		IDatabase &getDatabase() const { return m_database; }
		AsyncDatabase &getAsyncDatabase() const { return m_asyncDatabase; }
		proto::Project &getProject() const { return m_project; }
		GameTime getStartTime() const { return m_startTime; }
		const String &getPassword() const { return m_password; }

		virtual web::WebService::WebClientPtr createClient(std::shared_ptr<Client> connection) override;

	private:

		PlayerManager &m_playerManager;
		WorldManager &m_worldManager;
		IDatabase &m_database;
		AsyncDatabase &m_asyncDatabase;
		proto::Project &m_project;
		const GameTime m_startTime;
		const String m_password;
	};
}
