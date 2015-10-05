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

	class WebService : public web::WebService
	{
	public:

		explicit WebService(
		    boost::asio::io_service &service,
		    UInt16 port,
		    String password,
		    PlayerManager &playerManager,
			WorldManager &worldManager
		);

		PlayerManager &getPlayerManager() const;
		WorldManager &getWorldManager() const;
		GameTime getStartTime() const;
		const String &getPassword() const;

		virtual std::unique_ptr<web::WebClient> createClient(std::shared_ptr<Client> connection) override;

	private:

		PlayerManager &m_playerManager;
		WorldManager &m_worldManager;
		const GameTime m_startTime;
		const String m_password;
	};
}
