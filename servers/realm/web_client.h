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
#include "game_protocol/game_protocol.h"

namespace wowpp
{
	// Forwards
	class WebService;

	/// Manages the connection with a web client using the HTTP protocol.
	/// 
	/// Here, all administrative commands are handled (WoW++ doesn't support
	/// ingame chat-style commands like .server shutdown, instead you send an
	/// HTTP POST request with the address /shutdown).
	class WebClient 
		: public web::WebClient
		, public std::enable_shared_from_this<WebClient>
	{
	public:

		/// Initializes the WebClient class with a respective WebService and a connection pointer.
		/// 
		/// @param webService The web service that manages this object.
		/// @param connection The actual client connection object.
		explicit WebClient(
		    WebService &webService,
		    std::shared_ptr<Client> connection);

	public:
		/// @copydoc wowpp::web::WebClient::handleRequest
		virtual void handleRequest(const net::http::IncomingRequest &request, web::WebResponse &response) override;

	private:

		void listDeleteCharsHandler(const boost::optional<game::CharEntries> &result);

	private:
		// GET handlers

		/// Handles the /uptime GET request.
		/// 
		/// @param response Can be used to send a response to the web client.
		void handleGetUptime(const net::http::IncomingRequest &request, web::WebResponse &response);
		/// Handles the /players GET request.
		/// 
		/// @param response Can be used to send a response to the web client.
		void handleGetPlayers(const net::http::IncomingRequest &request, web::WebResponse &response);
		/// Handles the /nodes GET request.
		/// 
		/// @param response Can be used to send a response to the web client.
		void handleGetNodes(const net::http::IncomingRequest &request, web::WebResponse &response);
		/// Handles the /list-deleted-chars GET request.
		/// 
		/// @param response Can be used to receive a list of deleted characters.
		void handleListDeletedChars(const net::http::IncomingRequest &request, web::WebResponse &response);

	private:
		// POST handlers

		/// Handles the /shutdown POST request.
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostShutdown(web::WebResponse &response, const std::vector<std::string> &arguments);
		/// Handles the /copy-premade POST request.
		/// Required arguments: acc, race, class, lvl
		/// Optional arguments: gender, map, x, y, z, o, items[n], spells[n]
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostCopyPremade(web::WebResponse &response, const std::vector<std::string> &arguments);
#ifdef WOWPP_WITH_DEV_COMMANDS
		/// Handles the /additem POST request.
		/// Required arguments: character, item
		/// Optional arguments: amount
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostAddItem(web::WebResponse &response, const std::vector<std::string> &arguments);
		/// Handles the /learnspell POST request.
		/// Required arguments: character, spell
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostLearnSpell(web::WebResponse &response, const std::vector<std::string> &arguments);
		/// Handles the /teleport POST request.
		/// Required arguments: character, map
		/// Optional arguments: x, y, z, o
		/// 
		/// @param response Can be used to send a response to the web client.
		/// @param arguments List of arguments which have been parsed from the POST data.
		void handlePostTeleport(web::WebResponse &response, const std::vector<std::string> &arguments);
#endif
	};
}
