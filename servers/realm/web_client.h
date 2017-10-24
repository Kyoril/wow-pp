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

#include "web_services/web_client.h"

namespace wowpp
{
	class WebService;

	class WebClient : public web::WebClient
	{
	public:

		explicit WebClient(
		    WebService &webService,
		    std::shared_ptr<Client> connection);

		virtual void handleRequest(const net::http::IncomingRequest &request,
		                           web::WebResponse &response) override;

	private:
		// GET handlers

		/// Handles the /uptime GET request.
		/// 
		/// @param response Can be used to send a response to the web client.
		void handleGetUptime(web::WebResponse &response);
		/// Handles the /players GET request.
		/// 
		/// @param response Can be used to send a response to the web client.
		void handleGetPlayers(web::WebResponse &response);
		/// Handles the /nodes GET request.
		/// 
		/// @param response Can be used to send a response to the web client.
		void handleGetNodes(web::WebResponse &response);

	private:
		// POST handlers

		/// Handles the /shutdown POST request.
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostShutdown(web::WebResponse &response, const std::vector<std::string> arguments);
		/// Handles the /copy-premade POST request.
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostCopyPremade(web::WebResponse &response, const std::vector<std::string> arguments);
#ifdef WOWPP_WITH_DEV_COMMANDS
		/// Handles the /additem POST request.
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostAddItem(web::WebResponse &response, const std::vector<std::string> arguments);
		/// Handles the /learnspell POST request.
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostLearnSpell(web::WebResponse &response, const std::vector<std::string> arguments);
		/// Handles the /teleport POST request.
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostTeleport(web::WebResponse &response, const std::vector<std::string> arguments);
#endif
	};
}
