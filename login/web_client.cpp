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
#include "player_manager.h"
#include "player.h"
#include "game/game_character.h"
#include "log/default_log_levels.h"
#include "database.h"
#include <boost/algorithm/string.hpp>

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
			respondUnauthorized(response, "WoW++ Login");
			return;
		}

		const auto &url = request.getPath();
		switch(request.getType())
		{
			case net::http::IncomingRequest::Get:
			{
				if (url == "/uptime")
				{
					const GameTime startTime = static_cast<WebService &>(getService()).getStartTime();

					std::ostringstream message;
					message << "<uptime>" << gameTimeToSeconds<unsigned>(getCurrentTime() - startTime) << "</uptime>";

					sendXmlAnswer(response, message.str());
				}
				else
				{
					response.setStatus(net::http::OutgoingAnswer::NotFound);

					const String message = "The command '" + url + "' does not exist";
					response.finishWithContent("text/html", message.data(), message.size());
				}
				break;
			}
			case net::http::IncomingRequest::Post:
			{
				// Parse arguments
				std::vector<String> arguments;
				boost::split(arguments, request.getPostData(), boost::is_any_of("&"));

				if (url == "/shutdown")
				{
					ILOG("Shutting down..");
					sendXmlAnswer(response, "<status>SUCCESS</status>");

					auto &ioService = getService().getIOService();
					ioService.stop();
				}
				else if (url == "/sync-account")
				{
					// Handle required data
					String accountName, accountPassword;
					UInt32 accountId = 0;

					for (auto &arg : arguments)
					{
						auto delimiterPos = arg.find('=');
						String argName = arg.substr(0, delimiterPos);
						String argValue = arg.substr(delimiterPos + 1);

						if (argName == "id")
						{
							accountId = atoi(argValue.c_str());
						}
						else if (argName == "username")
						{
							accountName = boost::to_upper_copy(argValue);
						}
						else if (argName == "password")
						{
							accountPassword = boost::to_upper_copy(argValue);
						}
					}

					if (accountId == 0 || accountName.empty() || accountPassword.empty())
					{
						sendXmlAnswer(response, "<status>MISSING_DATA</status>");
						break;
					}

					auto &playerMgr = static_cast<WebService &>(this->getService()).getPlayerManager();
					auto *player = playerMgr.getPlayerByAccountID(accountId);
					if (player)
					{
						// Account is currently logged in
						sendXmlAnswer(response, "<status>ACCOUNT_LOGGED_IN</status>");
						break;
					}

					// Check if account already exists
					auto &database = static_cast<WebService &>(this->getService()).getDatabase();
					
					// Check database values
					String dbName, dbHash;
					if (!database.getAccountInfos(accountId, dbName, dbHash))
					{
						sendXmlAnswer(response, "<status>DATABASE_ERROR</status>");
						break;
					}

					// Convert to uppercase characters
					boost::to_upper(dbName);
					boost::to_upper(dbHash);

					// If the account doesn't already exist...
					if (dbName.empty())
					{
						// Check if account name is already taken
						UInt32 tmpId = 0;
						String tmpPass;
						if (!database.getPlayerPassword(accountName, tmpId, tmpPass))
						{
							sendXmlAnswer(response, "<status>DATABASE_ERROR</status>");
							break;
						}

						if (tmpId != 0 &&
							tmpId != accountId)
						{
							sendXmlAnswer(response, "<status>ACCOUNT_NAME_ALREADY_IN_USE</status>");
							break;
						}

						// Create new user account
						if (!database.createAccount(accountId, accountName, accountPassword))
						{
							sendXmlAnswer(response, "<status>DATABASE_ERROR</status>");
							break;
						}
					}
					else
					{
						// Account already existed: Compare account name
						if (accountName != dbName)
						{
							sendXmlAnswer(response, "<status>ACCOUNT_DATA_MISMATCH</status>");
							break;
						}

						// Update account password
						if (!database.setPlayerPassword(accountId, accountPassword))
						{
							sendXmlAnswer(response, "<status>DATABASE_ERROR</status>");
							break;
						}
					}

					// Succeeded
					sendXmlAnswer(response, "<status>SUCCESS</status>");
				}
				else
				{
					const String message = "The command '" + url + "' does not exist";
					response.finishWithContent("text/html", message.data(), message.size());
				}
			}
			default:
			{
				break;
			}
		}
	}
}
