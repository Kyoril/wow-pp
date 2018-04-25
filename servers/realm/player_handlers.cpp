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
#include "player.h"
#include "player_manager.h"
#include "configuration.h"
#include "common/sha1.h"
#include "log/default_log_levels.h"
#include "common/clock.h"
#include "login_connector.h"
#include "world_manager.h"
#include "world.h"
#include "player_social.h"
#include "common/big_number.h"
#include "binary_io/vector_sink.h"
#include "binary_io/writer.h"
#include "proto_data/project.h"
#include "common/utilities.h"
#include "game/game_item.h"
#include "player_group.h"
#include "game/constants.h"

using namespace std;

namespace wowpp
{

	PacketParseResult Player::handlePing(game::IncomingPacket &packet)
	{
		UInt32 ping, latency;
		if (!game::client_read::ping(packet, ping, latency))
		{
			return PacketParseResult::Disconnect;
		}

		// Send pong
		sendPacket(
			std::bind(game::server_write::pong, std::placeholders::_1, ping));

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleAuthSession(game::IncomingPacket &packet)
	{
		// Clear addon list
		m_addons.clear();

		UInt32 clientBuild;
		if (!game::client_read::authSession(packet, clientBuild, m_accountName, m_clientSeed, m_clientHash, m_addons))
		{
			return PacketParseResult::Disconnect;
		}

		// Check if the client version is valid (defined in CMake)
		if (clientBuild != SUPPORTED_CLIENT_BUILD)
		{
			//TODO Send error result
			WLOG("Client " << m_address << " tried to login with unsupported client build " << clientBuild);
			return PacketParseResult::Disconnect;
		}

		// Ask the login server if this login is okay and also ask for session key etc.
		if (!m_loginConnector.playerLoginRequest(m_accountName))
		{
			// Could not send player login request
			return PacketParseResult::Disconnect;
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleCharEnum(game::IncomingPacket &packet)
	{
		if (!(game::client_read::charEnum(packet)))
		{
			return PacketParseResult::Disconnect;
		}

		reloadCharacters();

		// Block incoming packets until characters are reloaded
		return PacketParseResult::Block;
	}

	PacketParseResult Player::handleCharCreate(game::IncomingPacket &packet)
	{
		game::CharEntry character;
		if (!(game::client_read::charCreate(packet, character)))
		{
			return PacketParseResult::Disconnect;
		}

		// Empty character name?
		character.name = trim(character.name);
		if (character.name.empty())
		{
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateError));
			return PacketParseResult::Pass;
		}

		// TODO: Check for invalid characters (numbers, white spaces etc.)

		// Capitalize the characters name
		capitalize(character.name);

		// Get number of characters on this account
		const UInt32 maxCharacters = 11;
		auto numCharacters = m_database.getCharacterCount(m_accountId);
		if (!numCharacters)
		{
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateError));
			return PacketParseResult::Pass;
		}

