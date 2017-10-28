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
#include "http/http_incoming_request.h"
#include "player_manager.h"
#include "player.h"
#include "world_manager.h"
#include "world.h"
#include "game/game_character.h"
#include "log/default_log_levels.h"
#include "database.h"
#include "proto_data/project.h"

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
					handleGetUptime(response);
				}
				else if (url == "/players")
				{
					handleGetPlayers(response);
				}
				else if (url == "/nodes")
				{
					handleGetNodes(response);
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
					handlePostShutdown(response, arguments);
				}
				else if (url == "/copy-premade")
				{
					handlePostCopyPremade(response, arguments);
				}
#ifdef WOWPP_WITH_DEV_COMMANDS
				else if (url == "/additem")
				{
					handlePostAddItem(response, arguments);
				}
				else if (url == "/learnspell")
				{
					handlePostLearnSpell(response, arguments);
				}
				else if (url == "/teleport")
				{
					handlePostTeleport(response, arguments);
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

	void WebClient::handleGetUptime(web::WebResponse &response)
	{
		const GameTime startTime = static_cast<WebService &>(getService()).getStartTime();

		std::ostringstream message;
		message << "<uptime>" << gameTimeToSeconds<unsigned>(getCurrentTime() - startTime) << "</uptime>";

		sendXmlAnswer(response, message.str());
	}

	void WebClient::handleGetPlayers(web::WebResponse &response)
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

	void WebClient::handleGetNodes(web::WebResponse &response)
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

	void WebClient::handlePostShutdown(web::WebResponse & response, const std::vector<std::string> arguments)
	{
		ILOG("Shutting down..");
		sendXmlAnswer(response, "<message>Shutting down..</message>");

		auto &ioService = getService().getIOService();
		ioService.stop();
	}

	void WebClient::handlePostCopyPremade(web::WebResponse & response, const std::vector<std::string> arguments)
	{
		auto &project = static_cast<WebService &>(this->getService()).getProject();

		/// Prepare default arguments
		UInt32 mapId = 0xffffffff;
		float x = 0.0f, y = 0.0f, z = 0.0f, o = 0.0f;
		UInt32 accountId = 0, charRace = 0, charClass = 0, charLvl = 0;
		Int32 gender = -1;
		LinearSet<UInt32> spellsToAdd;
		std::vector<const proto::SpellEntry*> spells;
		std::vector<ItemData> items;
		UInt32 itemIndex = 0, spellIndex = 0;
		UInt8 bagSlot = player_inventory_pack_slots::Start;
		UInt32 ringCount = 0, trinketCount = 0, bagCount = 0;
		LinearSet<UInt32> usedEquipmentSlots;

		// Parse for arguments
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
			else if (argName == "gender")
			{
				gender = atoi(argValue.c_str());
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
					spellsToAdd.optionalAdd(spellId);
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
#define WOWPP_EQUIPMENT_SLOT(slot) \
	if (!usedEquipmentSlots.contains(player_equipment_slots::slot)) { \
		preferredSlot = player_equipment_slots::slot | 0x0; \
		usedEquipmentSlots.add(player_equipment_slots::slot); \
	}

								case game::inventory_type::Head:
									WOWPP_EQUIPMENT_SLOT(Head);
									break;
								case game::inventory_type::Neck:
									WOWPP_EQUIPMENT_SLOT(Neck);
									break;
								case game::inventory_type::Shoulders:
									WOWPP_EQUIPMENT_SLOT(Shoulders);
									break;
								case game::inventory_type::Body:
									WOWPP_EQUIPMENT_SLOT(Body);
									break;
								case game::inventory_type::Chest:
								case game::inventory_type::Robe:
									WOWPP_EQUIPMENT_SLOT(Chest);
									break;
								case game::inventory_type::Waist:
									WOWPP_EQUIPMENT_SLOT(Waist);
									break;
								case game::inventory_type::Legs:
									WOWPP_EQUIPMENT_SLOT(Legs);
									break;
								case game::inventory_type::Feet:
									WOWPP_EQUIPMENT_SLOT(Feet);
									break;
								case game::inventory_type::Wrists:
									WOWPP_EQUIPMENT_SLOT(Wrists);
									break;
								case game::inventory_type::Hands:
									WOWPP_EQUIPMENT_SLOT(Hands);
									break;
								case game::inventory_type::Finger:
								{
									switch (ringCount++)
									{
									case 0:
										preferredSlot = player_equipment_slots::Finger1 | 0x0;
										break;
									case 1:
										preferredSlot = player_equipment_slots::Finger2 | 0x0;
										break;
									default:
										break;
									}
								}
								break;
								case game::inventory_type::Trinket:
									switch (trinketCount++)
									{
									case 0:
										preferredSlot = player_equipment_slots::Trinket1 | 0x0;
										break;
									case 1:
										preferredSlot = player_equipment_slots::Trinket2 | 0x0;
										break;
									default:
										break;
									}
									break;
								case game::inventory_type::Weapon:
								case game::inventory_type::TwoHandedWeapon:
								case game::inventory_type::MainHandWeapon:
									WOWPP_EQUIPMENT_SLOT(Mainhand);
									break;
								case game::inventory_type::Shield:
								case game::inventory_type::OffHandWeapon:
								case game::inventory_type::Holdable:
									WOWPP_EQUIPMENT_SLOT(Offhand);
									break;
								case game::inventory_type::Ranged:
								case game::inventory_type::Thrown:
									WOWPP_EQUIPMENT_SLOT(Ranged);
									break;
								case game::inventory_type::Cloak:
									WOWPP_EQUIPMENT_SLOT(Back);
									break;
								case game::inventory_type::Tabard:
									WOWPP_EQUIPMENT_SLOT(Tabard);
									break;
								}

#undef WOWPP_EQUIPMENT_SLOT

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
						ItemData data;
						data.entry = item->id();
						data.durability = item->durability();
						data.randomPropertyIndex = 0;
						data.randomSuffixIndex = 0;
						data.slot = preferredSlot | 0xFF00;
						data.stackCount = 1;
						items.emplace_back(std::move(data));
					}
				}
			}
		}

		// Check whether required arguments were set
		if (accountId == 0 || charRace == 0 || charClass == 0 || charLvl == 0)
		{
			sendXmlAnswer(response, "<status>MISSING_DATA</status>");
			return;
		}

		// Get database object for later use
		auto &database = static_cast<WebService &>(this->getService()).getDatabase();

		// Check character limit
		if (database.getCharacterCount(accountId).get_value_or(0) > 11)
		{
			sendXmlAnswer(response, "<status>CHARACTER_REALM_LIMIT</status>");
			return;
		}

		// Check if race exists
		auto *race = project.races.getById(charRace);
		if (!race)
		{
			sendXmlAnswer(response, "<status>INVALID_RACE</status>");
			return;
		}

		// Check if class exists
		auto *class_ = project.classes.getById(charClass);
		if (!class_)
		{
			sendXmlAnswer(response, "<status>INVALID_CLASS</status>");
			return;
		}

		// Get initial spells for that race/class combination and thus check if this combination is allowed
		const auto &initialSpellsEntry = race->initialspells().find(charClass);
		if (initialSpellsEntry == race->initialspells().end())
		{
			sendXmlAnswer(response, "<status>INVALID_RACE_CLASS_COMBINATION</status>");
			return;
		}

		// Check all initial spells to see if they are valid
		for (int i = 0; i < initialSpellsEntry->second.spells_size(); ++i)
		{
			const auto &spellid = initialSpellsEntry->second.spells(i);
			const auto *spell = project.spells.getById(spellid);
			if (spell)
			{
				spellsToAdd.optionalAdd(spellid);
				spells.push_back(spell);
			}
		}

		// Filter spells by ranks (don't need to learn lower rank spells if higher rank spells are available)
		for (const auto &spellId : spellsToAdd)
		{
			// Check if spell exists
			const auto *spell = project.spells.getById(spellId);
			if (!spell)
				continue;

			// Skip talent spells
			if (spell->talentcost() > 0)
				continue;

			// If spell has any dependant spells...
			if (spell->prevspell() != 0)
			{
				// Find previous spell and skip if not existant
				const auto *prevSpell = project.spells.getById(spell->prevspell());
				if (!prevSpell)
					continue;

				// Skip if spell is not in list of spells to add
				if (!spellsToAdd.contains(prevSpell->id()))
					continue;

				// Skip if previous spell's base spell isn't in list
				if (prevSpell->baseid() != prevSpell->id() && !spellsToAdd.contains(prevSpell->baseid()))
					continue;
			}

			// Really add spell
			spells.push_back(spell);
		}

		// Verify gender
		if (gender > 1)
		{
			sendXmlAnswer(response, "<status>INVALID_GENDER</status>");
			return;
		}

		// Eventually randomize gender if no gender was provided
		if (gender < 0 || gender > 1)
		{
			std::uniform_int_distribution<int> genderDist(0, 1);
			gender = genderDist(randomGenerator);
		}

		// Random generator for some character outfits
		std::uniform_int_distribution<> outfitDist(0, 3);

		// Setup CharEntry structure which we submit to the database
		game::CharEntry characterData;
		characterData.race = static_cast<game::Race>(charRace);
		characterData.class_ = static_cast<game::CharClass>(charClass);
		characterData.cinematic = false;
		characterData.face = outfitDist(randomGenerator);
		characterData.facialHair = outfitDist(randomGenerator);
		characterData.hairColor = outfitDist(randomGenerator);
		characterData.hairStyle = outfitDist(randomGenerator);
		characterData.skin = outfitDist(randomGenerator);
		characterData.gender = static_cast<game::Gender>(gender);
		characterData.id = 0;
		characterData.name = randomText(12);					// Generate an (most likely invalid) random name
		characterData.atLogin = game::atlogin_flags::Rename;	// Force the player to rename his character on first login
		characterData.outfitId = 0;
		characterData.level = charLvl;
		characterData.mapId = (mapId == 0xffffffff ? race->startmap() : mapId);
		characterData.zoneId = race->startzone();
		characterData.location.x = (x == 0.0f ? race->startposx() : x);
		characterData.location.y = (y == 0.0f ? race->startposy() : y);
		characterData.location.z = (z == 0.0f ? race->startposz() : z);
		characterData.o = (o == 0.0f ? race->startrotation() : o);

		// Create the character
		auto result = database.createCharacter(accountId, spells, items, characterData);
		switch (result)
		{
		case game::response_code::CharCreatePvPTeamsViolation:
			sendXmlAnswer(response, "<status>PVP_VIOLATION</status>");
			return;
		case game::response_code::CharCreateError:
			sendXmlAnswer(response, "<status>DATABASE_ERROR</status>");
			return;
		case game::response_code::CharCreateNameInUse:
			sendXmlAnswer(response, "<status>NAME_IN_USE</status>");
			break;
		case game::response_code::CharCreateSuccess:
			sendXmlAnswer(response, "<status>SUCCESS</status>");
			break;
		default:
			sendXmlAnswer(response, "<status>UNKNOWN_ERROR</status>");
			break;
		}
	}

