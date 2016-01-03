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
#include "database.h"
#include "proto_data/project.h"
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
				else if (url == "/copy-premade")
				{
					auto &project = static_cast<WebService &>(this->getService()).getProject();

					UInt32 mapId = 0xffffffff;
					float x = 0.0f, y = 0.0f, z = 0.0f, o = 0.0f;
					UInt32 accountId = 0, charRace = 0, charClass = 0, charLvl = 0;
					std::vector<const proto::SpellEntry*> spells;
					std::vector<pp::world_realm::ItemData> items;
					UInt32 itemIndex = 0, spellIndex = 0;

					UInt8 bagSlot = player_inventory_pack_slots::Start;

					for (auto &arg : arguments)
					{
						auto delimiterPos = arg.find('=');
						String argName = arg.substr(0, delimiterPos);
						String argValue = arg.substr(delimiterPos + 1);

						std::stringstream spellarg;
						spellarg << "spells%5B" << spellIndex << "%5D";
						std::stringstream itemarg;
						itemarg << "items%5B" << itemIndex << "%5D";

						if (argName == "acc")
						{
							accountId = atoi(argValue.c_str());
						}
						else if (argName == "race")
						{
							charRace = atoi(argValue.c_str());
						}
						else if (argName == "class")
						{
							charClass = atoi(argValue.c_str());
						}
						else if (argName == "lvl")
						{
							charLvl = atoi(argValue.c_str());
							if (charLvl < 1) charLvl = 1;
							else if (charLvl > 70) charLvl = 70;
						}
						else if (argName == "map")
						{
							mapId = atoi(argValue.c_str());
						}
						else if (argName == "x")
						{
							x = atof(argValue.c_str());
						}
						else if (argName == "y")
						{
							y = atof(argValue.c_str());
						}
						else if (argName == "z")
						{
							z = atof(argValue.c_str());
						}
						else if (argName == "o")
						{
							o = atof(argValue.c_str());
						}
						else if (argName == spellarg.str())
						{
							spellIndex++;

							UInt32 spellId = atoi(argValue.c_str());
							if (spellId != 0)
							{
								auto *spell = project.spells.getById(spellId);
								if (spell) spells.push_back(spell);
							}
						}
						else if (argName == itemarg.str())
						{
							itemIndex++;

							UInt32 itemId = atoi(argValue.c_str());
							if (itemId != 0)
							{
								auto *item = project.items.getById(itemId);
								if (item)
								{
									UInt16 preferredBagSlot = bagSlot;
									UInt16 preferredSlot = preferredBagSlot;

									if (item->maxstack() > 1)
									{
										// Check if we should add stacks
										bool addedStack = false;
										for (auto &data : items)
										{
											if (data.entry == item->id() &&
												data.stackCount < item->maxstack())
											{
												data.stackCount++;
												addedStack = true;
												break;
											}
										}

										if (addedStack)
										{
											// Item was added - next argument
											continue;
										}
									}
									else
									{
										if (bagSlot < player_inventory_pack_slots::End)
										{
											// Find preferred item slot based on inventory type
											switch (item->inventorytype())
											{
												case game::inventory_type::Head:
													preferredSlot = player_equipment_slots::Head | 0x0;
													break;
												case game::inventory_type::Neck:
													preferredSlot = player_equipment_slots::Neck | 0x0;
													break;
												case game::inventory_type::Shoulders:
													preferredSlot = player_equipment_slots::Shoulders | 0x0;
													break;
												case game::inventory_type::Body:
													preferredSlot = player_equipment_slots::Body | 0x0;
													break;
												case game::inventory_type::Chest:
												case game::inventory_type::Robe:
													preferredSlot = player_equipment_slots::Chest | 0x0;
													break;
												case game::inventory_type::Waist:
													preferredSlot = player_equipment_slots::Waist | 0x0;
													break;
												case game::inventory_type::Legs:
													preferredSlot = player_equipment_slots::Legs | 0x0;
													break;
												case game::inventory_type::Feet:
													preferredSlot = player_equipment_slots::Feet | 0x0;
													break;
												case game::inventory_type::Wrists:
													preferredSlot = player_equipment_slots::Wrists | 0x0;
													break;
												case game::inventory_type::Hands:
													preferredSlot = player_equipment_slots::Hands | 0x0;
													break;
												case game::inventory_type::Finger:
													preferredSlot = player_equipment_slots::Finger1 | 0x0;
													break;
												case game::inventory_type::Trinket:
													preferredSlot = player_equipment_slots::Trinket1 | 0x0;
													break;
												case game::inventory_type::Weapon:
												case game::inventory_type::TwoHandedWeapon:
												case game::inventory_type::MainHandWeapon:
													preferredSlot = player_equipment_slots::Mainhand | 0x0;
													break;
												case game::inventory_type::Shield:
												case game::inventory_type::OffHandWeapon:
												case game::inventory_type::Holdable:
													preferredSlot = player_equipment_slots::Offhand | 0x0;
													break;
												case game::inventory_type::Ranged:
												case game::inventory_type::Thrown:
													preferredSlot = player_equipment_slots::Ranged | 0x0;
													break;
												case game::inventory_type::Cloak:
													preferredSlot = player_equipment_slots::Back | 0x0;
													break;
												case game::inventory_type::Tabard:
													preferredSlot = player_equipment_slots::Tabard | 0x0;
													break;
											}

											// Check if the item is equippable
											if (preferredSlot != preferredBagSlot)
											{
												for (auto &data : items)
												{
													if (data.slot == preferredSlot)
													{
														preferredSlot = preferredBagSlot;
														break;
													}
												}
											}
										}
									}

									// New bag slot will be used
									if (preferredSlot == preferredBagSlot)
									{
										bagSlot++;
									}

									// Create a new stack
									pp::world_realm::ItemData data;
									data.entry = item->id();
									data.durability = item->durability();
									data.randomPropertyIndex = 0;
									data.randomSuffixIndex = 0;
									data.slot = preferredSlot;
									data.stackCount = 1;
									items.emplace_back(std::move(data));
								}
							}
						}
					}

					if (accountId == 0 || charRace == 0 || charClass == 0 || charLvl == 0)
					{
						sendXmlAnswer(response, "<status>MISSING_DATA</status>");
						break;
					}

					auto &database = static_cast<WebService &>(this->getService()).getDatabase();

					// Check character limit
					if (database.getCharacterCount(accountId) > 11)
					{
						sendXmlAnswer(response, "<status>CHARACTER_REALM_LIMIT</status>");
						break;
					}

					// Get race entry
					auto *race = project.races.getById(charRace);
					if (!race)
					{
						sendXmlAnswer(response, "<status>INVALID_RACE</status>");
						break;
					}

					// Get class entry
					auto *class_ = project.classes.getById(charClass);
					if (!class_)
					{
						sendXmlAnswer(response, "<status>INVALID_CLASS</status>");
						break;
					}

					// Add initial spells
					const auto &initialSpellsEntry = race->initialspells().find(charClass);
					if (initialSpellsEntry == race->initialspells().end())
					{
						sendXmlAnswer(response, "<status>INVALID_RACE_CLASS_COMBINATION</status>");
						break;
					}

					for (int i = 0; i < initialSpellsEntry->second.spells_size(); ++i)
					{
						const auto &spellid = initialSpellsEntry->second.spells(i);
						const auto *spell = project.spells.getById(spellid);
						if (spell)
						{
							spells.push_back(spell);
						}
					}

					std::uniform_int_distribution<> genderDist(0, 1);
					auto gender = genderDist(randomGenerator);

					game::CharEntry characterData;
					characterData.race = static_cast<game::Race>(charRace);
					characterData.class_ = static_cast<game::CharClass>(charClass);
					characterData.cinematic = false;
					characterData.face = 0;
					characterData.facialHair = 0;
					characterData.hairColor = 0;
					characterData.hairStyle = 0;
					characterData.skin = 0;
					characterData.gender = static_cast<game::Gender>(gender);
					characterData.id = 0;
					characterData.name = randomText(12);
					characterData.atLogin = game::atlogin_flags::Rename;
					characterData.outfitId = 0;
					characterData.level = charLvl;
					characterData.mapId = (mapId == 0xffffffff ? race->startmap() : mapId);
					characterData.zoneId = race->startzone();
					characterData.x = (x == 0.0f ? race->startposx() : x);
					characterData.y = (y == 0.0f ? race->startposy() : y);
					characterData.z = (z == 0.0f ? race->startposz() : z);
					characterData.o = (o == 0.0f ? race->startrotation() : o);

					// Create the character
					auto result = database.createCharacter(accountId, spells, items, characterData);
					switch (result)
					{
						case game::response_code::CharCreatePvPTeamsViolation:
						{
							sendXmlAnswer(response, "<status>PVP_VIOLATION</status>");
							break;
						}

						case game::response_code::CharCreateError:
						{
							sendXmlAnswer(response, "<status>DATABASE_ERROR</status>");
							break;
						}

						case game::response_code::CharCreateNameInUse:
						{
							sendXmlAnswer(response, "<status>NAME_IN_USE</status>");
							break;
						}

						case game::response_code::CharCreateSuccess:
						{
							sendXmlAnswer(response, "<status>SUCCESS</status>");
							break;
						}

						default:
						{
							sendXmlAnswer(response, "<status>UNKNOWN_ERROR</status>");
							break;
						}
					}
				}
#ifdef WOWPP_WITH_DEV_COMMANDS
				else if (url == "/additem")
				{
					// Handle required data
					String characterName;
					UInt32 itemGuid = 0;
					UInt32 amount = 1;

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
						else if (argName == "amount")
						{
							amount = atoi(argValue.c_str());
							if (amount < 1) amount = 1;
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
					auto result = player->addItem(itemGuid, amount);
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
