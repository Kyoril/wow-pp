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
#include "web_client.h"
#include "web_service.h"
#include "common/clock.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace web
	{
		WebClient::WebClient(WebService &webService, std::shared_ptr<Client> connection)
			: m_webService(webService)
			, m_connection(connection)
		{
			m_connection->setListener(*this);
		}

		WebClient::~WebClient()
		{
		}

		void WebClient::connectionLost()
		{
			m_webService.clientDisconnected(*this);
		}

		void WebClient::connectionMalformedPacket()
		{
			//WLOG("Bad HTTP request");
			m_webService.clientDisconnected(*this);
		}

		void WebClient::connectionPacketReceived(net::http::IncomingRequest &packet)
		{
			io::StringSink sink(m_connection->getSendBuffer());
			WebResponse response(sink);
			if (packet.getType() == net::http::IncomingRequest::Options)
			{
				response.addHeader("Access-Control-Allow-Origin", "*");
				response.addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
				response.addHeader("Access-Control-Allow-Max-Age", "1000");
				response.addHeader("Access-Control-Allow-Headers", "origin, x-csrftoken, content-type, accept, authentication");
			}
			response.addHeader("Connection", "close");
			handleRequest(packet, response);
			m_connection->flush();
		}

		WebService &WebClient::getService() const
		{
			return m_webService;
		}

		WebClient::Client &WebClient::getConnection() const
		{
			assert(m_connection);
			return *m_connection;
		}
	}
}
