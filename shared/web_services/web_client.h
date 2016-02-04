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

#include "http/http_client.h"
#include "http/http_outgoing_answer.h"
#include <memory>

namespace wowpp
{
	namespace web
	{
		class WebService;

		typedef net::http::OutgoingAnswer WebResponse;

		class WebClient : public net::http::IClientListener
		{
		public:

			typedef net::http::Client Client;

			explicit WebClient(
			    WebService &webService,
			    std::shared_ptr<Client> connection);
			virtual ~WebClient();

			virtual void connectionLost();
			virtual void connectionMalformedPacket();
			virtual void connectionPacketReceived(net::http::IncomingRequest &packet);

			WebService &getService() const;
			Client &getConnection() const;

			virtual void handleRequest(const net::http::IncomingRequest &request,
			                           WebResponse &response) = 0;

		private:

			WebService &m_webService;
			std::shared_ptr<Client> m_connection;
		};
	}
}