		// Check that the account doesn't exceed the limit
		if (numCharacters.get() >= maxCharacters)
		{
			// No more free slots
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateServerLimit));
			return PacketParseResult::Pass;
		}

		// Get the racial informations
		const auto *race = m_project.races.getById(character.race);
		if (!race)
		{
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateError));
			return PacketParseResult::Pass;
		}

		// Add initial spells
		const auto &initialSpellsEntry = race->initialspells().find(character.class_);
		if (initialSpellsEntry == race->initialspells().end())
		{
			sendPacket(
				std::bind(game::server_write::charCreate, std::placeholders::_1, game::response_code::CharCreateError));
			return PacketParseResult::Pass;
		}

		// Item data
		UInt8 bagSlot = 0;
		std::vector<ItemData> items;

		// Add initial items
		auto it1 = race->initialitems().find(character.class_);
		if (it1 != race->initialitems().end())
		{
			for (int i = 0; i < it1->second.items_size(); ++i)
			{
				const auto *item = m_project.items.getById(it1->second.items(i));
				if (!item)
					continue;

				// Get slot for this item
				UInt16 slot = 0xffff;
				switch (item->inventorytype())
				{
				case game::inventory_type::Head:
				{
					slot = player_equipment_slots::Head;
					break;
				}
				case game::inventory_type::Neck:
				{
					slot = player_equipment_slots::Neck;
					break;
				}
				case game::inventory_type::Shoulders:
				{
					slot = player_equipment_slots::Shoulders;
					break;
				}
				case game::inventory_type::Body:
				{
					slot = player_equipment_slots::Body;
					break;
				}
				case game::inventory_type::Chest:
				case game::inventory_type::Robe:
				{
					slot = player_equipment_slots::Chest;
					break;
				}
				case game::inventory_type::Waist:
				{
					slot = player_equipment_slots::Waist;
					break;
				}
				case game::inventory_type::Legs:
				{
					slot = player_equipment_slots::Legs;
					break;
				}
				case game::inventory_type::Feet:
				{
					slot = player_equipment_slots::Feet;
					break;
				}
				case game::inventory_type::Wrists:
				{
					slot = player_equipment_slots::Wrists;
					break;
				}
				case game::inventory_type::Hands:
				{
					slot = player_equipment_slots::Hands;
					break;
				}
				case game::inventory_type::Finger:
				{
					//TODO: Finger1/2
					slot = player_equipment_slots::Finger1;
					break;
				}
				case game::inventory_type::Trinket:
				{
					//TODO: Trinket1/2
					slot = player_equipment_slots::Trinket1;
					break;
				}
				case game::inventory_type::Weapon:
				case game::inventory_type::TwoHandedWeapon:
				case game::inventory_type::MainHandWeapon:
				{
					slot = player_equipment_slots::Mainhand;
					break;
				}
				case game::inventory_type::Shield:
				case game::inventory_type::OffHandWeapon:
				case game::inventory_type::Holdable:
				{
					slot = player_equipment_slots::Offhand;
					break;
				}
				case game::inventory_type::Ranged:
				case game::inventory_type::Thrown:
				{
					slot = player_equipment_slots::Ranged;
					break;
				}
				case game::inventory_type::Cloak:
				{
					slot = player_equipment_slots::Back;
					break;
				}
				case game::inventory_type::Tabard:
				{
					slot = player_equipment_slots::Tabard;
					break;
				}

				default:
				{
					if (bagSlot < player_inventory_pack_slots::Count_)
					{
						slot = player_inventory_pack_slots::Start + (bagSlot++);
					}
					break;
				}
				}

				if (slot != 0xffff)
				{
					ItemData itemData;
					itemData.entry = item->id();
					itemData.durability = item->durability();
					itemData.slot = slot | 0xFF00;
					itemData.stackCount = 1;
					items.push_back(std::move(itemData));
				}
			}
		}

		// Update character location
		character.mapId = race->startmap();
		character.zoneId = race->startzone();
		character.location.x = race->startposx();
		character.location.y = race->startposy();
		character.location.z = race->startposz();
		character.o = race->startrotation();

		std::vector<const proto::SpellEntry*> initialSpells;
		for (int i = 0; i < initialSpellsEntry->second.spells_size(); ++i)
		{
			const auto &spellid = initialSpellsEntry->second.spells(i);
			const auto *spell = m_project.spells.getById(spellid);
			if (spell)
			{
				initialSpells.push_back(spell);
			}
		}

		// Create character
		game::ResponseCode result = m_database.createCharacter(m_accountId, initialSpells, items, character);
		if (result == game::response_code::CharCreateSuccess)
		{
			// Cache the character data
			m_characters.push_back(character);

			// Add initial action buttons
			const auto classBtns = race->initialactionbuttons().find(character.class_);
			if (classBtns != race->initialactionbuttons().end())
			{
				wowpp::ActionButtons buttons;
				for (const auto &btn : classBtns->second.actionbuttons())
				{
					auto &entry = buttons[btn.first];
					entry.action = btn.second.action();
					entry.misc = btn.second.misc();
					entry.state = static_cast<ActionButtonUpdateState>(btn.second.state());
					entry.type = btn.second.type();
				}

				m_database.setCharacterActionButtons(character.id, buttons);
			}
		}

		// Send disabled message for now
		sendPacket(
			std::bind(game::server_write::charCreate, std::placeholders::_1, result));

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleCharDelete(game::IncomingPacket &packet)
	{
		// Read packet
		DatabaseId characterId;
		if (!(game::client_read::charDelete(packet, characterId)))
		{
			return PacketParseResult::Disconnect;
		}

		// Try to remove character from the cache
		const auto c = std::find_if(
			m_characters.begin(),
			m_characters.end(),
			[characterId](const game::CharEntry &c)
		{
			return (characterId == c.id);
		});

		// Did we find him?
		if (c == m_characters.end())
		{
			// We didn't find him
			sendPacket(
				std::bind(game::server_write::charDelete, std::placeholders::_1, game::response_code::CharDeleteFailed));
			return PacketParseResult::Pass;
		}

		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::OutgoingPacket outPacket(sink);
		game::server_write::friendStatus(outPacket, characterId, game::friend_result::Removed, game::SocialInfo());

		// Remove ourself from friend lists
		m_manager.foreachPlayer([characterId, &outPacket, &buffer](Player &player)
		{
			auto &social = player.getSocial();
			auto result = social.removeFromSocialList(characterId, false);
			if (result == game::friend_result::Removed)
			{
				player.sendProxyPacket(outPacket.getOpCode(), buffer);
			}
			social.removeFromSocialList(characterId, true);
		});

		// Remove character from cache
		m_characters.erase(c);

		// Prepare async delete request
		DeleteCharacterArgs arguments;
		arguments.accountId = m_accountId;
		arguments.characterId = characterId;
		auto handler = bind_weak_ptr(shared_from_this(), &Player::handleDeleteCharacter);
		m_asyncDatabase.asyncRequest(std::move(handler), &IDatabase::deleteCharacter, arguments);

		// Wait for delete to be completed
		return PacketParseResult::Block;
	}

	PacketParseResult Player::handlePlayerLogin(game::IncomingPacket &packet)
	{
		// Get the character id with which the player wants to enter the world
		DatabaseId characterId;
		if (!(game::client_read::playerLogin(packet, characterId)))
		{
			return PacketParseResult::Disconnect;
		}

		// Check if the requested character belongs to our account
		game::CharEntry *charEntry = getCharacterById(characterId);
		if (!charEntry)
		{
			// It seems like we don't own the requested character
			WLOG("Requested character id " << characterId << " does not belong to account " << m_accountId << " or does not exist");
			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoCharacter));
			return PacketParseResult::Disconnect;
		}

		// Make sure that a character, who is flagged for rename, is renamed first!
		if (charEntry->atLogin & game::atlogin_flags::Rename)
		{
			return PacketParseResult::Disconnect;
		}

		// Write something to the log just for informations
		ILOG("Player " << m_accountName << " tries to enter the world with character 0x" << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << characterId);

		// Load the player character data from the database
		std::shared_ptr<GameCharacter> character(new GameCharacter(m_project, m_manager.getTimers()));
		character->initialize();
		character->setGuid(createRealmGUID(characterId, m_loginConnector.getRealmID(), guid_type::Player));
		if (!m_database.getGameCharacter(guidLowerPart(characterId), *character))
		{
			// Send error packet
			WLOG("Player login failed: Could not load character " << characterId);
			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoCharacter));
			return PacketParseResult::Pass;
		}

		// Restore character group if possible
		UInt32 groupInstanceId = std::numeric_limits<UInt32>::max();
		if (character->getGroupId() != 0)
		{
			auto groupIt = PlayerGroup::GroupsById.find(character->getGroupId());
			if (groupIt != PlayerGroup::GroupsById.end())
			{
				// Apply character group
				m_group = groupIt->second;
				groupInstanceId = m_group->instanceBindingForMap(charEntry->mapId);
			}
			else
			{
				WLOG("Failed to restore character group");
				character->setGroupId(0);
			}
		}

		// We found the character - now we need to look for a world node
		// which is hosting a fitting world instance or is able to create
		// a new one

		auto *worldNode = m_worldManager.getWorldByMapId(charEntry->mapId);
		if (!worldNode)
		{
			// World does not exist
			WLOG("Player login failed: Could not find world server for map " << charEntry->mapId);
			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoWorld));
			return PacketParseResult::Pass;
		}

		// Store character id
		m_characterId = characterId;

		// Use the new character
		m_gameCharacter = std::move(character);
		m_gameCharacter->setZone(charEntry->zoneId);

		// TEST: If it is a hunter, set ammo
		if (m_gameCharacter->getClass() == game::char_class::Hunter)
		{
			m_gameCharacter->setUInt32Value(character_fields::AmmoId, 2512);
		}

		// Load the social list
		m_social.reset(new PlayerSocial(m_manager, *this));
		auto socialEntries = m_database.getCharacterSocialList(m_characterId);
		if (socialEntries)
		{
			for (auto &entry : *socialEntries)
			{
				const bool isFriend = (entry.flags & game::Friend);
				m_social->addToSocialList(entry.guid, !isFriend);

				if (isFriend)
				{
					m_social->setFriendNote(entry.guid, std::move(entry.note));
				}
			}
		}

		// Load action buttons
		m_actionButtons.clear();
		m_database.getCharacterActionButtons(m_characterId, m_actionButtons);

		// When the world node disconnects while we try to log in, send error packet
		m_worldDisconnected = worldNode->onConnectionLost.connect([this]()
		{
			m_gameCharacter.reset();
			m_worldNode = nullptr;
			m_instanceId = std::numeric_limits<UInt32>::max();

			sendPacket(
				std::bind(game::server_write::charLoginFailed, std::placeholders::_1, game::response_code::CharLoginNoWorld));

			m_worldDisconnected.disconnect();
		});

		// There should be an instance
		worldNode->enterWorldInstance(charEntry->id, groupInstanceId, *m_gameCharacter, m_locale);
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleNameQuery(game::IncomingPacket &packet)
	{
		// Object guid
		UInt64 objectGuid;
		if (!game::client_read::nameQuery(packet, objectGuid))
		{
			// Could not read packet
			return PacketParseResult::Disconnect;
		}

		// Get the realm ID out of this GUID and check if this is a player
		UInt32 realmID = guidRealmID(objectGuid);
		if (realmID != m_loginConnector.getRealmID())
		{
			// Redirect the request to the current world node
			auto world = m_worldManager.getWorldByInstanceId(m_instanceId);
			if (world)
			{
				// Decrypt position
				io::MemorySource *memorySource = static_cast<io::MemorySource*>(packet.getSource());

				// Buffer
				const std::vector<char> packetBuffer(memorySource->getBegin(), memorySource->getEnd());
				world->sendProxyPacket(m_characterId, game::client_packet::NameQuery, packetBuffer.size(), packetBuffer);
			}
			else
			{
				WLOG("Could not get world node to redirect name query request!");
			}
		}
		else
		{
			// Get the character db id
			UInt32 databaseID = guidLowerPart(objectGuid);

			// Look for the specified player
			auto handler = bind_weak_ptr(shared_from_this(), &Player::handleCharacterName);
			m_asyncDatabase.asyncRequest(std::move(handler), &IDatabase::getCharacterById, databaseID);
		}

		// We won't block name requests
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleMessageChat(game::IncomingPacket &packet)
	{
		game::ChatMsg type;
		game::Language lang;
		String receiver, channel, message;
		if (!game::client_read::messageChat(packet, type, lang, receiver, channel, message))
		{
			// Error reading packet
			return PacketParseResult::Disconnect;
		}

		if (!receiver.empty())
			capitalize(receiver);

		switch (type)
		{
			// Local chat modes
			case game::chat_msg::Say:
			case game::chat_msg::Yell:
			case game::chat_msg::Emote:
			case game::chat_msg::TextEmote:
			{
				// Redirect chat message to world node
				m_worldNode->sendChatMessage(
					m_gameCharacter->getGuid(),
					type,
					lang,
					receiver,
					channel,
					message);
				break;
			}
			// 
			case game::chat_msg::Whisper:
			{
				// Try to extract the realm name
				std::vector<String> nameElements;
				split(receiver, '-', nameElements);

				// Get the realm name (lower case)
				String realmName = m_config.internalName;
				boost::algorithm::to_lower(realmName);

				// Check if a realm name was provided
				if (nameElements.size() > 1)
				{
					String targetRealm = nameElements[1];
					boost::algorithm::to_lower(targetRealm);

					// There is a realm name - check if it is this realm
					if (targetRealm != realmName)
					{
						// It is another realm - redirect to the world node
						m_worldNode->sendChatMessage(
							m_gameCharacter->getGuid(),
							type,
							lang,
							receiver,
							channel,
							message);
						return PacketParseResult::Pass;
					}
					else
					{
						receiver = nameElements[0];
						capitalize(receiver);
					}
				}

				// Get player guid by name
				std::weak_ptr<Player> weakThis = shared_from_this();
				m_asyncDatabase.asyncRequest([weakThis, receiver, lang, channel, message](const boost::optional<game::CharEntry> &entry) 
				{
					if (auto this_ = weakThis.lock())
					{
						if (!entry)
						{
							this_->sendPacket(
								std::bind(game::server_write::chatPlayerNotFound, std::placeholders::_1, std::cref(receiver)));
							return;
						}

						// Check faction
						const bool isAllianceA = ((game::race::Alliance & (1 << (this_->m_gameCharacter->getRace() - 1))) == (1 << (this_->m_gameCharacter->getRace() - 1)));
						const bool isAllianceB = ((game::race::Alliance & (1 << (entry->race - 1))) == (1 << (entry->race - 1)));
						if (isAllianceA != isAllianceB)
						{
							this_->sendPacket(
								std::bind(game::server_write::chatWrongFaction, std::placeholders::_1));
							return;
						}

						// Make realm GUID
						UInt64 guid = createRealmGUID(entry->id, this_->m_loginConnector.getRealmID(), guid_type::Player);

						// Check if that player is online right now
						auto other = this_->m_manager.getPlayerByCharacterGuid(guid);
						if (!other)
						{
							this_->sendPacket(
								std::bind(game::server_write::chatPlayerNotFound, std::placeholders::_1, std::cref(receiver)));
							return;
						}

						if (other->getSocial().isIgnored(this_->m_gameCharacter->getGuid()))
						{
							DLOG("TODO: Other player ignores us - notify our client about this...");
							return;
						}

						// TODO: Check if that player is a GM and if he accepts whispers from us, eventually block

						// Change language if needed so that whispers are always readable
						auto language = (lang != game::language::Addon) ? game::language::Universal : lang;

						// Send whisper message
						other->sendPacket(
							std::bind(game::server_write::messageChat, std::placeholders::_1, game::chat_msg::Whisper, language, std::cref(channel), this_->m_characterId, std::cref(message), this_->m_gameCharacter.get()));

						// If not an addon message, send reply message
						if (language != game::language::Addon)
						{
							this_->sendPacket(
								std::bind(game::server_write::messageChat, std::placeholders::_1, game::chat_msg::Reply, language, std::cref(channel), guid, std::cref(message), this_->m_gameCharacter.get()));
						}
					}
				}, &IDatabase::getCharacterByName, receiver);
				break;
			}
			case game::chat_msg::Raid:
			case game::chat_msg::Party:
			case game::chat_msg::RaidLeader:
			case game::chat_msg::RaidWarning:
			{
				// Get the players group
				if (!m_group)
				{
					return PacketParseResult::Disconnect;
				}

				auto groupType = m_group->getType();
				if (groupType == group_type::Raid)
				{
					if (type == game::chat_msg::Party)
					{
						// Only affect players subgroup
						DLOG("TODO: Player subgroup");
					}

					const bool isRaidLead = m_group->getLeader() == m_gameCharacter->getGuid();
					const bool isAssistant = m_group->isLeaderOrAssistant(m_gameCharacter->getGuid());
					if (type == game::chat_msg::RaidLeader &&
						!isRaidLead)
					{
						type = game::chat_msg::Raid;
					}
					else if (type == game::chat_msg::RaidWarning &&
						!isRaidLead &&
						!isAssistant)
					{
						WLOG("Raid warning can only be done by raid leader or assistants");
						return PacketParseResult::Disconnect;
					}
				}
				else
				{
					if (type == game::chat_msg::Raid)
					{
						DLOG("Not a raid group!");
						return PacketParseResult::Disconnect;
					}
				}

				// Maybe we were just invited, but are not yet a member of that group
				if (!m_group->isMember(m_gameCharacter->getGuid()))
				{
					WLOG("Player is not a member of the group, but was just invited.");
					return PacketParseResult::Disconnect;
				}

				// Broadcast chat packet
				m_group->broadcastPacket(
					std::bind(game::server_write::messageChat, std::placeholders::_1, type, lang, std::cref(channel), m_characterId, std::cref(message), m_gameCharacter.get()), nullptr, m_gameCharacter->getGuid());

				break;
			}
			// Can be local or global chat mode
			case game::chat_msg::Channel:
			{
				WLOG("Channel Chat mode not yet implemented");
				break;
			}
			// Global chat modes / other
			default:
			{
				WLOG("Chat mode not yet implemented");
				break;
			}
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleContactList(game::IncomingPacket &packet)
	{
		if (!game::client_read::contactList(packet))
		{
			// Error reading packet
			return PacketParseResult::Disconnect;
		}

		// TODO: Only update the friend list after a specific time interval to prevent 
		// spamming of this command

		// Send the social list
		m_social->sendSocialList();
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleAddFriend(game::IncomingPacket &packet)
	{
		String name, note;
		if (!game::client_read::addFriend(packet, name, note))
		{
			// Error reading packet
			return PacketParseResult::Disconnect;
		}

		if (name.empty())
		{
			return PacketParseResult::Disconnect;
		}

		// Convert name
		capitalize(name);

		// Build the handler
		auto handler = std::bind<void>(bind_weak_ptr(shared_from_this(), &Player::handleAddFriendRequest), std::placeholders::_1, std::move(note));
		m_asyncDatabase.asyncRequest(std::move(handler), &IDatabase::getCharacterByName, name);
		return PacketParseResult::Block;
	}

	PacketParseResult Player::handleDeleteFriend(game::IncomingPacket &packet)
	{
		UInt64 guid;
		if (!game::client_read::deleteFriend(packet, guid))
		{
			// Error reading packet
			return PacketParseResult::Disconnect;
		}

		// Remove that friend from our social list
		auto result = m_social->removeFromSocialList(guid, false);
		if (result == game::friend_result::Removed)
		{
			try
			{
				if (m_social->isIgnored(guid))
				{
					// Old friend is still ignored - update flags
					m_database.updateCharacterSocialContact(m_characterId, guid, game::Ignored, "");
				}
				else
				{
					// Completely remove contact, as he is neither ignored nor a friend
					m_database.removeCharacterSocialContact(m_characterId, guid);
				}
			}
			catch (const std::exception& ex)
			{
				ELOG("Datbase exception: " << ex.what());
				result = game::friend_result::DatabaseError;
			}
		}

		game::SocialInfo info;
		sendPacket(
			std::bind(game::server_write::friendStatus, std::placeholders::_1, guid, result, std::cref(info)));
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleAddIgnore(game::IncomingPacket &packet)
	{
		String name;
		if (!game::client_read::addIgnore(packet, name) || name.empty())
		{
			// Error reading packet
			return PacketParseResult::Disconnect;
		}

		// Convert name
		capitalize(name);

		auto handler = bind_weak_ptr(shared_from_this(), &Player::handleAddIgnoreRequest);
		m_asyncDatabase.asyncRequest(std::move(handler), &IDatabase::getCharacterByName, name);
		return PacketParseResult::Block;
	}

	PacketParseResult Player::handleDeleteIgnore(game::IncomingPacket &packet)
	{
		UInt64 guid;
		if (!game::client_read::deleteIgnore(packet, guid))
		{
			return PacketParseResult::Disconnect;
		}

		// Find the character details
		game::CharEntry ignoredChar;

		// Fill ignored info
		game::SocialInfo info;

		//result
		auto result = m_social->removeFromSocialList(guid, true);
		if (result == game::friend_result::IgnoreRemoved)
		{
			try
			{
				if (m_social->isFriend(guid))
				{
					UpdateSocialContactArg arg;
					arg.characterId = m_characterId;
					arg.socialGuid = guid;
					arg.flags = game::Friend;
					m_database.updateCharacterSocialContact(arg);
				}
				else
				{
					m_database.removeCharacterSocialContact(m_characterId, guid);
				}
			}
			catch (const std::exception& ex)
			{
				ELOG("Database error: " << ex.what());
				result = game::friend_result::DatabaseError;
			}

			m_worldNode->characterRemoveIgnore(m_characterId, guid);
		}
		else
		{
			WLOG("handleDeleteIgnore: result " << result);
		}

		sendPacket(
			std::bind(game::server_write::friendStatus, std::placeholders::_1, guid, result, std::cref(info)));
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleItemQuerySingle(game::IncomingPacket &packet)
	{
		// Object guid
		UInt32 itemID;
		if (!game::client_read::itemQuerySingle(packet, itemID))
		{
			// Could not read packet
			return PacketParseResult::Disconnect;
		}

		// Find item
		const auto *item = m_project.items.getById(itemID);
		if (item)
		{
			// TODO: Cache multiple query requests and send one, bigger response with multiple items

			// Write answer packet
			sendPacket(
				std::bind(game::server_write::itemQuerySingleResponse, std::placeholders::_1, m_locale, std::cref(*item)));
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleCreatureQuery(game::IncomingPacket &packet)
	{
		// Read the client packet
		UInt32 creatureEntry;
		UInt64 objectGuid;
		if (!game::client_read::creatureQuery(packet, creatureEntry, objectGuid))
		{
			// Could not read packet
			return PacketParseResult::Disconnect;
		}

		//TODO: Find creature object and check if it exists

		// Find creature info by entry
		const auto *unit = m_project.units.getById(creatureEntry);
		if (unit)
		{
			// Write answer packet
			sendPacket(
				std::bind(game::server_write::creatureQueryResponse, std::placeholders::_1, m_locale, std::cref(*unit)));
		}
		else
		{
			//TODO: Send resulting packet SMSG_CREATURE_QUERY_RESPONSE with only one uin32 value
			//which is creatureEntry | 0x80000000
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleMailQueryNextTime(game::IncomingPacket & packet)
	{
		if (!game::client_read::mailQueryNextTime(packet))
		{
			return PacketParseResult::Disconnect;
		}

		sendPacket(
			std::bind(game::server_write::mailQueryNextTime, std::placeholders::_1, m_unreadMails, getMails()));

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleMailGetBody(game::IncomingPacket & packet)
	{
		UInt32 mailTextId, mailId;

		if (!game::client_read::mailGetBody(packet, mailTextId, mailId))
		{
			return PacketParseResult::Disconnect;
		}

		String body = getMail(mailId)->getBody();

		sendPacket(
			std::bind(game::server_write::mailSendBody, std::placeholders::_1, /*TODO: change to mailTextId*/mailId, body));

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleMailTakeMoney(game::IncomingPacket & packet)
	{
		ObjectGuid mailboxGuid;
		UInt32 mailId;

		if (!game::client_read::mailTakeMoney(packet, mailboxGuid, mailId))
		{
			return PacketParseResult::Disconnect;
		}

		// TODO check distance to mailbox, etc

		// TODO check state + delivery time
		auto mail = getMail(mailId);
		if (!mail)
		{
			sendPacket(
				std::bind(game::server_write::mailSendResult, std::placeholders::_1,
					MailResult(mailId, mail::response_type::MoneyTaken, mail::response_result::Internal)));
			return PacketParseResult::Pass;
		}

		sendPacket(
			std::bind(game::server_write::mailSendResult, std::placeholders::_1,
				MailResult(mailId, mail::response_type::MoneyTaken, mail::response_result::Ok)));

		m_worldNode->changeMoney(m_characterId, mail->getMoney(), false);
		mail->setMoney(0);
		// TODO change mail state

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleMailDelete(game::IncomingPacket & packet)
	{
		ObjectGuid mailboxGuid;
		UInt32 mailId;

		if (!game::client_read::mailDelete(packet, mailboxGuid, mailId))
		{
			return PacketParseResult::Disconnect;
		}

		// TODO check distance to mailbox, etc

		auto mail = getMail(mailId);
		if (mail && mail->getCOD() == 0)
		{
			sendPacket(
				std::bind(game::server_write::mailSendResult, std::placeholders::_1,
					MailResult(mailId, mail::response_type::Deleted, mail::response_result::Ok)));

			// TODO change mail state (delete it maybe ?)
		}
		else
		{
			sendPacket(
				std::bind(game::server_write::mailSendResult, std::placeholders::_1,
					MailResult(mailId, mail::response_type::Deleted, mail::response_result::Internal)));
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGetChannelMemberCount(game::IncomingPacket & packet)
	{
		// TODO
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGroupInvite(game::IncomingPacket &packet)
	{
		String playerName;
		if (!game::client_read::groupInvite(packet, playerName))
		{
			// Could not read packet
			return PacketParseResult::Disconnect;
		}

		// Capitalize player name
		capitalize(playerName);

		// Try to find that player
		auto player = m_manager.getPlayerByCharacterName(playerName);
		if (!player)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, std::cref(playerName), game::party_result::CantFindTarget));
			return PacketParseResult::Pass;
		}

		// Check that players character faction
		auto *character = player->getGameCharacter();
		if (!character)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, std::cref(playerName), game::party_result::CantFindTarget));
			return PacketParseResult::Pass;
		}

		// Check team (no cross-faction groups)
		const bool isAllianceA = ((game::race::Alliance & (1 << (m_gameCharacter->getRace() - 1))) == (1 << (m_gameCharacter->getRace() - 1)));
		const bool isAllianceB = ((game::race::Alliance & (1 << (character->getRace() - 1))) == (1 << (character->getRace() - 1)));
		if (isAllianceA != isAllianceB)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, std::cref(playerName), game::party_result::TargetUnfriendly));
			return PacketParseResult::Pass;
		}

		// Check if target is already member of a group
		if (player->getGroup())
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, std::cref(playerName), game::party_result::AlreadyInGroup));
			return PacketParseResult::Pass;
		}

		if (player->getSocial().isIgnored(m_gameCharacter->getGuid()))
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, std::cref(playerName), game::party_result::TargetIgnoreYou));
			return PacketParseResult::Pass;
		}

		// Get players group or create a new one
		if (!m_group)
		{
			// Create the group
			m_group = std::make_shared<PlayerGroup>(m_groupIdGenerator.generateId(), m_manager, m_database);
			m_group->create(*m_gameCharacter);

			// Save character group id
			m_gameCharacter->setGroupId(m_group->getId());

			// Send to world node
			m_worldNode->characterGroupChanged(m_gameCharacter->getGuid(), m_group->getId());
		}

		// Check if we are the leader of that group
		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, "", game::party_result::YouNotLeader));
			return PacketParseResult::Pass;
		}

		// Invite player to the group
		auto result = m_group->addInvite(character->getGuid());
		if (result != game::party_result::Ok)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, "", result));
			return PacketParseResult::Pass;
		}

		player->setGroup(m_group);
		player->sendPacket(
			std::bind(game::server_write::groupInvite, std::placeholders::_1, std::cref(m_gameCharacter->getName())));

		// Send result
		sendPacket(
			std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, std::cref(playerName), game::party_result::Ok));
		m_group->sendUpdate();
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGroupAccept(game::IncomingPacket &packet)
	{
		if (!game::client_read::groupAccept(packet))
		{
			return PacketParseResult::Disconnect;
		}

		if (!m_group)
		{
			WLOG("Player accepted group invitation, but is not in a group");
			return PacketParseResult::Disconnect;
		}

		auto result = m_group->addMember(*m_gameCharacter);
		if (result != game::party_result::Ok)
		{
			// TODO...
			return PacketParseResult::Pass;
		}

		m_gameCharacter->setGroupId(m_group->getId());

		// Send to world node
		m_worldNode->characterGroupChanged(m_gameCharacter->getGuid(), m_group->getId());
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGroupDecline(game::IncomingPacket &packet)
	{
		if (!game::client_read::groupDecline(packet))
		{
			return PacketParseResult::Disconnect;
		}

		declineGroupInvite();
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGroupUninvite(game::IncomingPacket &packet)
	{
		String memberName;
		if (!game::client_read::groupUninvite(packet, memberName))
		{
			return PacketParseResult::Disconnect;
		}

		// Capitalize player name
		capitalize(memberName);

		if (!m_group)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotInGroup));
			return PacketParseResult::Pass;
		}

		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotLeader));
			return PacketParseResult::Pass;
		}

		UInt64 guid = m_group->getMemberGuid(memberName);
		if (guid == 0)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, std::cref(memberName), game::party_result::NotInYourParty));
			return PacketParseResult::Pass;
		}

		// Raid assistants may not kick the leader
		if (m_gameCharacter->getGuid() != m_group->getLeader() &&
			m_group->getLeader() == guid)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotLeader));
			return PacketParseResult::Pass;
		}

		m_group->removeMember(guid);
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGroupUninviteGUID(game::IncomingPacket &packet)
	{
		UInt64 memberGUID;
		if (!game::client_read::groupUninviteGUID(packet, memberGUID))
		{
			return PacketParseResult::Disconnect;
		}

		if (!m_group)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotInGroup));
			return PacketParseResult::Pass;
		}

		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotLeader));
			return PacketParseResult::Pass;
		}

		if (!m_group->isMember(memberGUID))
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::NotInYourParty));
			return PacketParseResult::Pass;
		}

		// Raid assistants may not kick the leader
		if (m_gameCharacter->getGuid() != m_group->getLeader() &&
			m_group->getLeader() == memberGUID)
		{
			sendPacket(
				std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Leave, "", game::party_result::YouNotLeader));
			return PacketParseResult::Pass;
		}

		m_group->removeMember(memberGUID);
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGroupSetLeader(game::IncomingPacket &packet)
	{
		UInt64 leaderGUID;
		if (!game::client_read::groupSetLeader(packet, leaderGUID))
		{
			return PacketParseResult::Disconnect;
		}

		if (!m_group)
		{
			WLOG("Player is not a member of a group!");
			return PacketParseResult::Disconnect;
		}

		if (m_group->getLeader() == leaderGUID ||
			m_group->getLeader() != m_gameCharacter->getGuid())
		{
			WLOG("Player is not the group leader or no leader change");
			return PacketParseResult::Disconnect;
		}

		m_group->setLeader(leaderGUID);
		m_group->sendUpdate();
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleLootMethod(game::IncomingPacket &packet)
	{
		UInt32 lootMethod, lootTreshold;
		UInt64 lootMasterGUID;
		if (!game::client_read::lootMethod(packet, lootMethod, lootMasterGUID, lootTreshold))
		{
			return PacketParseResult::Disconnect;
		}

		if (!m_group)
		{
			return PacketParseResult::Disconnect;
		}

		if (m_group->getLeader() != m_gameCharacter->getGuid())
		{
			return PacketParseResult::Disconnect;
		}
		if (lootMethod > loot_method::NeedBeforeGreed)
		{
			return PacketParseResult::Disconnect;
		}
		if (lootTreshold < 2 || lootTreshold > 6)
		{
			return PacketParseResult::Disconnect;
		}
		if (lootMethod == loot_method::MasterLoot &&
			!m_group->isMember(lootMasterGUID))
		{
			return PacketParseResult::Disconnect;
		}

		m_group->setLootMethod(static_cast<LootMethod>(lootMethod), lootMasterGUID, lootTreshold);
		m_group->sendUpdate();
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGroupDisband(game::IncomingPacket &packet)
	{
		if (!game::client_read::groupDisband(packet) ||
			!m_group)
		{
			return PacketParseResult::Disconnect;
		}

		// Remove this group member
		// Note: This will make m_group invalid
		m_group->removeMember(m_gameCharacter->getGuid());
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleRequestPartyMemberStats(game::IncomingPacket &packet)
	{
		UInt64 guid = 0;
		if (!game::client_read::requestPartyMemberStats(packet, guid))
		{
			return PacketParseResult::Disconnect;
		}

		// Try to find that player
		auto player = m_manager.getPlayerByCharacterGuid(guid);
		if (!player)
		{
			DLOG("Could not find player with character guid - send offline packet");
			sendPacket(
				std::bind(game::server_write::partyMemberStatsFullOffline, std::placeholders::_1, guid));
			return PacketParseResult::Pass;
		}

		// Send result
		sendPacket(
			std::bind(game::server_write::partyMemberStatsFull, std::placeholders::_1, std::cref(*m_gameCharacter)));
		return PacketParseResult::Pass;
	}


	PacketParseResult Player::handleMoveWorldPortAck(game::IncomingPacket &packet)
	{
		if (m_transferMap == 0 && m_transfer.x == 0.0f && m_transfer.y == 0.0f && m_transfer.z == 0.0f && m_transferO == 0.0f)
		{
			WLOG("No transfer pending - commit will be ignored.");
			return PacketParseResult::Pass;
		}

		// Update character location
		m_gameCharacter->setMapId(m_transferMap);
		m_gameCharacter->relocate(m_transfer, m_transferO);

		// We found the character - now we need to look for a world node
		// which is hosting a fitting world instance or is able to create
		// a new one

		UInt32 groupInstanceId = std::numeric_limits<UInt32>::max();

		// Determine group instance to join
		auto player = m_manager.getPlayerByCharacterGuid(m_gameCharacter->getGuid());
		if (player)
		{
			if (auto *group = player->getGroup())
			{
				groupInstanceId = group->instanceBindingForMap(m_transferMap);
			}
		}

		// Find a new world node
		auto *world = m_worldManager.getWorldByMapId(m_transferMap);
		if (!world)
		{
			// World does not exist
			WLOG("Player login failed: Could not find world server for map " << m_transferMap);
			sendPacket(
				std::bind(game::server_write::transferAborted, std::placeholders::_1, m_transferMap, game::transfer_abort_reason::NotFound));
			return PacketParseResult::Pass;
		}

		//TODO Map found - check if player is member of a group and if this instance
		// is valid on the world node and if not, transfer player

		// There should be an instance
		m_worldNode = world;
		m_worldNode->enterWorldInstance(m_characterId, groupInstanceId, *m_gameCharacter, m_locale);

		// Reset transfer data
		m_transferMap = 0;
		m_transfer = math::Vector3(0.0f, 0.0f, 0.0f);
		m_transferO = 0.0f;
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleSetActionButton(game::IncomingPacket &packet)
	{
		ActionButton button;
		UInt8 slot = 0;
		if (!game::client_read::setActionButton(packet, slot, button.misc, button.type, button.action))
		{
			return PacketParseResult::Disconnect;
		}

		// Validate button
		if (slot > constants::ActionButtonLimit)		// TODO: Maximum number of action buttons
		{
			WLOG("Client sent invalid action button number");
			return PacketParseResult::Disconnect;
		}

		// Check if we want to remove that button or add a new one
		if (button.action == 0)
		{
			auto it = m_actionButtons.find(slot);
			if (it == m_actionButtons.end())
			{
				WLOG("Could not find action button to remove - button seems to be empty already!");
				return PacketParseResult::Disconnect;
			}

			// Clear button
			m_actionButtons.erase(it);
		}
		else
		{
			m_actionButtons[slot] = button;
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGameObjectQuery(game::IncomingPacket &packet)
	{
		UInt32 entry = 0;
		UInt64 guid = 0;
		if (!game::client_read::gameObjectQuery(packet, entry, guid))
		{
			return PacketParseResult::Disconnect;
		}

		const auto *objectEntry = m_project.objects.getById(entry);
		if (!objectEntry)
		{
			WLOG("Could not find game object by entry " << entry);
			sendPacket(
				std::bind(game::server_write::gameObjectQueryResponseEmpty, std::placeholders::_1, entry));
			return PacketParseResult::Pass;
		}

		// Send response
		sendPacket(
			std::bind(game::server_write::gameObjectQueryResponse, std::placeholders::_1, m_locale, std::cref(*objectEntry)));
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleTutorialFlag(game::IncomingPacket &packet)
	{
		UInt32 flag = 0;
		if (!game::client_read::tutorialFlag(packet, flag))
		{
			return PacketParseResult::Disconnect;
		}

		UInt32 wInt = (flag / 32);
		if (wInt >= 8)
		{
			return PacketParseResult::Disconnect;
		}

		UInt32 rInt = (flag % 32);

		UInt32 &tutflag = m_tutorialData[wInt];
		tutflag |= (1 << rInt);

		m_loginConnector.sendTutorialData(m_accountId, m_tutorialData);
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleTutorialClear(game::IncomingPacket &packet)
	{
		if (!game::client_read::tutorialClear(packet))
		{
			return PacketParseResult::Disconnect;
		}

		m_tutorialData.fill(0xFFFFFFFF);
		m_loginConnector.sendTutorialData(m_accountId, m_tutorialData);
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleTutorialReset(game::IncomingPacket &packet)
	{
		if (!game::client_read::tutorialReset(packet))
		{
			return PacketParseResult::Disconnect;
		}

		m_tutorialData.fill(0);
		m_loginConnector.sendTutorialData(m_accountId, m_tutorialData);
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleCompleteCinematic(game::IncomingPacket &packet)
	{
		if (!game::client_read::completeCinematic(packet))
		{
			return PacketParseResult::Disconnect;
		}

		m_database.setCinematicState(m_characterId, false);
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleRaidTargetUpdate(game::IncomingPacket &packet)
	{
		UInt8 mode = 0;
		UInt64 guid = 0;
		if (!(game::client_read::raidTargetUpdate(packet, mode, guid)))
		{
			return PacketParseResult::Disconnect;
		}

		if (!m_group)
		{
			return PacketParseResult::Disconnect;
		}
		if (!m_group->isMember(m_gameCharacter->getGuid()))
		{
			return PacketParseResult::Disconnect;
		}

		if (mode == 0xFF)
		{
			m_group->sendTargetList(*this);
		}
		else
		{
			if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
			{
				WLOG("Only the group leader is allowed to update raid target icons!");
				return PacketParseResult::Disconnect;
			}

			m_group->setTargetIcon(mode, guid);
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGroupRaidConvert(game::IncomingPacket &packet)
	{
		if (!(game::client_read::groupRaidConvert(packet)))
		{
			return PacketParseResult::Disconnect;
		}

		const bool isLeader = (m_group && (m_group->getLeader() == m_gameCharacter->getGuid()));
		if (!isLeader)
		{
			return PacketParseResult::Disconnect;
		}

		sendPacket(
			std::bind(game::server_write::partyCommandResult, std::placeholders::_1, game::party_operation::Invite, "", game::party_result::Ok));
		m_group->convertToRaidGroup();
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleGroupAssistentLeader(game::IncomingPacket &packet)
	{
		UInt64 guid;
		UInt8 flags;
		if (!(game::client_read::groupAssistentLeader(packet, guid, flags)))
		{
			return PacketParseResult::Disconnect;
		}

		if (!m_group)
		{
			return PacketParseResult::Disconnect;
		}
		if (m_group->getType() != group_type::Raid)
		{
			return PacketParseResult::Disconnect;
		}
		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
		{
			return PacketParseResult::Disconnect;
		}

		m_group->setAssistant(guid, flags);
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleRaidReadyCheck(game::IncomingPacket &packet)
	{
		bool hasState = false;
		UInt8 state = 0;
		if (!(game::client_read::raidReadyCheck(packet, hasState, state)))
		{
			return PacketParseResult::Disconnect;
		}

		if (!m_group)
		{
			return PacketParseResult::Disconnect;
		}
		if (!m_group->isMember(m_gameCharacter->getGuid()))
		{
			return PacketParseResult::Disconnect;
		}

		if (!hasState)
		{
			if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
			{
				return PacketParseResult::Disconnect;
			}

			// Ready check request
			m_group->broadcastPacket(
				std::bind(game::server_write::raidReadyCheck, std::placeholders::_1, m_gameCharacter->getGuid()));
		}
		else
		{
			// Ready check request
			m_group->broadcastPacket(
				std::bind(game::server_write::raidReadyCheckConfirm, std::placeholders::_1, m_gameCharacter->getGuid(), state));
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleRaidReadyCheckFinished(game::IncomingPacket &packet)
	{
		if (!m_group)
		{
			return PacketParseResult::Disconnect;
		}
		if (!m_group->isLeaderOrAssistant(m_gameCharacter->getGuid()))
		{
			return PacketParseResult::Disconnect;
		}

		// Ready check request
		m_group->broadcastPacket(
			std::bind(game::server_write::raidReadyCheckFinished, std::placeholders::_1));

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleRealmSplit(game::IncomingPacket & packet)
	{
		UInt32 preferred = 0;
		if (!(game::client_read::realmSplit(packet, preferred)))
		{
			return PacketParseResult::Disconnect;
		}

		if (preferred == 0xFFFFFFFF)
		{
			// The player requests his preferred realm id from the server
			// TODO: Determine if a realm split is happening and if so, send SMSG_REALM_SPLIT message
		}
		else
		{
			DLOG("TODO...");
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleVoiceSessionEnable(game::IncomingPacket & packet)
	{
		UInt16 unknown = 0;
		if (!(game::client_read::voiceSessionEnable(packet, unknown)))
		{
			return PacketParseResult::Disconnect;
		}

		// TODO
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleCharRename(game::IncomingPacket & packet)
	{
		UInt64 characterId = 0;
		String newName;
		if (!(game::client_read::charRename(packet, characterId, newName)))
		{
			return PacketParseResult::Disconnect;
		}
		
		// Rename character
		game::ResponseCode response = game::response_code::CharCreateError;
		for (auto &entry : m_characters)
		{
			if (entry.id == characterId)
			{
				// Capitalize the characters name
				capitalize(newName);

				response = m_database.renameCharacter(characterId, newName);
				if (response == game::response_code::Success)
				{
					// Fix entry
					entry.name = newName;
					entry.atLogin = static_cast<game::AtLoginFlags>(entry.atLogin & ~game::atlogin_flags::Rename);
				}

				break;
			}
		}

		// Send response
		sendPacket(
			std::bind(game::server_write::charRename, std::placeholders::_1, response, characterId, std::cref(newName)));

		if (response == game::response_code::Success)
		{
			// Update character name for all connected players
			m_manager.foreachPlayer([characterId](Player& player) {
				if (player.getCharacterId() != 0 && player.getCharacterId() != characterId) {
					player.sendPacket(std::bind(game::server_write::invalidatePlayer, std::placeholders::_1, characterId));
				}
			});
		}

		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleQuestQuery(game::IncomingPacket & packet)
	{
		UInt32 questId = 0;
		if (!(game::client_read::questQuery(packet, questId)))
		{
			return PacketParseResult::Disconnect;
		}

		const auto *quest = m_project.quests.getById(questId);
		if (!quest)
		{
			return PacketParseResult::Disconnect;
		}

		// Send response
		sendPacket(
			std::bind(game::server_write::questQueryResponse, std::placeholders::_1, m_locale, std::cref(*quest)));
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleWho(game::IncomingPacket & packet)
	{
		// Timer protection
		GameTime now = getCurrentTime();
		if (now < m_nextWhoRequest)
		{
			// Don't do that yet
			return PacketParseResult::Pass;
		}

		// Allow one request every 6 seconds
		m_nextWhoRequest = now + constants::OneSecond * 6;

		// Read request packet
		game::WhoListRequest request;
		if (!game::client_read::who(packet, request))
		{
			return PacketParseResult::Disconnect;
		}

		// Check if the packet can be valid, as WoW allows a maximum of 10 zones and 4 strings
		// per request.
		if (request.zoneids.size() > 10 || request.strings.size() > 4)
		{
			return PacketParseResult::Disconnect;
		}

		if (!m_gameCharacter)
			return PacketParseResult::Disconnect;

		// Used for response packet
		game::WhoResponse response;
		const bool is_this_Alliance = ((game::race::Alliance & (1 << (m_gameCharacter->getRace() - 1))) == (1 << (m_gameCharacter->getRace() - 1)));

		// Iterate through all connected players
		for (auto &player : m_manager.getPlayers())
		{
			// Maximum of 50 allowed characters
			if (response.entries.size() >= 50)
			{
				// STOP the loop
				break;
			}

			// Check if the player has a valid game character
			auto *character = player->getGameCharacter();
			if (!character)
			{
				continue;
			}

			//is this player we are looking for alliance??
			const bool isAlliance = ((game::race::Alliance & (1 << (character->getRace() - 1))) == (1 << (character->getRace() - 1)));
			if (is_this_Alliance != isAlliance)
			{
				//cant find players that are not same faction
				continue;
			}

			// Check if that game character is currently in a world
			if (!player->getWorldNode())
			{
				continue;
			}

			// Check if levels match, first, as this is the least expensive thing to check
			const UInt32 level = character->getLevel();
			if (level < request.level_min || level > request.level_max)
			{
				continue;
			}

			//search by racemask
			if (0 != request.racemask)
			{
				UInt32 race = character->getRace();
				if (!(request.racemask & (1 << race)))
				{
					continue;
				}
			}

			//search by classmask
			if (0 != request.classmask)
			{
				UInt32 _class = character->getClass();
				if (!(request.classmask & (1 << _class)))
				{
					continue;
				}
			}

			//search by zoneID
			if (!request.zoneids.empty())
			{
				bool found_by_zoneid = false;

				for (auto zone : request.zoneids)
				{
					if (zone == character->getZone())
					{
						found_by_zoneid = true;
						break;
					}
				}

				if (found_by_zoneid == false)
				{
					continue;
				}
			}

			// Convert character name to lower case string
			String charNameLowered = character->getName();
			std::transform(charNameLowered.begin(), charNameLowered.end(), charNameLowered.begin(), ::tolower);

			// Validate character name
			if (!request.player_name.empty())
			{
				String searchLowered = request.player_name;
				std::transform(searchLowered.begin(), searchLowered.end(), searchLowered.begin(), ::tolower);

				// Part not found
				if (charNameLowered.find(searchLowered) == String::npos)
				{
					continue;
				}
			}

			// Validate strings
			bool passedStringTest = request.strings.empty();
			for (const auto &string : request.strings)
			{
				String searchLowered = string;
				std::transform(searchLowered.begin(), searchLowered.end(), searchLowered.begin(), ::tolower);

				// TODO: These strings also have to be validated against the guild name.
				// the players name

				//get zone name
				auto zone = m_project.zones.getById(character->getZone());
				auto zone_name = zone->name();
				std::transform(zone_name.begin(), zone_name.end(), zone_name.begin(), ::tolower);

				if ((charNameLowered.find(searchLowered) != String::npos) || (zone_name.find(searchLowered) != String::npos))
				{
					passedStringTest = true;
					break;
				}


			}

			// Skip this character if not passed string test
			if (!passedStringTest)
			{
				continue;
			}

			// Add new response entry, as all checks above were fulfilled
			game::WhoResponseEntry entry(*character);
			response.entries.push_back(std::move(entry));
		}

		// Match Count is request count right now (TODO)
		sendPacket(
			std::bind(game::server_write::whoRequestResponse, std::placeholders::_1, response, response.entries.size()));
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleMinimapPing(game::IncomingPacket & packet)
	{
		float x = 0.0f, y = 0.0f;
		if (!(game::client_read::minimapPing(packet, x, y)))
		{
			return PacketParseResult::Disconnect;
		}

		// Only accepted while in group
		if (!m_group)
		{
			return PacketParseResult::Disconnect;
		}

		// TODO: Only send to group members in the same map instance?

		std::vector<UInt64> excludeGuids;
		excludeGuids.push_back(m_gameCharacter->getGuid());

		// Broadcast packet to group
		auto generator = std::bind(game::server_write::minimapPing, std::placeholders::_1, m_gameCharacter->getGuid(), x, y);
		m_group->broadcastPacket(generator, &excludeGuids);
		return PacketParseResult::Pass;
	}

	PacketParseResult Player::handleItemNameQuery(game::IncomingPacket & packet)
	{
		UInt32 itemEntry = 0;
		UInt64 itemGuid = 0;
		if (!(game::client_read::itemNameQuery(packet, itemEntry, itemGuid)))
		{
			return PacketParseResult::Disconnect;
		}

		const auto *entry = m_project.items.getById(itemEntry);
		if (!entry)
		{
			return PacketParseResult::Disconnect;
		}

		String name =
			entry->name_loc_size() >= static_cast<Int32>(m_locale) ?
			entry->name_loc(static_cast<Int32>(m_locale) - 1) : entry->name();
		if (name.empty()) name = entry->name();

		// Match Count is request count right now
		sendPacket(
			std::bind(game::server_write::itemNameQueryResponse, std::placeholders::_1, itemEntry, std::cref(name), entry->inventorytype()));
		return PacketParseResult::Pass;
	}
}
