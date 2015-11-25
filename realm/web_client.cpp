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
#include "world_manager.h"
#include "world.h"
#include "game/game_character.h"
#include "log/default_log_levels.h"
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
			respondUnauthorized(response, "WoW++ Realm");
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
				else if (url == "/players")
				{
					std::ostringstream message;
					message << "<players>";

					auto &playerMgr = static_cast<WebService &>(this->getService()).getPlayerManager();
					for (const auto &player : playerMgr.getPlayers())
					{
						const auto *character = player->getGameCharacter();
						if (character)
						{
							message << "<player name=\"" << character->getName() << "\" level=\"" << character->getLevel() << "\" race=\"" <<
								static_cast<UInt16>(character->getRace()) << "\" class=\"" << static_cast<UInt16>(character->getClass()) << "\" map=\"" <<
								character->getMapId() << "\" zone=\"" << character->getZone() << "\" />";
						}
					}
					message << "</players>";
					sendXmlAnswer(response, message.str());
				}
				else if (url == "/nodes")
				{
					std::ostringstream message;
					message << "<nodes>";

					auto &worldMgr = static_cast<WebService &>(this->getService()).getWorldManager();
					for (const auto &world : worldMgr.getWorlds())
					{
						message << "<node><maps>";
						for (auto &mapId : world->getMapList())
						{
							message << "<map id=\"" << mapId << "\" />";
						}
						message << "</maps><instances>";
						for (auto &instanceId : world->getInstanceList())
						{
							message << "<instance id=\"" << instanceId << "\" />";
						}
						message << "</instances>";
						message << "</node>";
					}
					message << "</nodes>";
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
					sendXmlAnswer(response, "<message>Shutting down..</message>");

					auto &ioService = getService().getIOService();
					ioService.stop();
				}
#ifdef WOWPP_WITH_DEV_COMMANDS
				else if (url == "/additem")
				{
					// Handle required data
					String characterName;
					UInt32 itemGuid = 0;

					for (auto &arg : arguments)
					{
						auto delimiterPos = arg.find('=');
						String argName = arg.substr(0, delimiterPos);
						String argValue = arg.substr(delimiterPos + 1);

						if (argName == "character")
						{
							characterName = argValue;
						}
						else if (argName == "item")
						{
							itemGuid = atoi(argValue.c_str());
						}
					}

					if (itemGuid == 0 || characterName.empty())
					{
						sendXmlAnswer(response, "<status>MISSING_DATA</status>");
						break;
					}

					auto &playerMgr = static_cast<WebService &>(this->getService()).getPlayerManager();
					auto *player = playerMgr.getPlayerByCharacterName(characterName);
					if (!player)
					{
						// TODO: Add item when offline
						sendXmlAnswer(response, "<status>PLAYER_NOT_ONLINE</status>");
						break;
					}

					// Send a create item request
					auto result = player->addItem(itemGuid, 1);
					switch (result)
					{
						case add_item_result::Success:
							sendXmlAnswer(response, "<status>SUCCESS</status>");
							return;
						case add_item_result::ItemNotFound:
							sendXmlAnswer(response, "<status>UNKNOWN_ITEM</status>");
							return;
						case add_item_result::BagIsFull:
							sendXmlAnswer(response, "<status>INVENTORY_FULL</status>");
							return;
						case add_item_result::TooManyItems:
							sendXmlAnswer(response, "<status>TOO_MANY_ITEMS</status>");
							return;
						default:
							sendXmlAnswer(response, "<status>UNKNOWN_ERROR</status>");
							return;
					}
				}
#endif
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
