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
#include "realm_connector.h"
#include "game/world_instance_manager.h"
#include "game/world_instance.h"
#include "player_manager.h"
#include "player.h"
#include "game/game_creature.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "game_protocol/game_protocol.h"
#include "configuration.h"
#include "common/clock.h"
#include "proto_data/project.h"
#include "game/game_world_object.h"
#include "game/visibility_tile.h"
#include "game/each_tile_in_region.h"
#include "game/universe.h"
#include "game/each_tile_in_sight.h"
#include "binary_io/vector_sink.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	static const auto ReconnectDelay = (constants::OneSecond * 4);
	static const auto KeepAliveDelay = (constants::OneMinute / 2);


	RealmConnector::RealmConnector(
		boost::asio::io_service &ioService, 
		WorldInstanceManager &worldInstanceManager, 
		PlayerManager &playerManager, 
		const Configuration &config,
		UInt32 realmEntryIndex,
		proto::Project &project, 
		TimerQueue &timer
		)
		: m_ioService(ioService)
		, m_worldInstanceManager(worldInstanceManager)
		, m_playerManager(playerManager)
		, m_config(config)
		, m_project(project)
		, m_timer(timer)
		, m_realmEntryIndex(realmEntryIndex)
		, m_realmName("UNKNOWN")
	{
		tryConnect();
	}

	RealmConnector::~RealmConnector()
	{
	}

	void RealmConnector::connectionLost()
	{
		const auto &realm = m_config.realms[m_realmEntryIndex];
		WLOG("Lost connection with the realm server at " << realm.realmAddress << ":" << realm.realmPort);
		m_connection->resetListener();
		m_connection.reset();
		scheduleConnect();
	}

	void RealmConnector::connectionMalformedPacket()
	{
		WLOG("Received malformed packet from the realm server");
		m_connection->resetListener();
		m_connection.reset();
	}

	void RealmConnector::connectionPacketReceived(pp::Protocol::IncomingPacket &packet)
	{
		const auto &packetId = packet.getId();

		switch (packetId)
		{
			case pp::world_realm::realm_packet::LoginAnswer:
				handleLoginAnswer(packet);
				break;
			case pp::world_realm::realm_packet::CharacterLogIn:
				handleCharacterLogin(packet);
				break;
			case pp::world_realm::realm_packet::ClientProxyPacket:
				handleProxyPacket(packet);
				break;
			case pp::world_realm::realm_packet::ChatMessage:
				handleChatMessage(packet);
				break;
			case pp::world_realm::realm_packet::LeaveWorldInstance:
				handleLeaveWorldInstance(packet);
				break;
			case pp::world_realm::realm_packet::CharacterGroupChanged:
				handleCharacterGroupChanged(packet);
				break;
			case pp::world_realm::realm_packet::IgnoreList:
				handleIgnoreList(packet);
				break;
			case pp::world_realm::realm_packet::AddIgnore:
				handleAddIgnore(packet);
				break;
			case pp::world_realm::realm_packet::RemoveIgnore:
				handleRemoveIgnore(packet);
				break;
			case pp::world_realm::realm_packet::ItemData:
				handleItemData(packet);
				break;
			case pp::world_realm::realm_packet::SpellLearned:
				handleSpellLearned(packet);
				break;
			default:
				// Log about unknown or unhandled packet
				const auto &realm = m_config.realms[m_realmEntryIndex];
				WLOG("Received unknown packet " << static_cast<UInt32>(packetId)
					<< " from realm server at " << realm.realmAddress << ":" << realm.realmPort);
				break;
		}
	}

	bool RealmConnector::connectionEstablished(bool success)
	{
		const auto &realm = m_config.realms[m_realmEntryIndex];
		if (success)
		{
			ILOG("Connected to the realm server");

			io::StringSink sink(m_connection->getSendBuffer());
			pp::OutgoingPacket packet(sink);

			std::vector<UInt32> instanceIds;;

			// Write packet structure
			pp::world_realm::world_write::login(packet,
				realm.hostedMaps,
				instanceIds
				);

			m_connection->flush();
			scheduleKeepAlive();
		}
		else
		{
			WLOG("Could not connect to the realm server at " << realm.realmAddress << ":" << realm.realmPort);
			scheduleConnect();
		}

		return true;
	}

	void RealmConnector::scheduleConnect()
	{
		const GameTime now = getCurrentTime();
		m_timer.addEvent(
			std::bind(&RealmConnector::tryConnect, this), now + ReconnectDelay);
	}

	void RealmConnector::tryConnect()
	{
		const auto &realm = m_config.realms[m_realmEntryIndex];
		ILOG("Trying to connect to the realm server..");

		m_connection = pp::Connector::create(m_ioService);
		m_connection->connect(realm.realmAddress, realm.realmPort, *this, m_ioService);
	}

	void RealmConnector::scheduleKeepAlive()
	{
	}

	void RealmConnector::onScheduledKeepAlive()
	{
	}

	void RealmConnector::handleLoginAnswer(pp::Protocol::IncomingPacket &packet)
	{
		using namespace pp::world_realm;

		// Read packet
		UInt32 protocolVersion;
		LoginResult result;
		if (!(realm_read::loginAnswer(packet, protocolVersion, result, m_realmName)))
		{
			return;
		}

		// Detect version mismatch
		if (protocolVersion != pp::world_realm::ProtocolVersion)
		{
			WLOG("Realm protocol version mismatch detected. Please update...");
			return;
		}

		// Check result
		switch (result)
		{
			case login_result::Success:
			{
				ILOG("World node successfully registered at the realm server");
				break;
			}

			case login_result::UnknownMap:
			{
				ELOG("Realm does not know any of the maps we sent!");
				return;
			}

			case login_result::MapsAlreadyInUse:
			{
				ELOG("All maps are already handled by another world server");
				return;
			}

			case login_result::InternalError:
			{
				ELOG("Internal server error at the realm server");
				return;
			}

			default:
			{
				ELOG("Unknown login answer from realm server received: " << result);
				return;
			}
		}
	}

	void RealmConnector::handleCharacterLogin(pp::Protocol::IncomingPacket &packet)
	{
		// Read packet from the realm
		DatabaseId requesterDbId;
		UInt32 instanceId;
		std::vector<UInt32> spellIds;
		std::vector<ItemData> items;
		std::shared_ptr<GameCharacter> character(new GameCharacter(
			m_project,
			m_worldInstanceManager.getUniverse().getTimers()));
		if (!(pp::world_realm::realm_read::characterLogIn(packet, requesterDbId, instanceId, character.get(), spellIds, items)))
		{
			// Error: could not read packet
			return;
		}

		float o = character->getOrientation();
		math::Vector3 location(character->getLocation());

		// Let's lookup some informations about the requested map
		auto map = m_project.maps.getById(character->getMapId());
		if (!map)
		{
			ELOG("Unsupported map: " << character->getMapId());
			m_connection->sendSinglePacket(
				std::bind(pp::world_realm::world_write::worldInstanceError, std::placeholders::_1, requesterDbId, pp::world_realm::world_instance_error::UnsupportedMap));
			return;
		}

		// We know the map now - check if this is a global map
		WorldInstance *instance = nullptr;
		
		// Find instance
		if (instanceId != std::numeric_limits<UInt32>::max())
		{
			instance = m_worldInstanceManager.getInstanceById(instanceId);
			if (instance)
			{
				if (instance->getMapId() != map->id())
				{
					WLOG("Instance found but different map id - can't enter instance, creating new one.");
					instance = nullptr;
				}
			}
		}

		// Have we found an instance?
		if (instance == nullptr)
		{
			if (map->instancetype() == proto::MapEntry_MapInstanceType_GLOBAL)
			{
				// It is a global map instance... look for an instance of this map
				instance = m_worldInstanceManager.getInstanceByMapId(map->id());
				if (!instance)
				{
					// The global world instance for this map does not yet exist - we want to
					// create it
					instance = m_worldInstanceManager.createInstance(*map);
					if (!instance)
					{
						ELOG("Could not create world instance for map " << map->id());
						m_connection->sendSinglePacket(
							std::bind(pp::world_realm::world_write::worldInstanceError, std::placeholders::_1, requesterDbId, pp::world_realm::world_instance_error::InternalError));
						return;
					}
				}
			}
			else
			{
				// It is an instanced map - create a new instance (TODO: Group handling etc.)
				instance = m_worldInstanceManager.createInstance(*map);
				if (!instance)
				{
					ELOG("Could not create new world instance for map " << map->id());
					m_connection->sendSinglePacket(
						std::bind(pp::world_realm::world_write::worldInstanceError, std::placeholders::_1, requesterDbId, pp::world_realm::world_instance_error::InternalError));
					return;
				}
			}
		}

		// We really need an instance now
		assert(instance);
		
		// Fire signal which should create a player instance for us
		character->setWorldInstance(instance);	// This is required for spell auras
		worldInstanceEntered(*this, requesterDbId, character, *instance);

		// Resolve spells
		for (auto &spellId : spellIds)
		{
			const auto *spell = m_project.spells.getById(spellId);
			if (!spell)
			{
				WLOG("Received unknown character spell id " << spellId << ": Will be ignored");
				continue;
			}

			character->addSpell(*spell);
		}

		// Get character location
		UInt32 mapId = map->id();
		UInt32 zoneId = character->getZone();

		// Notify the realm that we successfully spawned in this world
		m_connection->sendSinglePacket(
			std::bind(
			pp::world_realm::world_write::worldInstanceEntered,
			std::placeholders::_1,
			requesterDbId,
			character->getGuid(),
			instance->getId(),
			mapId,
			zoneId,
			location,
			o
			));

		// Add character to the world
		assert(instance);
		instance->addGameObject(*character);

		// Spawn objects
		TileIndex2D tileIndex;
		instance->getGrid().getTilePosition(location, tileIndex[0], tileIndex[1]);
		forEachTileInSight(instance->getGrid(), tileIndex, [&character, this](VisibilityTile &tile)
		{
			for (auto it = tile.getGameObjects().begin(); it != tile.getGameObjects().end(); ++it)
			{
				auto &object = *it;
				if (object->getGuid() != character->getGuid() &&
					object->canSpawnForCharacter(*character))
				{
					// Create update block
					std::vector<std::vector<char>> blocks;
					createUpdateBlocks(*object, *character, blocks);

					std::vector<char> buffer;
					io::VectorSink sink(buffer);
					game::Protocol::OutgoingPacket packet(sink);
					game::server_write::compressedUpdateObject(packet, blocks);
					this->sendProxyPacket(character->getGuid(), packet.getOpCode(), packet.getSize(), buffer);
				}
			}
		});

		// Get that tile and make us a subscriber
		instance->getGrid().requireTile(tileIndex);
	}

	void RealmConnector::handleCharacterGroupChanged(pp::Protocol::IncomingPacket &packet)
	{
		UInt64 characterId, groupId;
		if (!(pp::world_realm::realm_read::characterGroupChanged(packet, characterId, groupId)))
		{
			return;
		}

		// Try to find character
		auto *player = m_playerManager.getPlayerByCharacterGuid(characterId);
		if (!player)
		{
			WLOG("Could not find character by guid 0x" << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << characterId);
			return;
		}

		player->updateCharacterGroup(groupId);
	}

	void RealmConnector::handleIgnoreList(pp::Protocol::IncomingPacket &packet)
	{
		UInt64 characterId;
		std::vector<UInt64> ignoreList;
		if (!(pp::world_realm::realm_read::ignoreList(packet, characterId, ignoreList)))
		{
			return;
		}

		// Try to find character
		auto *player = m_playerManager.getPlayerByCharacterGuid(characterId);
		if (!player)
		{
			WLOG("Could not find character by guid 0x" << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << characterId);
			return;
		}

		for (auto &guid : ignoreList)
		{
			player->addIgnore(guid);
		}
	}

	void RealmConnector::handleAddIgnore(pp::Protocol::IncomingPacket &packet)
	{
		UInt64 characterId, ignoreGUID;
		if (!(pp::world_realm::realm_read::addIgnore(packet, characterId, ignoreGUID)))
		{
			return;
		}

		// Try to find character
		auto *player = m_playerManager.getPlayerByCharacterGuid(characterId);
		if (!player)
		{
			WLOG("Could not find character by guid 0x" << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << characterId);
			return;
		}

		player->addIgnore(ignoreGUID);
	}

	void RealmConnector::handleRemoveIgnore(pp::Protocol::IncomingPacket &packet)
	{
		UInt64 characterId, ignoreGUID;
		if (!(pp::world_realm::realm_read::addIgnore(packet, characterId, ignoreGUID)))
		{
			return;
		}

		// Try to find character
		auto *player = m_playerManager.getPlayerByCharacterGuid(characterId);
		if (!player)
		{
			WLOG("Could not find character by guid 0x" << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << characterId);
			return;
		}

		player->removeIgnore(ignoreGUID);
	}

	void RealmConnector::handleItemData(pp::Protocol::IncomingPacket & packet)
	{
		UInt64 characterId;
		std::vector<ItemData> data;
		if (!(pp::world_realm::realm_read::itemData(packet, characterId, data)))
		{
			return;
		}

		// Check if data is empty
		if (data.empty())
			return;

		// Find requested character
		auto *player = m_playerManager.getPlayerByCharacterGuid(characterId);
		if (!player)
		{
			WLOG("Received item data from realm but couldn't find player connection!");
			return;
		}

		auto character = player->getCharacter();
		if (!character)
		{
			WLOG("Received item data, but could not find players game character!");
			return;
		}

		// Create items
		for (auto &it : data)
		{
			// Skip items that are not available
			const auto *entry = m_project.items.getById(it.entry);
			if (!entry)
			{
				WLOG("Unknown item entry: " << it.entry << " - item will be skipped!");
				continue;
			}

			std::map<UInt16, UInt16> addedBySlot;
			auto result = character->getInventory().createItems(*entry, it.stackCount, &addedBySlot);
			if (result != game::inventory_change_failure::Okay)
			{
				ELOG("Coult not create items: " << result);
				character->inventoryChangeFailure(result, nullptr, nullptr);
				continue;
			}

			for (auto &pair : addedBySlot)
			{
				auto itemInstance = character->getInventory().getItemAtSlot(pair.first);
				UInt8 bag = 0, subslot = 0;
				Inventory::getRelativeSlots(pair.first, bag, subslot);
				player->sendProxyPacket(
					std::bind(game::server_write::itemPushResult, std::placeholders::_1,
						character->getGuid(), std::cref(*itemInstance), false, false, bag, subslot, pair.second, character->getInventory().getItemCount(entry->id())));
			}
		}
	}

	void RealmConnector::handleSpellLearned(pp::Protocol::IncomingPacket & packet)
	{
		UInt64 characterId;
		UInt32 spellId;
		if (!(pp::world_realm::realm_read::spellLearned(packet, characterId, spellId)))
		{
			return;
		}

		const auto *spell = m_project.spells.getById(spellId);
		if (!spell)
		{
			return;
		}

		// Find requested character
		auto *player = m_playerManager.getPlayerByCharacterGuid(characterId);
		if (!player)
		{
			return;
		}
		auto character = player->getCharacter();
		if (!character)
		{
			return;
		}

		// Already learned?
		if (character->hasSpell(spellId))
		{
			return;
		}

		// World check (just in case)
		auto *world = character->getWorldInstance();
		if (!world)
		{
			return;
		}

		TileIndex2D tile;
		if (!character->getTileIndex(tile))
		{
			return;
		}

		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket outPacket(sink);
		game::server_write::playSpellImpact(outPacket, character->getGuid(), 0x016A);
		forEachSubscriberInSight(
			world->getGrid(),
			tile,
			[&outPacket, &buffer](ITileSubscriber &subscriber)
		{
			subscriber.sendPacket(outPacket, buffer);
		});

		// Learn the required spell
		character->addSpell(*spell);
		if ((spell->attributes(0) & game::spell_attributes::Passive) != 0)
		{
			SpellTargetMap targetMap;
			targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
			targetMap.m_unitTarget = character->getGuid();
			character->castSpell(std::move(targetMap), spellId);
		}

		player->sendProxyPacket(
			std::bind(game::server_write::learnedSpell, std::placeholders::_1, spellId));
	}

	void RealmConnector::handleProxyPacket(pp::Protocol::IncomingPacket &packet)
	{
		DatabaseId characterId;
		UInt16 opCode;
		UInt32 size;
		std::vector<char> buffer;
		if (!(pp::world_realm::realm_read::clientProxyPacket(packet, characterId, opCode, size, buffer)))
		{
			return;
		}

		if (size == 0) buffer.resize(1);
		
		// Setup packet
		io::MemorySource source(reinterpret_cast<const char*>(&buffer[0]), reinterpret_cast<const char*>(&buffer[0]) + size);

		// Create incoming packet structure
		game::IncomingPacket clientPacket;
		clientPacket.setSource(&source);

		// Find the sender
		Player *sender = m_playerManager.getPlayerByCharacterId(characterId);
		if (!sender)
		{
			WLOG("Invalid sender id " << characterId << " for proxy packet 0x" << std::hex << std::uppercase << opCode);
			return;
		}

		// Check op code
		switch (opCode)
		{
			// Client packets handled by connector
#define WOWPP_HANDLE_PACKET(name) \
			case wowpp::game::client_packet::name: \
			{ \
				/*ILOG("CLIENT PACKET " #name " (0x" << std::hex << wowpp::game::client_packet::name << ")");*/ \
				handle##name(*sender, clientPacket); \
				break; \
			}

			WOWPP_HANDLE_PACKET(NameQuery)
			WOWPP_HANDLE_PACKET(CreatureQuery)
			WOWPP_HANDLE_PACKET(LogoutRequest)
			WOWPP_HANDLE_PACKET(LogoutCancel)
			WOWPP_HANDLE_PACKET(SetSelection)
			WOWPP_HANDLE_PACKET(StandStateChange)
			WOWPP_HANDLE_PACKET(CastSpell)
			WOWPP_HANDLE_PACKET(CancelCast)
			WOWPP_HANDLE_PACKET(AttackSwing)
			WOWPP_HANDLE_PACKET(AttackStop)
			WOWPP_HANDLE_PACKET(SetSheathed)
			WOWPP_HANDLE_PACKET(AreaTrigger)
			WOWPP_HANDLE_PACKET(CancelAura)
			WOWPP_HANDLE_PACKET(Emote)
			WOWPP_HANDLE_PACKET(TextEmote)
#undef WOWPP_HANDLE_PACKET

			// Client packets handled by player
#define WOWPP_HANDLE_PLAYER_PACKET(name) \
			case wowpp::game::client_packet::name: \
			{ \
				/*ILOG("CLIENT PACKET " #name " (0x" << std::hex << wowpp::game::client_packet::name << ")");*/ \
				sender->handle##name(clientPacket); \
				break; \
			}

			WOWPP_HANDLE_PLAYER_PACKET(AutoStoreLootItem)
			WOWPP_HANDLE_PLAYER_PACKET(AutoEquipItem)
			WOWPP_HANDLE_PLAYER_PACKET(AutoStoreBagItem)
			WOWPP_HANDLE_PLAYER_PACKET(SwapItem)
			WOWPP_HANDLE_PLAYER_PACKET(SwapInvItem)
			WOWPP_HANDLE_PLAYER_PACKET(SplitItem)
			WOWPP_HANDLE_PLAYER_PACKET(AutoEquipItemSlot)
			WOWPP_HANDLE_PLAYER_PACKET(DestroyItem)
			WOWPP_HANDLE_PLAYER_PACKET(RepopRequest)
			WOWPP_HANDLE_PLAYER_PACKET(Loot)
			WOWPP_HANDLE_PLAYER_PACKET(LootMoney)
			WOWPP_HANDLE_PLAYER_PACKET(LootRelease)
			WOWPP_HANDLE_PLAYER_PACKET(TimeSyncResponse)
			WOWPP_HANDLE_PLAYER_PACKET(LearnTalent)
			WOWPP_HANDLE_PLAYER_PACKET(UseItem)
			WOWPP_HANDLE_PLAYER_PACKET(ListInventory)
			WOWPP_HANDLE_PLAYER_PACKET(SellItem)
			WOWPP_HANDLE_PLAYER_PACKET(BuyItem)
			WOWPP_HANDLE_PLAYER_PACKET(BuyItemInSlot)
			WOWPP_HANDLE_PLAYER_PACKET(GossipHello)
			WOWPP_HANDLE_PLAYER_PACKET(TrainerBuySpell)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverStatusQuery)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverHello)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverQueryQuest)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverQuestAutolaunch)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverAcceptQuest)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverCompleteQuest)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverRequestReward)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverChooseReward)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverCancel)
			WOWPP_HANDLE_PLAYER_PACKET(QuestgiverStatusMultipleQuery)
			WOWPP_HANDLE_PLAYER_PACKET(QuestlogRemoveQuest)
			WOWPP_HANDLE_PLAYER_PACKET(GameObjectUse)
			WOWPP_HANDLE_PLAYER_PACKET(OpenItem)
			WOWPP_HANDLE_PLAYER_PACKET(MoveTimeSkipped)

