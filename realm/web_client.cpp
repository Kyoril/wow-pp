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

#include "web_client.h"
#include "web_service.h"
#include "common/clock.h"
#include "http/http_incoming_request.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace
	{
		void sendXmlAnswer(web::WebResponse &response, const String &xml)
		{
			response.finishWithContent("text/xml", xml.data(), xml.size());
		}
	}

	WebClient::WebClient(WebService &webService, std::shared_ptr<Client> connection)
		: web::WebClient(webService, connection)
	{
	}

	void WebClient::handleRequest(const net::http::IncomingRequest &request,
	                              web::WebResponse &response)
	{
		if (!net::http::authorize(request,
		                          [this](const std::string &name, const std::string &password) -> bool
			{
				(void)name;
				const auto &expectedPassword = static_cast<WebService &>(this->getService()).getPassword();
				return (expectedPassword == password);
			}))
		{
			respondUnauthorized(response, "WoW++ Realm");
			return;
		}

		const auto &url = request.getPath();
		if (url == "/uptime")
		{
			const GameTime startTime = static_cast<WebService &>(getService()).getStartTime();

			std::ostringstream message;
			message << "<uptime>" << gameTimeToSeconds<unsigned>(getCurrentTime() - startTime) << "</uptime>";

			sendXmlAnswer(response, message.str());
		}
		else if (url == "/shutdown")
		{
			ILOG("Shutting down..");
			sendXmlAnswer(response, "<message>shutting down..</message>");

			auto &ioService = getService().getIOService();
			ioService.stop();
		}
		else
		{
			response.setStatus(net::http::OutgoingAnswer::NotFound);

			const String message = "The document '" + url + "' does not exist";
			response.finishWithContent("text/html", message.data(), message.size());
		}
	}
}