#ifdef WOWPP_WITH_DEV_COMMANDS
	void WebClient::handlePostAddItem(web::WebResponse & response, const std::vector<std::string> arguments)
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
			return;
		}

		auto &playerMgr = static_cast<WebService &>(this->getService()).getPlayerManager();
		auto player = playerMgr.getPlayerByCharacterName(characterName);
		if (!player)
		{
			// TODO: Add item when offline
			sendXmlAnswer(response, "<status>PLAYER_NOT_ONLINE</status>");
			return;
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

	void WebClient::handlePostLearnSpell(web::WebResponse & response, const std::vector<std::string> arguments)
	{
		String characterName;
		UInt32 spellId = 0;
		for (auto &arg : arguments)
		{
			auto delimiterPos = arg.find('=');
			String argName = arg.substr(0, delimiterPos);
			String argValue = arg.substr(delimiterPos + 1);

			if (argName == "character")
			{
				characterName = argValue;
			}
			else if (argName == "spell")
			{
				spellId = atoi(argValue.c_str());
			}
		}

		if (spellId == 0 || characterName.empty())
		{
			sendXmlAnswer(response, "<status>MISSING_DATA</status>");
			return;
		}

		auto &project = static_cast<WebService &>(this->getService()).getProject();
		const auto *spellInst = project.spells.getById(spellId);
		if (!spellInst)
		{
			sendXmlAnswer(response, "<status>INVALID_SPELL</status>");
			return;
		}

		auto &playerMgr = static_cast<WebService &>(this->getService()).getPlayerManager();
		auto player = playerMgr.getPlayerByCharacterName(characterName);
		if (!player)
		{
			auto &db = static_cast<WebService &>(this->getService()).getDatabase();

			// Does the character exist?
			try
			{
				auto entry = db.getCharacterByName(characterName);

				// Character known - learn spell
				try
				{
					db.learnSpell(entry.id, spellId);
				}
				catch (const std::exception &ex)
				{
					ELOG("Database error: " << ex.what());
					sendXmlAnswer(response, "<status>DATABASE_ERROR</status>");
					return;
				}
			}
			catch (const std::exception& ex)
			{
				defaultLogException(ex);
				sendXmlAnswer(response, "<status>CHARACTER_NOT_FOUND</status>");
				return;
			}

			sendXmlAnswer(response, "<status>SUCCESS</status>");
			return;
		}
		else
		{
			// Send a learn spell request
			auto result = player->learnSpell(*spellInst);
			switch (result)
			{
			case learn_spell_result::Success:
				sendXmlAnswer(response, "<status>SUCCESS</status>");
				return;
			case learn_spell_result::AlreadyLearned:
				sendXmlAnswer(response, "<status>ALREADY_LEARNED</status>");
				return;
			default:
				sendXmlAnswer(response, "<status>UNKNOWN_ERROR</status>");
				return;
			}
		}
	}

	void WebClient::handlePostTeleport(web::WebResponse & response, const std::vector<std::string> arguments)
	{
		// Handle required data
		String characterName;
		UInt32 mapId = 0xffffffff;
		float x = 0.0f, y = 0.0f, z = 0.0f, o = 0.0f;

		for (auto &arg : arguments)
		{
			auto delimiterPos = arg.find('=');
			String argName = arg.substr(0, delimiterPos);
			String argValue = arg.substr(delimiterPos + 1);

			if (argName == "character")
			{
				characterName = argValue;
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
		}

		if (mapId == 0xffffffff || characterName.empty())
		{
			sendXmlAnswer(response, "<status>MISSING_DATA</status>");
			return;
		}

		auto &project = static_cast<WebService &>(this->getService()).getProject();
		const auto *mapInst = project.maps.getById(mapId);
		if (!mapInst)
		{
			sendXmlAnswer(response, "<status>INVALID_MAP</status>");
			return;
		}

		auto &playerMgr = static_cast<WebService &>(this->getService()).getPlayerManager();
		auto player = playerMgr.getPlayerByCharacterName(characterName);
		if (!player)
		{
			// Player is offline, so just change database coordinates
			auto &db = static_cast<WebService &>(this->getService()).getDatabase();

			// Does the character exist?
			try
			{
				auto entry = db.getCharacterByName(characterName);

				// Character exists - teleport him
				try
				{
					db.teleportCharacter(entry.id, mapId, x, y, z, o);
				}
				catch (const std::exception &ex)
				{
					ELOG("Database error: " << ex.what());
					sendXmlAnswer(response, "<status>DATABASE_ERROR</status>");
					return;
				}
			}
			catch (const std::exception &ex)
			{
				defaultLogException(ex);
				sendXmlAnswer(response, "<status>CHARACTER_NOT_FOUND</status>");
				return;
			}

			sendXmlAnswer(response, "<status>SUCCESS</status>");
		}
		else
		{
			if (!player->initializeTransfer(mapId, math::Vector3(x, y, z), o, true))
			{
				sendXmlAnswer(response, "<status>PLAYER_NOT_IN_WORLD</status>");
				return;
			}

			sendXmlAnswer(response, "<status>SUCCESS</status>");
		}
	}
#endif
}
