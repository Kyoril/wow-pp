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

#include "http/http_server.h"

namespace wowpp
{
	namespace web
	{
		class WebClient;

		class WebService
		{
		public:

			typedef net::http::Client Client;

			explicit WebService(
			    boost::asio::io_service &service,
			    UInt16 port
			);
			~WebService();

			void clientDisconnected(WebClient &client);
			boost::asio::io_service &getIOService() const;

			virtual std::unique_ptr<WebClient> createClient(std::shared_ptr<Client> connection) = 0;

		private:

			typedef std::vector<std::unique_ptr<WebClient>> Clients;


			boost::asio::io_service &m_ioService;
			net::http::Server m_server;
			Clients m_clients;
		};
	}
}
