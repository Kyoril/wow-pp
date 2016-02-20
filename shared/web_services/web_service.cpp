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

#include "web_service.h"
#include "web_client.h"
#include "common/constants.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace web
	{
		WebService::WebService(
		    boost::asio::io_service &service,
		    UInt16 port
		)
			: m_ioService(service)
			, m_server(service, port, std::bind(Client::create, std::placeholders::_1, nullptr))
		{
			m_server.connected().connect([this](std::shared_ptr<Client> connection)
			{
				connection->startReceiving();
				auto client = createClient(std::move(connection));
				m_clients.push_back(std::move(client));
			});
			m_server.startAccept();
		}

		WebService::~WebService()
		{
		}

		void WebService::clientDisconnected(WebClient &client)
		{
			for (Clients::iterator i = m_clients.begin(); i != m_clients.end(); ++i)
			{
				if (i->get() == &client)
				{
					m_clients.erase(i);
					return;
				}
			}
		}

		boost::asio::io_service &WebService::getIOService() const
		{
			return m_ioService;
		}
	}
}