#undef WOWPP_HANDLE_PLAYER_PACKET

			// Movement packets get special treatment
			case game::client_packet::MoveStartForward:
			case game::client_packet::MoveStartBackward:
			case game::client_packet::MoveStop:
			case game::client_packet::StartStrafeLeft:
			case game::client_packet::StartStrafeRight:
			case game::client_packet::StopStrafe:
			case game::client_packet::Jump:
			case game::client_packet::StartTurnLeft:
			case game::client_packet::StartTurnRight:
			case game::client_packet::StopTurn:
			case game::client_packet::StartPitchUp:
			case game::client_packet::StartPitchDown:
			case game::client_packet::StopPitch:
			case game::client_packet::SetPitch:
			case game::client_packet::MoveSetRunMode:
			case game::client_packet::MoveSetWalkMode:
			case game::client_packet::MoveFallLand:
			case game::client_packet::MoveStartSwim:
			case game::client_packet::MoveStopSwim:
			case game::client_packet::SetFacing:
			case game::client_packet::MoveHeartBeat:
			case game::client_packet::MoveFallReset:
			case game::client_packet::MoveSetFly:
			case game::client_packet::MoveStartAscend:
			case game::client_packet::MoveStopAscend:
			case game::client_packet::MoveChangeTransport:
			case game::client_packet::MoveStartDescend:
			{
				sender->handleMovementCode(clientPacket, opCode);
				break;
			}
			default:
			{
				// Unhandled packet
				WLOG("Received client proxy packet: " << opCode << " (Size: " << size << " bytes)");
				break;
			}
		}
	}

	void RealmConnector::handleNameQuery(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Object guid
		UInt64 objectGuid;
		if (!game::client_read::nameQuery(packet, objectGuid))
		{
			// Could not read packet
			return;
		}

		// Find the player using the requested guid
		Player *player = m_playerManager.getPlayerByCharacterGuid(objectGuid);
		if (!player)
		{
			WLOG("Could not find requested player object with guid " << objectGuid);
			return;
		}

		// Get the character of this player
		auto character = player->getCharacter();

		// Get some values
		auto raceId = character->getRace();
		auto classId = character->getClass();
		auto genderId = character->getGender();
		const auto &name = character->getName();
		String realmName = player->getRealmName();

		// Send answer
		sender.sendProxyPacket(
			std::bind(game::server_write::nameQueryResponse, std::placeholders::_1, objectGuid, std::cref(name), std::cref(realmName), raceId, genderId, classId));
	}

	void RealmConnector::sendProxyPacket(DatabaseId senderId, UInt16 opCode, UInt32 size, const std::vector<char> &buffer)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::world_write::clientProxyPacket, std::placeholders::_1, senderId, opCode, size, std::cref(buffer)));
	}

	void RealmConnector::notifyWorldInstanceLeft(DatabaseId characterId, pp::world_realm::WorldLeftReason reason)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::world_write::worldInstanceLeft, std::placeholders::_1, characterId, reason));
	}

	void RealmConnector::sendTeleportRequest(DatabaseId characterId, UInt32 map, math::Vector3 location, float o)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::world_write::teleportRequest, std::placeholders::_1, characterId, map, location, o));
	}

	void RealmConnector::handleCreatureQuery(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Read the client packet
		UInt32 creatureEntry;
		UInt64 objectGuid;
		if (!game::client_read::creatureQuery(packet, creatureEntry, objectGuid))
		{
			// Could not read packet
			WLOG("Could not read packet data");
			return;
		}

		//TODO: Find creature object and check if it exists

		// Find creature info by entry
		const auto *unit = m_project.units.getById(creatureEntry);
		if (unit)
		{
			// Write answer packet
			sender.sendProxyPacket(
				std::bind(game::server_write::creatureQueryResponse, std::placeholders::_1, std::cref(*unit)));
		}
		else
		{
			//TODO: Send resulting packet SMSG_CREATURE_QUERY_RESPONSE with only one uin32 value
			//which is creatureEntry | 0x80000000
		}
	}

	void RealmConnector::handleLogoutRequest(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Read client packet
		if (!game::client_read::logoutRequest(packet))
		{
			WLOG("Could not read packet data");
			return;
		}

		//TODO check if the player is allowed to log out (is in combat? is moving? is frozen by gm? etc.)

		// Send answer and engage logout process
		sender.sendProxyPacket(
			std::bind(game::server_write::logoutResponse, std::placeholders::_1, true));

		// Start logout countdown or something? ...
		sender.logoutRequest();
	}

	void RealmConnector::handleLogoutCancel(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Read client packet
		if (!game::client_read::logoutCancel(packet))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Notify the client that we have received it's request
		sender.sendProxyPacket(
			std::bind(game::server_write::logoutCancelAck, std::placeholders::_1));

		// Cancel the logout process
		sender.cancelLogoutRequest();
	}

	void RealmConnector::handleSetSelection(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		UInt64 targetGUID;
		if (!game::client_read::setSelection(packet, targetGUID))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Try to find that target
		auto *world = sender.getCharacter()->getWorldInstance();
		if (!world)
			return;

		// Find object by GUID
		auto *targetObj =
			world->findObjectByGUID(targetGUID);
		if (targetObj)
		{
			if (targetObj->isCreature() ||
				targetObj->isGameCharacter())
			{
				// Unit is flagged as "NotSelectable"
				if ((targetObj->getUInt32Value(unit_fields::UnitFlags) & game::unit_flags::NotSelectable) != 0)
				{
					return;
				}
			}
			else
			{
				// Unselectable object type
				return;
			}
		}

		// Object found or no object provided
		if (targetObj || (!targetObj && targetGUID == 0))
		{
			GameUnit *unitTarget = nullptr;
			if (targetObj &&
				(targetObj->isGameCharacter() ||
				targetObj->getTypeId() == object_type::Unit))
			{
				unitTarget = reinterpret_cast<GameUnit*>(targetObj);
			}

			sender.getCharacter()->setVictim(unitTarget);
			return;
		}
		
		sender.getCharacter()->setUInt64Value(unit_fields::Target, targetGUID);
	}

	void RealmConnector::handleStandStateChange(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		UnitStandState standState;
		if (!game::client_read::standStateChange(packet, standState))
		{
			WLOG("Could not read packet data");
			return;
		}

		sender.getCharacter()->setStandState(standState);
	}

	void RealmConnector::handleChatMessage(pp::Protocol::IncomingPacket &packet)
	{
		UInt64 characterGuid;
		game::ChatMsg type;
		game::Language lang;
		String receiver, channel, message;
		if (!pp::world_realm::realm_read::chatMessage(packet, characterGuid, type, lang, receiver, channel, message))
		{
			WLOG("Could not read realm packet!");
			return;
		}

		// Find the player using this character guid
		Player *player = m_playerManager.getPlayerByCharacterGuid(characterGuid);
		if (!player)
		{
			WLOG("Could not find player for character GUID 0x" << std::hex << std::uppercase << characterGuid);
			return;
		}

		// Found the player, handle chat message
		player->chatMessage(type, lang, receiver, channel, message);
	}

	void RealmConnector::handleLeaveWorldInstance(pp::Protocol::IncomingPacket &packet)
	{
		UInt64 characterGuid;
		pp::world_realm::WorldLeftReason reason;
		if (!pp::world_realm::realm_read::leaveWorldInstance(packet, characterGuid, reason))
		{
			WLOG("Could not read realm packet!");
			return;
		}

		if (reason == pp::world_realm::world_left_reason::Disconnect ||
			reason == pp::world_realm::world_left_reason::Teleport)
		{
			// Find the character and remove it from the world instance
			auto *player = m_playerManager.getPlayerByCharacterGuid(characterGuid);
			if (!player)
			{
				WLOG("Could not find requested character.");
				return;
			}

			// Remove the character
			player->getWorldInstance().removeGameObject(*player->getCharacter());

			// Notify the realm
			notifyWorldInstanceLeft(characterGuid, reason);

			// Remove the player instance
			m_playerManager.playerDisconnected(*player);
		}
	}

	void RealmConnector::handleCastSpell(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Read spell cast packet
		UInt32 spellId;
		UInt8 castCount;
		SpellTargetMap targetMap;
		if (!game::client_read::castSpell(packet, spellId, castCount, targetMap))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Look for the spell
		const auto *spell = m_project.spells.getById(spellId);
		if (!spell)
		{
			return;
		}

		// Get the cast time of this spell
		Int64 castTime = spell->casttime();

		Int64 diff = sender.getCharacter()->applySpellMod(spell_mod_op::CastTime, spellId, castTime);
		if (castTime < 0) castTime = 0;

		// Spell cast logic
		sender.getCharacter()->castSpell(
			std::move(targetMap),
			spell->id(),
			-1,
			castTime,
			false,
			0,
			[&spell, castCount, &sender](game::SpellCastResult result)
			{
				if (result != game::spell_cast_result::CastOkay)
				{
					sender.sendProxyPacket(
						std::bind(game::server_write::castFailed, std::placeholders::_1, result, *spell, castCount));
				}
			});
	}

	void RealmConnector::handleCancelCast(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		UInt32 spellId;
		if (!game::client_read::cancelCast(packet, spellId))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Spell cast logic
		sender.getCharacter()->cancelCast(game::spell_interrupt_flags::None);
	}

	void RealmConnector::handleAttackSwing(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		UInt64 targetGUID;
		if (!game::client_read::attackSwing(packet, targetGUID))
		{
			return;
		}

		// We can't attack ourself
		if (targetGUID == sender.getCharacterGuid())
		{
			return;
		}

		// Check if target is a unit target
		if (!isUnitGUID(targetGUID))
		{
			return;
		}

		// Check if target does exist
		auto *target = dynamic_cast<GameUnit*>(sender.getWorldInstance().findObjectByGUID(targetGUID));
		if (!target)
		{
			return;
		}

		// Start attacking the given target
		sender.getCharacter()->setVictim(target);
		sender.getCharacter()->startAttack();
	}

	void RealmConnector::handleAttackStop(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		// Stop auto attack (if any)
		sender.getCharacter()->stopAttack();
	}

	void RealmConnector::handleSetSheathed(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		UInt32 sheathed;
		if (!game::client_read::setSheathed(packet, sheathed))
		{
			WLOG("Could not read packet data");
			return;
		}

		// TODO: Check sheath state

		// TODO: Update visual weapon ids if needed

		sender.getCharacter()->setByteValue(unit_fields::Bytes2, 0, static_cast<UInt8>(sheathed));
	}

	void RealmConnector::sendCharacterData(GameCharacter &character)
	{
		io::StringSink sink(m_connection->getSendBuffer());
		pp::OutgoingPacket packet(sink);
		pp::world_realm::world_write::characterData(packet, character.getGuid(), character);
		m_connection->flush();
	}

	void RealmConnector::handleAreaTrigger(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		UInt32 triggerId;
		if (!game::client_read::areaTrigger(packet, triggerId))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Check the trigger
		const auto *trigger = m_project.areaTriggers.getById(triggerId);
		if (!trigger)
		{
			return;
		}

		// Check if the players character could really have triggered that trigger
		auto character = sender.getCharacter();
		if (trigger->map() != character->getMapId())
		{
			return;
		}

		// Get player location for distance checks
		math::Vector3 location(character->getLocation());
		if (trigger->radius() > 0.0f)
		{
			const float dist = 
				::sqrtf(((location.x - trigger->x()) * (location.x - trigger->x())) + ((location.y - trigger->y()) * (location.y - trigger->y())) + ((location.z - trigger->z()) * (location.z - trigger->z())));
			if (dist > trigger->radius())
			{
				return;
			}
		}
		else
		{
			// TODO: Box check
		}

		// Check if this is a teleport trigger
		// TODO: Optimize this, create a trigger type
		if (trigger->targetmap() != 0 || trigger->target_x() != 0.0f || trigger->target_y() != 0.0f || trigger->target_z() != 0.0f || trigger->target_o() != 0.0f)
		{
			// Teleport
			character->teleport(trigger->targetmap(), math::Vector3(trigger->target_x(), trigger->target_y(), trigger->target_z()), trigger->target_o());
		}
		else
		{
			DLOG("TODO: Unknown trigger type '" << trigger->name() << "'...");
		}
	}

	void RealmConnector::handleCancelAura(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		if (!sender.getCharacter()->isAlive())
		{
			return;
		}

		UInt32 spellId;
		if (!game::client_read::cancelAura(packet, spellId))
		{
			return;
		}

		// Find that spell
		const auto *spell = m_project.spells.getById(spellId);
		if (!spell)
		{
			WLOG("Unknown spell - can't cancel aura");
			return;
		}

		// Check if that spell aura can be cancelled
		if ((spell->attributes(0) & game::spell_attributes::CantCancel) != 0)
		{
			WLOG("Aura of spell " << spell->id() << " - " << spell->name() << " can't be canceled!");
			return;
		}

		// Find all auras of that spell
		auto &auras = sender.getCharacter()->getAuras();
		auras.removeAllAurasDueToSpell(spellId);
	}

	void RealmConnector::handleEmote(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		if (!sender.getCharacter()->isAlive())
		{
			return;
		}

		UInt32 emoteId;
		if (!game::client_read::emote(packet, emoteId))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Create the chat packet
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket emotePacket(sink);
		game::server_write::emote(emotePacket, emoteId, sender.getCharacterGuid());

		TileIndex2D gridIndex;
		sender.getCharacter()->getTileIndex(gridIndex);

		// Notify all watchers about the new object
		forEachTileInSight(
			sender.getWorldInstance().getGrid(),
			gridIndex,
			[&emotePacket, &buffer](VisibilityTile &tile)
		{
			for (auto &watcher : tile.getWatchers())
			{
				watcher->sendPacket(emotePacket, buffer);
			}
		});
	}

	void RealmConnector::handleTextEmote(Player &sender, game::Protocol::IncomingPacket &packet)
	{
		if (!sender.getCharacter()->isAlive())
		{
			DLOG("Sender is dead...");
			return;
		}

		UInt32 textEmote, emoteNum;
		UInt64 guid;
		if (!game::client_read::textEmote(packet, textEmote, emoteNum, guid))
		{
			WLOG("Could not read packet data");
			return;
		}

		const auto *emote = m_project.emotes.getById(textEmote);
		if (!emote)
		{
			WLOG("Could not find emote " << textEmote);
			return;
		}

		TileIndex2D gridIndex;
		sender.getCharacter()->getTileIndex(gridIndex);

		UInt32 anim = emote->textid();
		switch (anim)
		{
			case 12:		// SLEEP
			case 13:		// SIT
			case 68:		// KNEEL
			case 0:			// ONESHOT_NONE
				break;

			default:
			{
				// Create the chat packet
				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				game::Protocol::OutgoingPacket emotePacket(sink);
				game::server_write::emote(emotePacket, anim, sender.getCharacterGuid());

				// Notify all watchers about the new object
				forEachTileInSight(
					sender.getWorldInstance().getGrid(),
					gridIndex,
					[&emotePacket, &buffer](VisibilityTile &tile)
				{
					for (auto &watcher : tile.getWatchers())
					{
						watcher->sendPacket(emotePacket, buffer);
					}
				});
			}
		}

		// Resolve unit name
		String name;
		if (guid != 0)
		{
			auto * unit = dynamic_cast<GameUnit*>(sender.getWorldInstance().findObjectByGUID(guid));
			if (unit)
			{
				if (isPlayerGUID(guid))
				{
					auto * gameChar = dynamic_cast<GameCharacter*>(unit);
					if (gameChar)
					{
						name = gameChar->getName();
					}
				}
				else
				{
					UInt32 entry = unit->getUInt32Value(object_fields::Entry);
					auto *unitEntry = m_project.units.getById(entry);
					if (unitEntry)
					{
						name = unitEntry->name();
					}
				}
			}
		}
		
		// Create the chat packet
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::Protocol::OutgoingPacket emotePacket(sink);
		game::server_write::textEmote(emotePacket, sender.getCharacterGuid(), textEmote, emoteNum, name);

		// Notify all watchers about the new object
		forEachTileInSight(
			sender.getWorldInstance().getGrid(),
			gridIndex,
			[&emotePacket, &buffer](VisibilityTile &tile)
		{
			for (auto &watcher : tile.getWatchers())
			{
				watcher->sendPacket(emotePacket, buffer);
			}
		});
	}

	void RealmConnector::sendCharacterGroupUpdate(GameCharacter &character, const std::vector<UInt64> &nearbyMembers)
	{
		math::Vector3 location(character.getLocation());

		auto powerType = character.getByteValue(unit_fields::Bytes0, 3);
		std::vector<UInt32> auras;
		// TODO: Auras

		io::StringSink sink(m_connection->getSendBuffer());
		pp::OutgoingPacket packet(sink);
		pp::world_realm::world_write::characterGroupUpdate(packet, character.getGuid(), nearbyMembers,
			character.getUInt32Value(unit_fields::Health), character.getUInt32Value(unit_fields::MaxHealth),
			character.getByteValue(unit_fields::Bytes0, 3), character.getUInt32Value(unit_fields::Power1 + powerType), character.getUInt32Value(unit_fields::MaxPower1 + powerType),
			character.getUInt32Value(unit_fields::Level), character.getMapId(), character.getZone(), location, auras);
		m_connection->flush();
	}

	void RealmConnector::sendQuestData(DatabaseId characterId, UInt32 quest, const QuestStatusData & data)
	{
		m_connection->sendSinglePacket(
			std::bind(pp::world_realm::world_write::questUpdate, std::placeholders::_1, characterId, quest, std::cref(data)));
	}

}
