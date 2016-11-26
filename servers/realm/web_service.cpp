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

#include "pch.h"
#include "web_service.h"
#include "web_client.h"
#include "common/constants.h"
#include "common/clock.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	WebService::WebService(
	    boost::asio::io_service &service,
	    UInt16 port,
	    String password,
	    PlayerManager &playerManager,
		WorldManager &worldManager,
		IDatabase &database,
		proto::Project &project
	)
		: web::WebService(service, port)
		, m_playerManager(playerManager)
		, m_worldManager(worldManager)
		, m_database(database)
		, m_project(project)
		, m_startTime(getCurrentTime())
		, m_password(std::move(password))
	{
	}

	std::unique_ptr<web::WebClient> WebService::createClient(std::shared_ptr<Client> connection)
	{
		return std::unique_ptr<web::WebClient>(new WebClient(*this, connection));
	}


}
