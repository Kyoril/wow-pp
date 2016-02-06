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

#include "player.h"
#include "player_manager.h"
#include "log/default_log_levels.h"
#include "game/world_instance_manager.h"
#include "game/world_instance.h"
#include "game/each_tile_in_sight.h"
#include "game/universe.h"
#include "proto_data/project.h"
#include "game/game_creature.h"
#include "game/game_world_object.h"
#include <iomanip>
#include <cassert>
#include <limits>

using namespace std;

namespace wowpp
{
	Player::Player(PlayerManager &manager, RealmConnector &realmConnector, WorldInstanceManager &worldInstanceManager, DatabaseId characterId, std::shared_ptr<GameCharacter> character, WorldInstance &instance, proto::Project &project)
		: m_manager(manager)
		, m_realmConnector(realmConnector)
		, m_worldInstanceManager(worldInstanceManager)
		, m_characterId(characterId)
		, m_character(std::move(character))
		, m_logoutCountdown(instance.getUniverse().getTimers())
		, m_instance(instance)
		, m_lastError(attack_swing_error::Unknown)
		, m_lastFallTime(0)
		, m_lastFallZ(0.0f)
		, m_project(project)
		, m_loot(nullptr)
		, m_clientDelayMs(0)
		, m_nextDelayReset(0)
		, m_clientTicks(0)
		, m_groupUpdate(instance.getUniverse().getTimers())
	{
		m_logoutCountdown.ended.connect(
			std::bind(&Player::onLogout, this));
		m_onSpawn = m_character->spawned.connect(
			std::bind(&Player::onSpawn, this));
		m_onDespawn = m_character->despawned.connect(
			std::bind(&Player::onDespawn, this));
		m_onTileChange = m_character->tileChangePending.connect(
			std::bind(&Player::onTileChangePending, this, std::placeholders::_1, std::placeholders::_2));
		m_onProfChanged = m_character->proficiencyChanged.connect(
			std::bind(&Player::onProficiencyChanged, this, std::placeholders::_1, std::placeholders::_2));
		m_onAtkSwingErr = m_character->autoAttackError.connect(
			std::bind(&Player::onAttackSwingError, this, std::placeholders::_1));
		m_onInvFailure = m_character->inventoryChangeFailure.connect(
			std::bind(&Player::onInventoryChangeFailure, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		m_onComboPoints = m_character->comboPointsChanged.connect(
			std::bind(&Player::onComboPointsChanged, this));
		m_onXP = m_character->experienceGained.connect(
			std::bind(&Player::onExperienceGained, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		m_onCastError = m_character->spellCastError.connect(
			std::bind(&Player::onSpellCastError, this, std::placeholders::_1, std::placeholders::_2));
		m_onGainLevel = m_character->levelGained.connect(
			std::bind(&Player::onLevelGained, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, 
													std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8));
		m_onAuraUpdate = m_character->auraUpdated.connect(
			std::bind(&Player::onAuraUpdated, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		m_onTargetAuraUpdate = m_character->targetAuraUpdated.connect(
			std::bind(&Player::onTargetAuraUpdated, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
		m_onTeleport = m_character->teleport.connect(
			std::bind(&Player::onTeleport, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		m_onCooldownEvent = m_character->cooldownEvent.connect([this](UInt32 spellId) {
				sendProxyPacket(std::bind(game::server_write::cooldownEvent, std::placeholders::_1, spellId, m_character->getGuid()));
		});
		m_questChanged = m_character->questDataChanged.connect([this](UInt32 questId, const QuestStatusData &data) {
			m_realmConnector.sendQuestData(m_character->getGuid(), questId, data);
		});
		m_questKill = m_character->questKillCredit.connect([this](const proto::QuestEntry &quest, UInt64 guid, UInt32 entry, UInt32 count, UInt32 total) {
			sendProxyPacket(std::bind(game::server_write::questupdateAddKill, std::placeholders::_1, quest.id(), entry, count, total, guid));
		});

		// Inventory change signals
		auto &inventory = m_character->getInventory();
		m_itemCreated = inventory.itemInstanceCreated.connect(std::bind(&Player::onItemCreated, this, std::placeholders::_1, std::placeholders::_2));
		m_itemUpdated = inventory.itemInstanceUpdated.connect(std::bind(&Player::onItemUpdated, this, std::placeholders::_1, std::placeholders::_2));
		m_itemDestroyed = inventory.itemInstanceDestroyed.connect(std::bind(&Player::onItemDestroyed, this, std::placeholders::_1, std::placeholders::_2));

		// Root / stun change signals
		auto onRootOrStunUpdate = [this](bool flag) {
			if (flag || m_character->isRooted() || m_character->isStunned())
			{
				// Root the character
				m_character->addFlag(unit_fields::UnitFlags, 0x00040000);
				sendProxyPacket(
					std::bind(game::server_write::forceMoveRoot, std::placeholders::_1, m_character->getGuid(), 2));
			}
			else
			{
				m_character->removeFlag(unit_fields::UnitFlags, 0x00040000);
				sendProxyPacket(
					std::bind(game::server_write::forceMoveUnroot, std::placeholders::_1, m_character->getGuid(), 0));
			}
		};
		m_onRootUpdate = m_character->rootStateChanged.connect(onRootOrStunUpdate);
		m_onStunUpdate = m_character->stunStateChanged.connect(onRootOrStunUpdate);

		// Group update signal
		m_groupUpdate.ended.connect([this]()
		{
			math::Vector3 location(m_character->getLocation());

			// Get group id
			auto groupId = m_character->getGroupId();
			std::vector<UInt64> nearbyMembers;

			// Determine nearby party members
			m_instance.getUnitFinder().findUnits(Circle(location.x, location.y, 100.0f), [this, groupId, &nearbyMembers](GameUnit &unit) -> bool
			{
				// Only characters
				if (unit.getTypeId() != object_type::Character)
				{
					return true;
				}

				// Check characters group
				GameCharacter *character = static_cast<GameCharacter*>(&unit);
				if (character->getGroupId() == groupId &&
					character->getGuid() != getCharacterGuid())
				{
					nearbyMembers.push_back(character->getGuid());
				}

				return true;
			});

			// Send packet to world node
			m_realmConnector.sendCharacterGroupUpdate(*m_character, nearbyMembers);
			m_groupUpdate.setEnd(getCurrentTime() + constants::OneSecond * 3);
		});

		// Trigger regeneration for our character
		m_character->startRegeneration();
	}

	void Player::logoutRequest()
	{
		// Make our character sit down
		auto standState = unit_stand_state::Sit;
		m_character->setByteValue(unit_fields::Bytes1, 0, standState);
		sendProxyPacket(
			std::bind(game::server_write::standStateUpdate, std::placeholders::_1, standState));

		// Root our character
		m_character->addFlag(unit_fields::UnitFlags, 0x00040000);
		sendProxyPacket(
			std::bind(game::server_write::forceMoveRoot, std::placeholders::_1, m_character->getGuid(), 2));

		// Setup the logout countdown
		m_logoutCountdown.setEnd(
			getCurrentTime() + (20 * constants::OneSecond));
	}

	void Player::cancelLogoutRequest()
	{
		// Unroot
		m_character->removeFlag(unit_fields::UnitFlags, 0x00040000);
		sendProxyPacket(
			std::bind(game::server_write::forceMoveUnroot, std::placeholders::_1, m_character->getGuid(), 0));

		// Stand up again
		auto standState = unit_stand_state::Stand;
		m_character->setByteValue(unit_fields::Bytes1, 0, standState);
		sendProxyPacket(
			std::bind(game::server_write::standStateUpdate, std::placeholders::_1, standState));

		// Cancel the countdown
		m_logoutCountdown.cancel();
	}

	void Player::onLogout()
	{
		// Remove the character from the world
		m_instance.removeGameObject(*m_character);
		m_character.reset();

		// Notify the realm
		m_realmConnector.notifyWorldInstanceLeft(m_characterId, pp::world_realm::world_left_reason::Logout);
		
		// Remove player
		m_manager.playerDisconnected(*this);
	}
	
	TileIndex2D Player::getTileIndex() const
	{
		TileIndex2D tile;
		
		math::Vector3 location(m_character->getLocation());

		// Get a list of potential watchers
		auto &grid = m_instance.getGrid();
		grid.getTilePosition(location, tile[0], tile[1]);
		
		return tile;
	}

	void Player::chatMessage(game::ChatMsg type, game::Language lang, const String &receiver, const String &channel, const String &message)
	{
		if (!m_character)
		{
			WLOG("No character assigned!");
			return;
		}

		// Check if this is a whisper message
		if (type == game::chat_msg::Whisper)
		{
			// Get realm name and receiver name
			std::vector<String> names;
			split(receiver, '-', names);

			// There has to be a realm name since we are a world node and only receive whisper messages in case of cross
			// realm whispers
			if (names.size() != 2)
			{
				WLOG("Cross realm whisper: No realm name received!");
				return;
			}

			for (auto &str : names)
			{
				capitalize(str);
			}

			// Change language if needed so that whispers are always readable
			if (lang != game::language::Addon) lang = game::language::Universal;

			// Build full name
			std::ostringstream strm;
			strm << names[0] << "-" << names[1];
			String fullName = strm.str();

			// Iterate through the players (TODO: Make this more performant)
			for (const auto &player : m_manager.getPlayers())
			{
				if (player->getRealmName() == names[1] &&
					player->getCharacter()->getName() == names[0])
				{
					// Check faction
					const bool isAllianceA = ((game::race::Alliance & (1 << (player->getCharacter()->getRace() - 1))) == (1 << (player->getCharacter()->getRace() - 1)));
					const bool isAllianceB = ((game::race::Alliance & (1 << (m_character->getRace() - 1))) == (1 << (m_character->getRace() - 1)));
					if (isAllianceA != isAllianceB)
					{
						sendProxyPacket(
							std::bind(game::server_write::chatWrongFaction, std::placeholders::_1));
						return;
					}

					if (player->isIgnored(getCharacterGuid()))
					{
						WLOG("TODO: Target player ignores us.");
						return;
					}

					// Send whisper message
					player->sendProxyPacket(
						std::bind(game::server_write::messageChat, std::placeholders::_1, game::chat_msg::Whisper, lang, std::cref(channel), m_characterId, std::cref(message), m_character.get()));

					// If not an addon message, send reply message
					if (lang != game::language::Addon)
					{
						sendProxyPacket(
							std::bind(game::server_write::messageChat, std::placeholders::_1, game::chat_msg::Reply, lang, std::cref(channel), player->getCharacterGuid(), std::cref(message), m_character.get()));
					}
					return;
				}
			}

			// Could not find any player using that name
			sendProxyPacket(
				std::bind(game::server_write::chatPlayerNotFound, std::placeholders::_1, std::cref(fullName)));
		}
		else
		{
			math::Vector3 location(m_character->getLocation());

			// Get a list of potential watchers
			auto &grid = m_instance.getGrid();

			// Get tile index
			TileIndex2D tile;
			grid.getTilePosition(location, tile[0], tile[1]);

			// Get all potential characters
			std::vector<ITileSubscriber*> subscribers;
			forEachTileInSight(
				grid,
				tile,
				[this, &subscribers](VisibilityTile &tile)
			{
				for (auto * const subscriber : tile.getWatchers().getElements())
				{
					if(!isIgnored(subscriber->getControlledObject()->getGuid())) subscribers.push_back(subscriber);
				}
			});

			if (type != game::chat_msg::Say &&
				type != game::chat_msg::Yell &&
				type != game::chat_msg::Emote &&
				type != game::chat_msg::TextEmote)
			{
				WLOG("Unsupported chat mode");
				return;
			}

			if (!m_character->isAlive())
			{
				return;
			}

			// Create the chat packet
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::messageChat(packet, type, lang, channel, m_character->getGuid(), message, m_character.get());

			const float chatRange = (type == game::chat_msg::Yell || type == game::chat_msg::Emote || type == game::chat_msg::TextEmote) ? 300.0f : 25.0f;
			for (auto * const subscriber : subscribers)
			{
				const float distance = m_character->getDistanceTo(*subscriber->getControlledObject());
				if (distance <= chatRange)
				{
					subscriber->sendPacket(packet, buffer);
				}
			}
		}
	}

	void Player::sendPacket(game::Protocol::OutgoingPacket &packet, const std::vector<char> &buffer)
	{
		// Send the proxy packet to the realm server
		m_realmConnector.sendProxyPacket(m_characterId, packet.getOpCode(), packet.getSize(), buffer);
	}

	void Player::onSpawn()
	{
		// Send self spawn packet
		sendProxyPacket(
			std::bind(game::server_write::initWorldStates, std::placeholders::_1, m_character->getMapId(), m_character->getZone()));
		sendProxyPacket(
			std::bind(game::server_write::loginSetTimeSpeed, std::placeholders::_1, 0));

		// Create object blocks used in spawn packet
		std::vector<std::vector<char>> blocks;

		// Create item spawn packets - this actually creates the item instances
		// Note: We create these blocks first, so that the character item fields can be set properly
		// since we want the right values in the spawn packet and we don't want to send another update 
		// packet for this right away
		m_character->getInventory().addSpawnBlocks(blocks);

		// Write create object block (This block will be made the first block even though it is created
		// as the last block)
		std::vector<char> createBlock;
		io::VectorSink sink(createBlock);
		io::Writer writer(sink);
		{
			UInt8 updateType = 0x03;						// Player
			UInt8 updateFlags = 0x01 | 0x10 | 0x20 | 0x40;	// UPDATEFLAG_SELF | UPDATEFLAG_ALL | UPDATEFLAG_LIVING | UPDATEFLAG_HAS_POSITION
			UInt8 objectTypeId = 0x04;						// Player

			UInt64 guid = m_character->getGuid();

			// Header with object guid and type
			writer
				<< io::write<NetUInt8>(updateType);

			UInt64 guidCopy = guid;
			UInt8 packGUID[8 + 1];
			packGUID[0] = 0;
			size_t size = 1;
			for (UInt8 i = 0; guidCopy != 0; ++i)
			{
				if (guidCopy & 0xFF)
				{
					packGUID[0] |= UInt8(1 << i);
					packGUID[size] = UInt8(guidCopy & 0xFF);
					++size;
				}

				guidCopy >>= 8;
			}
			writer.sink().write((const char*)&packGUID[0], size);
			writer
				<< io::write<NetUInt8>(objectTypeId);

			writer
				<< io::write<NetUInt8>(updateFlags);

			// Write movement update
			{
				UInt32 moveFlags = 0x00;
				writer
					<< io::write<NetUInt32>(moveFlags)
					<< io::write<NetUInt8>(0x00)
					<< io::write<NetUInt32>(mTimeStamp());	//TODO: Time

															// Position & Rotation
				float o = m_character->getOrientation();
				math::Vector3 location(m_character->getLocation());


				writer
					<< io::write<float>(location.x)
					<< io::write<float>(location.y)
					<< io::write<float>(location.z)
					<< io::write<float>(o);

				// Fall time
				writer
					<< io::write<NetUInt32>(0);

				// Speeds
				writer
					<< io::write<float>(m_character->getSpeed(movement_type::Walk))					// Walk
					<< io::write<float>(m_character->getSpeed(movement_type::Run))					// Run
					<< io::write<float>(m_character->getSpeed(movement_type::Backwards))			// Backwards
					<< io::write<float>(m_character->getSpeed(movement_type::Swim))				// Swim
					<< io::write<float>(m_character->getSpeed(movement_type::SwimBackwards))	// Swim Backwards
					<< io::write<float>(m_character->getSpeed(movement_type::Flight))				// Fly
					<< io::write<float>(m_character->getSpeed(movement_type::FlightBackwards))		// Fly Backwards
					<< io::write<float>(m_character->getSpeed(movement_type::Turn));				// Turn (radians / sec: PI)
			}

			// Lower-GUID update?
			if (updateFlags & 0x08)
			{
				writer
					<< io::write<NetUInt32>(guidLowerPart(guid));
			}

			// High-GUID update?
			if (updateFlags & 0x10)
			{
				switch (objectTypeId)
				{
					case object_type::Object:
					case object_type::Item:
					case object_type::Container:
					case object_type::GameObject:
					case object_type::DynamicObject:
					case object_type::Corpse:
						writer
							<< io::write<NetUInt32>((guid << 48) & 0x0000FFFF);
						break;
					default:
						writer
							<< io::write<NetUInt32>(0);
				}
			}

			// Write values update
			m_character->writeValueUpdateBlock(writer, *m_character, true);

			// Add block
			blocks.emplace(blocks.begin(), std::move(createBlock));
		}

		// Send the actual spawn packet packet
		sendProxyPacket(
			std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(blocks)));

		// Send time sync request packet (this will also enable character movement at the client)
		sendProxyPacket(
			std::bind(game::server_write::timeSyncReq, std::placeholders::_1, 0));

		// Find our tile
		VisibilityTile &tile = m_instance.getGrid().requireTile(getTileIndex());
		tile.getWatchers().add(this);
	}

	void Player::onDespawn()
	{
		// Send character data to the realm
		m_realmConnector.sendCharacterData(*m_character);

		// Find our tile
		TileIndex2D tileIndex = getTileIndex();
		VisibilityTile &tile = m_instance.getGrid().requireTile(tileIndex);
		tile.getWatchers().remove(this);
	}

	void Player::onTileChangePending(VisibilityTile &oldTile, VisibilityTile &newTile)
	{
		// We no longer watch for changes on our old tile
		oldTile.getWatchers().remove(this);

		// Convert position to ADT position, and to ADT cell position
		auto newPos = newTile.getPosition();
		auto adtPos = makeVector<TileIndex>(newPos[0] / 16, newPos[1] / 16);
		auto localPos = makeVector<TileIndex>(newPos[0] - (adtPos[0] * 16), newPos[1] - (adtPos[1] * 16));

		// Check zone exploration
		auto *map = m_instance.getMapData();
		if (map)
		{
			// Get or load map tile
			auto *tile = map->getTile(adtPos);
			if (tile)
			{
				// Find local tile
				auto &area = tile->areas.cellAreas[localPos[1] + localPos[0] * 16];
				const auto *areaZone = m_project.zones.getById(area.areaId);
				if (areaZone)
				{
					// Update players zone field
					const auto *topLevelZone = areaZone;
					while (topLevelZone->parentzone())
					{
						topLevelZone = m_project.zones.getById(topLevelZone->parentzone());
					}

					m_character->setZone(topLevelZone->id());

					// Exploration
					UInt32 exploration = areaZone->explore();
					int offset = exploration / 32;

					UInt32 val = (UInt32)(1 << (exploration % 32));
					UInt32 currFields = m_character->getUInt32Value(character_fields::ExploredZones_1 + offset);

					if (!(currFields & val))
					{
						m_character->setUInt32Value(character_fields::ExploredZones_1 + offset, (UInt32)(currFields | val));

						if (m_character->getLevel() >= 70)
						{
							sendProxyPacket(
								std::bind(game::server_write::explorationExperience, std::placeholders::_1, areaZone->id(), 0));
						}
						else
						{
							const Int32 diff = static_cast<Int32>(m_character->getLevel()) - static_cast<Int32>(areaZone->level());
							UInt32 xp = 0;

							if (diff < -5)
							{
								const auto *levelEntry = m_project.levels.getById(m_character->getLevel() + 5);
								xp = (levelEntry ? levelEntry->explorationbasexp() : 0);
							}
							else if (diff > 5)
							{
								Int32 xpPct = (100 - ((diff - 5) * 5));
								if (xpPct > 100)
								{
									xpPct = 100;
								}
								else if (xpPct < 0)
								{
									xpPct = 0;
								}

								const auto *levelEntry = m_project.levels.getById(areaZone->level());
								xp = (levelEntry ? levelEntry->explorationbasexp() : 0) * xpPct / 100;
							}
							else
							{
								const auto *levelEntry = m_project.levels.getById(areaZone->level());
								xp = (levelEntry ? levelEntry->explorationbasexp() : 0);
							}

							// Calculate experience points
							if (xp > 0) m_character->rewardExperience(nullptr, xp);
							sendProxyPacket(
								std::bind(game::server_write::explorationExperience, std::placeholders::_1, areaZone->id(), xp));
						}
					}
				}
			}
		}

		auto &grid = m_instance.getGrid();

		// Spawn ourself for new watchers
		forEachTileInSightWithout(
			grid,
			newTile.getPosition(),
			oldTile.getPosition(),
			[this](VisibilityTile &tile)
		{
			for(auto * subscriber : tile.getWatchers().getElements())
			{
				auto *character = subscriber->getControlledObject();
				if (!character)
					continue;

				// Create spawn message blocks
				std::vector<std::vector<char>> spawnBlocks;
				createUpdateBlocks(*m_character, *character, spawnBlocks);

				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				game::Protocol::OutgoingPacket packet(sink);
				game::server_write::compressedUpdateObject(packet, spawnBlocks);

				assert(subscriber != this);
				subscriber->sendPacket(packet, buffer);
			}

			for (auto *object : tile.getGameObjects().getElements())
			{
				std::vector<std::vector<char>> createBlock;
				createUpdateBlocks(*object, *m_character, createBlock);

				this->sendProxyPacket(
					std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(createBlock)));
			}
		});

		// Despawn ourself for old watchers
		auto guid = m_character->getGuid();
		forEachTileInSightWithout(
			grid,
			oldTile.getPosition(),
			newTile.getPosition(),
			[guid, this](VisibilityTile &tile)
		{
			// Create the chat packet
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::destroyObject(packet, guid, false);

			for (auto * subscriber : tile.getWatchers().getElements())
			{
				assert(subscriber != this);
				subscriber->sendPacket(packet, buffer);
			}

			for (auto *object : tile.getGameObjects().getElements())
			{
				this->sendProxyPacket(
					std::bind(game::server_write::destroyObject, std::placeholders::_1, object->getGuid(), false));
			}
		});

		// Make us a watcher of the new tile
		newTile.getWatchers().add(this);
	}

	void Player::onProficiencyChanged(Int32 itemClass, UInt32 mask)
	{
		if (itemClass >= 0)
		{
			sendProxyPacket(
				std::bind(game::server_write::setProficiency, std::placeholders::_1, itemClass, mask));
		}
	}

	void Player::onAttackSwingError(AttackSwingError error)
	{
		// Nothing to do here, since the client will automatically repeat the last error message periodically
		if (error == m_lastError)
			return;

		// Save last error
		m_lastError = error;

		switch (error)
		{
			case attack_swing_error::CantAttack:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingCantAttack, std::placeholders::_1));
				break;
			}

			case attack_swing_error::NotStanding:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingNotStanding, std::placeholders::_1));
				break;
			}

			case attack_swing_error::OutOfRange:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingNotInRange, std::placeholders::_1));
				break;
			}

			case attack_swing_error::WrongFacing:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingBadFacing, std::placeholders::_1));
				break;
			}

			case attack_swing_error::TargetDead:
			{
				sendProxyPacket(
					std::bind(game::server_write::attackSwingDeadTarget, std::placeholders::_1));
				break;
			}

			case attack_swing_error::Success:
			{
				// Nothing to do here, since the auto attack was a success. This event is only fired to
				// reset the variable.
				break;
			}

			default:
			{
				WLOG("Unknown attack swing error code: " << error);
				break;
			}
		}
	}

	void Player::handleAutoStoreLootItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 lootSlot;
		if (!game::client_read::autoStoreLootItem(packet, lootSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Check if the player is looting
		if (!m_loot)
		{
			WLOG("Player isn't looting anything");
			return;
		}

		// Try to get loot at slot
		auto *lootItem = m_loot->getLootDefinition(lootSlot);
		if (!lootItem)
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::SlotIsEmpty, nullptr, nullptr));
			return;
		}

		// Already looted?
		if (lootItem->isLooted)
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::AlreadyLooted, nullptr, nullptr));
			return;
		}

		const auto *item = m_project.items.getById(lootItem->definition.item());
		if (!item)
		{
			WLOG("Can't find item!");
			return;
		}

		auto &inv = m_character->getInventory();

		std::map<UInt16, UInt16> addedBySlot;
		auto result = inv.createItems(*item, lootItem->count, &addedBySlot);
		if (result != game::inventory_change_failure::Okay)
		{
			// Error
			return;
		}

		for (auto &slot : addedBySlot)
		{
			auto inst = inv.getItemAtSlot(slot.first);
			if (inst)
			{
				UInt8 bag = 0, subslot = 0;
				Inventory::getRelativeSlots(slot.first, bag, subslot);
				const auto totalCount = inv.getItemCount(item->id());

				sendProxyPacket(
					std::bind(game::server_write::itemPushResult, std::placeholders::_1,
						m_character->getGuid(), std::cref(*inst), true, false, bag, subslot, slot.second, totalCount));

				// Group broadcasting
				if (m_character->getGroupId() != 0)
				{
					TileIndex2D tile;
					if (m_character->getTileIndex(tile))
					{
						std::vector<char> buffer;
						io::VectorSink sink(buffer);
						game::Protocol::OutgoingPacket itemPacket(sink);
						game::server_write::itemPushResult(itemPacket, m_character->getGuid(), std::cref(*inst), true, false, bag, subslot, slot.second, totalCount);
						forEachSubscriberInSight(
							m_character->getWorldInstance()->getGrid(),
							tile,
							[&](ITileSubscriber &subscriber)
						{
							if (subscriber.getControlledObject()->getGuid() != m_character->getGuid())
							{
								auto subscriberGroup = subscriber.getControlledObject()->getGroupId();
								if (subscriberGroup != 0 && subscriberGroup == m_character->getGroupId())
								{
									subscriber.sendPacket(itemPacket, buffer);
								}
							}
						});
					}
				}
			}
		}

		m_loot->takeItem(lootSlot);
	}

	void Player::handleAutoEquipItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcBag, srcSlot;
		if (!game::client_read::autoEquipItem(packet, srcBag, srcSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		auto &inv = m_character->getInventory();
		auto absSrcSlot = Inventory::getAbsoluteSlot(srcBag, srcSlot);
		auto item = inv.getItemAtSlot(absSrcSlot);
		if (!item)
		{
			ELOG("Item not found");
			return;
		}

		UInt8 targetSlot = 0xFF;

		// Check if item is equippable
		const auto &entry = item->getEntry();
		switch (entry.inventorytype())
		{
		case game::inventory_type::Ammo:
			// TODO
			break;
		case game::inventory_type::Head :
			targetSlot = player_equipment_slots::Head;
			break;
		case game::inventory_type::Cloak:
			targetSlot = player_equipment_slots::Back;
			break;
		case game::inventory_type::Neck:
			targetSlot = player_equipment_slots::Neck;
			break;
		case game::inventory_type::Feet:
			targetSlot = player_equipment_slots::Feet;
			break;
		case game::inventory_type::Body:
			targetSlot = player_equipment_slots::Body;
			break;
		case game::inventory_type::Chest:
		case game::inventory_type::Robe:
			targetSlot = player_equipment_slots::Chest;
			break;
		case game::inventory_type::Legs:
			targetSlot = player_equipment_slots::Legs;
			break;
		case game::inventory_type::Shoulders:
			targetSlot = player_equipment_slots::Shoulders;
			break;
		case game::inventory_type::TwoHandedWeapon:
		case game::inventory_type::MainHandWeapon:
			targetSlot = player_equipment_slots::Mainhand;
			break;
		case game::inventory_type::OffHandWeapon:
		case game::inventory_type::Shield:
			targetSlot = player_equipment_slots::Offhand;
			break;
		case game::inventory_type::Weapon:
			targetSlot = player_equipment_slots::Mainhand;
			break;
		case game::inventory_type::Finger:
			targetSlot = player_equipment_slots::Finger1;
			break;
		case game::inventory_type::Trinket:
			targetSlot = player_equipment_slots::Trinket1;
			break;
		case game::inventory_type::Wrists:
			targetSlot = player_equipment_slots::Wrists;
			break;
		case game::inventory_type::Tabard:
			targetSlot = player_equipment_slots::Tabard;
			break;
		case game::inventory_type::Hands:
			targetSlot = player_equipment_slots::Hands;
			break;
		case game::inventory_type::Waist:
			targetSlot = player_equipment_slots::Waist;
			break;
		default:
			break;
		}

		// Check if valid slot found
		auto absDstSlot = Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, targetSlot);
		if (!Inventory::isEquipmentSlot(absDstSlot))
		{
			ELOG("Invalid target slot")
			m_character->inventoryChangeFailure(game::inventory_change_failure::ItemCantBeEquipped, item.get(), nullptr);
			return;
		}

		// Get item at target slot
		auto result = inv.swapItems(absSrcSlot, absDstSlot);
		if (result != game::inventory_change_failure::Okay)
		{
			// Something went wrong
			ELOG("ERROR: " << result);
		}
	}

	void Player::handleAutoStoreBagItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcBag, srcSlot, dstBag;
		if (!game::client_read::autoStoreBagItem(packet, srcBag, srcSlot, dstBag))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_AUTO_STORE_BAG_ITEM(src bag: " << UInt32(srcBag) << ", src slot: " << UInt32(srcSlot) << ", dst bag: " << UInt32(dstBag) << ")");

		// TODO
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleSwapItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcBag, srcSlot, dstBag, dstSlot;
		if (!game::client_read::swapItem(packet, dstBag, dstSlot, srcBag, srcSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_SWAP_ITEM(src bag: " << UInt32(srcBag) << ", src slot: " << UInt32(srcSlot) << ", dst bag: " << UInt32(dstBag) << ", dst slot: " << UInt32(dstSlot) << ")");
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleSwapInvItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcSlot, dstSlot;
		if (!game::client_read::swapInvItem(packet, srcSlot, dstSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		auto &inv = m_character->getInventory();
		auto result = inv.swapItems(
			Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, srcSlot),
			Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, dstSlot));
		if (!result)
		{
			// An error happened
		}
	}

	void Player::handleSplitItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 srcBag, srcSlot, dstBag, dstSlot, count;
		if (!game::client_read::splitItem(packet, srcBag, srcSlot, dstBag, dstSlot, count))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_SPLIT_ITEM(src bag: " << UInt32(srcBag) << ", src slot: " << UInt32(srcSlot) << ", dst bag: " << UInt32(dstBag) << ", dst slot: " << UInt32(dstSlot) << ", count: " << UInt32(count) << ")");
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleAutoEquipItemSlot(game::Protocol::IncomingPacket &packet)
	{
		UInt64 itemGUID;
		UInt8 dstSlot;
		if (!game::client_read::autoEquipItemSlot(packet, itemGUID, dstSlot))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("CMSG_AUTO_EQUIP_ITEM_SLOT(item: 0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << itemGUID << std::dec << ", dst slot: " << UInt32(dstSlot) << ")");
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::InternalBagError, nullptr, nullptr));
	}

	void Player::handleDestroyItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 bag, slot, count, data1, data2, data3;
		if (!game::client_read::destroyItem(packet, bag, slot, count, data1, data2, data3))
		{
			return;
		}
		
		auto result = m_character->getInventory().removeItem(Inventory::getAbsoluteSlot(bag, slot), count);
		if (!result)
		{
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, result, nullptr, nullptr));
		}
	}

	void Player::handleLoot(game::Protocol::IncomingPacket &packet)
	{
		UInt64 objectGuid;
		if (!game::client_read::loot(packet, objectGuid))
		{
			return;
		}
		if (!m_character->isAlive())
		{
			return;
		}

		// Find game object
		GameObject *lootObject = m_character->getWorldInstance()->findObjectByGUID(objectGuid);
		if (!lootObject)
		{
			return;
		}

		// Is it a creature?
		if (lootObject->getTypeId() == object_type::Unit)
		{
			GameCreature *creature = reinterpret_cast<GameCreature*>(lootObject);
			if (creature->isAlive())
			{
				WLOG("Target creature is not dead and thus has no loot");
				return;
			}

			// Get loot from creature
			auto *loot = creature->getUnitLoot();
			if (loot && !loot->isEmpty())
			{
				m_loot = loot;
				m_onLootInvalidate = creature->despawned.connect([this](GameObject &despawned)
				{
					releaseLoot();
				});
				m_onLootCleared = m_loot->cleared.connect([this]()
				{
					releaseLoot();
				});

				sendProxyPacket(
					std::bind(game::server_write::lootResponse, std::placeholders::_1, objectGuid, game::loot_type::Corpse, std::cref(*loot)));

				m_character->addFlag(unit_fields::UnitFlags, game::unit_flags::Looting);
			}
			else
			{
				sendProxyPacket(
					std::bind(game::server_write::lootResponseError, std::placeholders::_1, objectGuid, game::loot_type::None, game::loot_error::Locked));
			}
		}
		else
		{
			// TODO
			WLOG("Only creatures are lootable at the moment.");
			sendProxyPacket(
				std::bind(game::server_write::lootResponseError, std::placeholders::_1, objectGuid, game::loot_type::None, game::loot_error::Locked));
		}
	}

	void Player::handleLootMoney(game::Protocol::IncomingPacket &packet)
	{
		if (!game::client_read::lootMoney(packet))
		{
			return;
		}
		if (!m_loot)
		{
			return;
		}

		// Find loot recipients
		auto lootGuid = m_loot->getLootGuid();
		auto *world = m_character->getWorldInstance();
		if (!world)
		{
			return;
		}

		// Get loot object
		GameObject *object = world->findObjectByGUID(lootGuid);
		if (!object)
		{
			return;
		}

		// Warning: takeGold may make m_loot invalid, because the loot will be reset if it is empty.
		UInt32 lootGold = m_loot->getGold();
		if (lootGold == 0)
		{
			return;
		}

		// Check if it's a creature
		std::vector<GameCharacter*> recipients;
		if (object->getTypeId() == object_type::Unit)
		{
			GameCreature *creature = reinterpret_cast<GameCreature*>(object);
			creature->forEachLootRecipient([&recipients](GameCharacter &recipient)
			{
				recipients.push_back(&recipient);
			});

			// Share gold
			lootGold /= recipients.size();
			if (lootGold == 0)
			{
				lootGold = 1;
			}
		}
		else
		{
			WLOG("Unsupported loot object");
			return;
		}

		// Reward with gold
		for (auto *recipient : recipients)
		{
			UInt32 coinage = recipient->getUInt32Value(character_fields::Coinage);
			if (coinage >= std::numeric_limits<UInt32>::max() - lootGold)
			{
				coinage = std::numeric_limits<UInt32>::max();
			}
			else
			{
				coinage += lootGold;
			}
			recipient->setUInt32Value(character_fields::Coinage, coinage);

			// Notify players
			auto *player = m_manager.getPlayerByCharacterGuid(recipient->getGuid());
			if (player)
			{
				if (recipients.size() > 1)
				{
					player->sendProxyPacket(
						std::bind(game::server_write::lootMoneyNotify, std::placeholders::_1, lootGold));
				}

				// TODO: Put this packet into the LootInstance class or in an event callback maybe
				if (player->isLooting(lootGuid))
				{
					player->sendProxyPacket(
						std::bind(game::server_write::lootClearMoney, std::placeholders::_1));
				}
			}
		}

		// Take gold (WARNING: May reset m_loot as loot may become empty after this)
		m_loot->takeGold();
	}

	void Player::handleLootRelease(game::Protocol::IncomingPacket &packet)
	{
		UInt64 creatureId;
		if (!game::client_read::lootRelease(packet, creatureId))
		{
			WLOG("Could not read packet data");
			return;
		}

		releaseLoot();
	}

	void Player::onInventoryChangeFailure(game::InventoryChangeFailure failure, GameItem *itemA, GameItem *itemB)
	{
		// Send network packet about inventory change failure
		sendProxyPacket(
			std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, failure, itemA, itemB));
	}

	void Player::onComboPointsChanged()
	{
		sendProxyPacket(
			std::bind(game::server_write::updateComboPoints, std::placeholders::_1, m_character->getComboTarget(), m_character->getComboPoints()));
	}

	void Player::onExperienceGained(UInt64 victimGUID, UInt32 baseXP, UInt32 restXP)
	{
		sendProxyPacket(
			std::bind(game::server_write::logXPGain, std::placeholders::_1, victimGUID, baseXP, restXP, false));	// TODO: Refer a friend support
	}

	void Player::onSpellCastError(const proto::SpellEntry &spell, game::SpellCastResult result)
	{
		sendProxyPacket(
			std::bind(game::server_write::castFailed, std::placeholders::_1, result, std::cref(spell), 0));
	}

	void Player::onLevelGained(UInt32 previousLevel, Int32 healthDiff, Int32 manaDiff, Int32 statDiff0, Int32 statDiff1, Int32 statDiff2, Int32 statDiff3, Int32 statDiff4)
	{
		UInt32 level = getCharacter()->getLevel();
		sendProxyPacket(
			std::bind(game::server_write::levelUpInfo, 
				std::placeholders::_1, 
				level,
				healthDiff,
				manaDiff,
				statDiff0,
				statDiff1,
				statDiff2,
				statDiff3,
				statDiff4)
			);
	}

	void Player::onAuraUpdated(UInt8 slot, UInt32 spellId, Int32 duration, Int32 maxDuration)
	{
		sendProxyPacket(
			std::bind(game::server_write::updateAuraDuration, std::placeholders::_1, slot, duration));

		sendProxyPacket(
			std::bind(game::server_write::setExtraAuraInfo, std::placeholders::_1, getCharacterGuid(), slot, spellId, maxDuration, duration));
	}

	void Player::onTargetAuraUpdated(UInt64 target, UInt8 slot, UInt32 spellId, Int32 duration, Int32 maxDuration)
	{
		sendProxyPacket(
			std::bind(game::server_write::setExtraAuraInfoNeedUpdate, std::placeholders::_1, target, slot, spellId, maxDuration, duration));
	}

	void Player::onTeleport(UInt16 map, math::Vector3 location, float o)
	{
		// If it's the same map, just send success notification
		if (m_character->getMapId() == map)
		{
			// Apply character position
			m_character->relocate(location, o);

			// Reset movement info
			MovementInfo info = m_character->getMovementInfo();
			info.moveFlags = game::movement_flags::None;
			info.x = location.x;
			info.y = location.y;
			info.z = location.z;
			info.o = o;
			sendProxyPacket(
				std::bind(game::server_write::moveTeleportAck, std::placeholders::_1, m_character->getGuid(), std::cref(info)));

			// Update movement info
			m_character->setMovementInfo(std::move(info));
		}
		else
		{
			// Send transfer pending state. This will show up the loading screen at the client side and will
			// tell the realm where our character should be sent to
			sendProxyPacket(
				std::bind(game::server_write::transferPending, std::placeholders::_1, map, 0, 0));
			m_realmConnector.sendTeleportRequest(m_characterId, map, location, o);

			// Remove the character from the world
			m_instance.removeGameObject(*m_character);
			m_character.reset();

			// Notify the realm
			m_realmConnector.notifyWorldInstanceLeft(m_characterId, pp::world_realm::world_left_reason::Teleport);

			// Destroy player instance
			m_manager.playerDisconnected(*this);
		}
	}

	void Player::onItemCreated(std::shared_ptr<GameItem> item, UInt16 slot)
	{
		// Now we can send the actual packet
		std::vector<std::vector<char>> blocks;
		std::vector<char> createItemBlock;
		io::VectorSink createItemSink(createItemBlock);
		io::Writer createItemWriter(createItemSink);
		{
			UInt8 updateType = 0x02;						// Item
			UInt8 updateFlags = 0x08 | 0x10;				// 
			UInt8 objectTypeId = 0x01;						// Item

			UInt64 guid = item->getGuid();

			// Header with object guid and type
			createItemWriter
				<< io::write<NetUInt8>(updateType);
			UInt64 guidCopy = guid;
			UInt8 packGUID[8 + 1];
			packGUID[0] = 0;
			size_t size = 1;
			for (UInt8 i = 0; guidCopy != 0; ++i)
			{
				if (guidCopy & 0xFF)
				{
					packGUID[0] |= UInt8(1 << i);
					packGUID[size] = UInt8(guidCopy & 0xFF);
					++size;
				}
				guidCopy >>= 8;
			}
			createItemWriter.sink().write((const char*)&packGUID[0], size);
			createItemWriter
				<< io::write<NetUInt8>(objectTypeId)
				<< io::write<NetUInt8>(updateFlags);
			if (updateFlags & 0x08)
			{
				createItemWriter
					<< io::write<NetUInt32>(guidLowerPart(guid));
			}
			if (updateFlags & 0x10)
			{
				createItemWriter
					<< io::write<NetUInt32>((guid << 48) & 0x0000FFFF);
			}
			item->writeValueUpdateBlock(createItemWriter, *m_character, true);
		}

		// Send packet
		blocks.push_back(std::move(createItemBlock));
		sendProxyPacket(
			std::bind(game::server_write::updateObject, std::placeholders::_1, std::cref(blocks)));
	}

	void Player::onItemUpdated(std::shared_ptr<GameItem> item, UInt16 slot)
	{
		std::vector<std::vector<char>> blocks;
		std::vector<char> createItemBlock;
		io::VectorSink createItemSink(createItemBlock);
		io::Writer createItemWriter(createItemSink);
		io::VectorSink sink(createItemBlock);
		io::Writer writer(sink);
		{
			UInt8 updateType = 0x00;						// Update type (0x00 = UPDATE_VALUES)
			UInt64 guid = item->getGuid();
			writer
				<< io::write<NetUInt8>(updateType);

			UInt64 guidCopy = guid;
			UInt8 packGUID[8 + 1];
			packGUID[0] = 0;
			size_t size = 1;
			for (UInt8 i = 0; guidCopy != 0; ++i)
			{
				if (guidCopy & 0xFF)
				{
					packGUID[0] |= UInt8(1 << i);
					packGUID[size] = UInt8(guidCopy & 0xFF);
					++size;
				}

				guidCopy >>= 8;
			}
			writer.sink().write((const char*)&packGUID[0], size);
			item->writeValueUpdateBlock(writer, *m_character, false);
		}

		item->clearUpdateMask();

		// Send packet
		blocks.emplace_back(std::move(createItemBlock));
		sendProxyPacket(
			std::bind(game::server_write::updateObject, std::placeholders::_1, std::cref(blocks)));
	}

	void Player::onItemDestroyed(std::shared_ptr<GameItem> item, UInt16 slot)
	{
		sendProxyPacket(
			std::bind(game::server_write::destroyObject, std::placeholders::_1, item->getGuid(), false));
	}

	void Player::handleRepopRequest(game::Protocol::IncomingPacket &packet)
	{
		if (!m_character)
		{
			return;
		}

		if (m_character->isAlive())
		{
			return;
		}

		WLOG("TODO: repop at nearest graveyard!");
		m_character->revive(m_character->getUInt32Value(unit_fields::MaxHealth), 0);
	}

	void Player::setFallInfo(UInt32 time, float z)
	{
		m_lastFallTime = time;
		m_lastFallZ = z;
	}

	void Player::releaseLoot()
	{
		if (m_loot)
		{
			m_onLootCleared.disconnect();
			m_onLootInvalidate.disconnect();

			sendProxyPacket(
				std::bind(game::server_write::lootReleaseResponse, std::placeholders::_1, m_loot->getLootGuid()));
			m_loot = nullptr;

			m_character->removeFlag(unit_fields::UnitFlags, game::unit_flags::Looting);
		}
	}

	bool Player::isIgnored(UInt64 guid) const
	{
		return guid == 504403158265495562;
	}

	void Player::handleMovementCode(game::Protocol::IncomingPacket &packet, UInt16 opCode)
	{
		MovementInfo info;
		if (!game::client_read::moveHeartBeat(packet, info))
		{
			WLOG("Could not read packet data");
			return;
		}

		UInt64 currentTime = getCurrentTime();
		if (m_nextDelayReset <= currentTime)
		{
			m_clientDelayMs = 0;
			m_nextDelayReset = currentTime + constants::OneSecond * 30;
		}

		if (opCode != game::client_packet::MoveStop)
		{
			auto flags = m_character->getUInt32Value(unit_fields::UnitFlags);
			if ((flags & 0x00040000) != 0 ||
				(flags & 0x00000004) != 0)
			{
				WLOG("Character is unable to move, ignoring move packet");
				return;
			}
		}

		// Sender guid
		auto guid = m_character->getGuid();

		// Get object location
		math::Vector3 location(m_character->getLocation());

		// Store movement information
		m_character->setMovementInfo(info);

		// Transform into grid location
		TileIndex2D gridIndex;
		auto &grid = getWorldInstance().getGrid();
		if (!grid.getTilePosition(location, gridIndex[0], gridIndex[1]))
		{
			// TODO: Error?
			ELOG("Could not resolve grid location!");
			return;
		}

		UInt32 msTime = mTimeStamp();
		if (m_clientDelayMs == 0)
		{
			m_clientDelayMs = msTime - info.time;
		}
		UInt32 move_time = (info.time - (msTime - m_clientDelayMs)) + 500 + msTime;

		// Get grid tile
		auto &tile = grid.requireTile(gridIndex);
		info.time = move_time;

		// Notify all watchers about the new object
		forEachTileInSight(
			getWorldInstance().getGrid(),
			gridIndex,
			[this, &info, opCode, guid, move_time](VisibilityTile &tile)
		{
			for (auto &watcher : tile.getWatchers())
			{
				if (watcher != this)
				{
					// Convert timestamps
					//info.time = watcher->convertTimestamp(move_time, m_clientTicks) + 500;
					//info.fallTime = watcher->convertTimestamp(info.fallTime, m_clientTicks) + 500;

					// Create the chat packet
					std::vector<char> buffer;
					io::VectorSink sink(buffer);
					game::Protocol::OutgoingPacket movePacket(sink);
					game::server_write::movePacket(movePacket, opCode, guid, info);

					watcher->sendPacket(movePacket, buffer);
				}
			}
		});

		//TODO: Verify new location

		UInt32 lastFallTime = 0;
		float lastFallZ = 0.0f;
		getFallInfo(lastFallTime, lastFallZ);

		// Fall damage
		if (opCode == game::client_packet::MoveFallLand)
		{
			if (info.fallTime >= 1100)
			{
				float deltaZ = lastFallZ - info.z;
				if (m_character->isAlive())
				{
					float damageperc = 0.018f * deltaZ - 0.2426f;
					if (damageperc > 0)
					{
						const UInt32 maxHealth = m_character->getUInt32Value(unit_fields::MaxHealth);
						UInt32 damage = (UInt32)(damageperc * maxHealth);
						float height = info.z;

						if (damage > 0)
						{
							//Prevent fall damage from being more than the player maximum health
							if (damage > maxHealth) damage = maxHealth;

							std::vector<char> dmgBuffer;
							io::VectorSink dmgSink(dmgBuffer);
							game::Protocol::OutgoingPacket dmgPacket(dmgSink);
							game::server_write::environmentalDamageLog(dmgPacket, getCharacterGuid(), 2, damage, 0, 0);

							// Deal damage
							forEachTileInSight(
								getWorldInstance().getGrid(),
								gridIndex,
								[&dmgPacket, &dmgBuffer](VisibilityTile &tile)
							{
								for (auto &watcher : tile.getWatchers())
								{
									watcher->sendPacket(dmgPacket, dmgBuffer);
								}
							});

							m_character->dealDamage(damage, 0, nullptr, true);
							if (!m_character->isAlive())
							{
								WLOG("TODO: Durability damage!");
								sendProxyPacket(
									std::bind(game::server_write::durabilityDamageDeath, std::placeholders::_1));
							}
						}
					}
				}
			}
		}

		if (opCode == game::client_packet::MoveFallLand ||
			lastFallTime > info.fallTime/* ||
										info.z < lastFallZ*/)
		{
			setFallInfo(info.fallTime, info.z);
		}

		// Update position
		m_character->relocate(math::Vector3(info.x, info.y, info.z), info.o);
	}

	void Player::handleTimeSyncResponse(game::Protocol::IncomingPacket &packet)
	{
		UInt32 counter, ticks;
		if (!game::client_read::timeSyncResponse(packet, counter, ticks))
		{
			WLOG("Could not read packet data");
			return;
		}

		DLOG("TIME SYNC RESPONSE " << m_character->getName() << ": Counter " << counter << "; Ticks: " << ticks);
		m_clientTicks = ticks;
	}

	UInt32 Player::convertTimestamp(UInt32 otherTimestamp, UInt32 otherTick) const
	{
		UInt32 otherDiff = otherTimestamp - otherTick;
		return m_clientTicks + otherDiff;
	}

	void Player::addIgnore(UInt64 guid)
	{
		if (!m_ignoredGUIDs.contains(guid)) m_ignoredGUIDs.add(guid);
	}

	void Player::removeIgnore(UInt64 guid)
	{
		if (m_ignoredGUIDs.contains(guid)) m_ignoredGUIDs.remove(guid);
	}

	void Player::updateCharacterGroup(UInt64 group)
	{
		if (m_character)
		{
			UInt64 oldGroup = m_character->getGroupId();
			m_character->setGroupId(group);

			m_character->forceFieldUpdate(unit_fields::Health);
			m_character->forceFieldUpdate(unit_fields::MaxHealth);

			// For every group member in range, also update health fields
			TileIndex2D tileIndex;
			m_character->getTileIndex(tileIndex);
			forEachSubscriberInSight(
				m_character->getWorldInstance()->getGrid(),
				tileIndex,
				[oldGroup, group](ITileSubscriber &subscriber)
			{
				auto *character = subscriber.getControlledObject();
				if (character)
				{
					if ((oldGroup != 0 && character->getGroupId() == oldGroup) ||
						(group != 0 && character->getGroupId() == group))
					{
						character->forceFieldUpdate(unit_fields::Health);
						character->forceFieldUpdate(unit_fields::MaxHealth);
					}
				}
			});
		}

		if (group != 0)
		{
			// Trigger next group member update
			m_groupUpdate.setEnd(getCurrentTime() + (constants::OneSecond * 3));
		}
		else
		{
			// Stop group member update
			m_groupUpdate.cancel();
		}
	}

	void Player::handleLearnTalent(game::Protocol::IncomingPacket &packet)
	{
		UInt32 talentId = 0, rank = 0;
		if (!game::client_read::learnTalent(packet, talentId, rank))
		{
			WLOG("Could not read packet data");
			return;
		}

		// Try to find talent
		auto *talent = m_project.talents.getById(talentId);
		if (!talent)
		{
			WLOG("Could not find requested talent id " << talentId);
			return;
		}

		// Check rank
		if (talent->ranks_size() < static_cast<int>(rank))
		{
			WLOG("Talent " << talentId << " does offer " << talent->ranks_size() << " ranks, but rank " << rank << " is requested!");
			return;
		}

		// TODO: Check whether the player is allowed to learn that talent, based on his class

		// Check if another talent is required
		if (talent->dependson())
		{
			const auto *dependson = m_project.talents.getById(talentId);
			assert(dependson);

			// Check if we have learned the requested talent rank
			auto dependantRank = dependson->ranks(talent->dependsonrank());
			if (!m_character->hasSpell(dependantRank))
			{
				WLOG("Dependent talent not learned!");
				return;
			}
		}

		// Check if another spell is required
		if (talent->dependsonspell())
		{
			if (!m_character->hasSpell(talent->dependsonspell()))
			{
				WLOG("Dependent spell not learned!");
				return;
			}
		}

		// Check if we have enough talent points
		UInt32 freeTalentPoints = m_character->getUInt32Value(character_fields::CharacterPoints_1);
		if (freeTalentPoints == 0)
		{
			WLOG("Not enough talent points available");
			return;
		}

		// Remove all previous learnt ranks, learn the highest one
		for (UInt32 i = 0; i < rank; ++i)
		{
			const auto *spell = m_project.spells.getById(talent->ranks(i));
			if (m_character->removeSpell(*spell))
			{
				// TODO: Send packet maybe?
			}
		}

		const auto *spell = m_project.spells.getById(talent->ranks(rank));

		// Add new spell, and maybe remove old spells
		m_character->addSpell(*spell);
		if ((spell->attributes(0) & game::spell_attributes::Passive) != 0)
		{
			SpellTargetMap targets;
			targets.m_targetMap = game::spell_cast_target_flags::Unit;
			targets.m_unitTarget = m_character->getGuid();
			m_character->castSpell(std::move(targets), spell->id());
		}

		sendProxyPacket(
			std::bind(game::server_write::learnedSpell, std::placeholders::_1, spell->id()));
	}

	void Player::handleUseItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 bagId = 0, slotId = 0, spellCount = 0, castCount = 0;
		UInt64 itemGuid = 0;
		SpellTargetMap targetMap;
		if (!game::client_read::useItem(packet, bagId, slotId, spellCount, castCount, itemGuid, targetMap))
		{
			return;
		}

		// Get item
		auto item = m_character->getInventory().getItemAtSlot(Inventory::getAbsoluteSlot(bagId, slotId));
		if (!item)
		{
			WLOG("Item not found! Bag: " << UInt16(bagId) << "; Slot: " << UInt16(slotId));
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::ItemNotFound, nullptr, nullptr));
			return;
		}

		if (item->getGuid() != itemGuid)
		{
			WLOG("Item GUID does not match. We look for 0x" << std::hex << itemGuid << " but found 0x" << std::hex << item->getGuid());
			sendProxyPacket(
				std::bind(game::server_write::inventoryChangeFailure, std::placeholders::_1, game::inventory_change_failure::ItemNotFound, nullptr, nullptr));
			return;
		}

		auto &entry = item->getEntry();
		for (int i = 0; i < entry.spells_size(); ++i)
		{
			const auto &spell = entry.spells(i);
			if (!spell.spell())
			{
				continue;
			}

			// Spell effect has to be triggered "on use", not "on equip" etc.
			if (spell.trigger() != 0 && spell.trigger() != 5)
			{
				continue;
			}

			const auto *spellEntry = m_project.spells.getById(spell.spell());
			if (!spellEntry)
			{
				continue;
			}

			UInt64 time = spellEntry->casttime();
			m_character->castSpell(std::move(targetMap), spell.spell(), -1, time);
		}
	}

	void Player::handleListInventory(game::Protocol::IncomingPacket &packet)
	{
		UInt64 vendorGuid = 0;
		if (!game::client_read::listInventory(packet, vendorGuid))
		{
			WLOG("Could not read packet data");
			return;
		}

		if (!m_character->isAlive())
			return;

		auto *world = m_character->getWorldInstance();
		if (!world)
			return;

		// Try to find that vendor
		GameCreature *vendor = dynamic_cast<GameCreature*>(m_character->getWorldInstance()->findObjectByGUID(vendorGuid));
		if (!vendor)
		{
			// Could not find vendor by GUID or vendor is not a creature
			return;
		}

		// Check if vendor is alive and not in fight
		if (!vendor->isAlive() || vendor->isInCombat())
			return;

		// Check if vendor is not hostile against players
		if (m_character->isHostileToPlayers())
			return;

		// Check if that vendor has the vendor flag
		if ((vendor->getUInt32Value(unit_fields::NpcFlags) & game::unit_npc_flags::Vendor) == 0)
			return;

		// Check if the vendor DO sell items
		const auto *vendorEntry = m_project.vendors.getById(vendor->getEntry().vendorentry());
		if (!vendorEntry)
		{
			std::vector<proto::VendorItemEntry> emptyList;
			sendProxyPacket(
				std::bind(game::server_write::listInventory, std::placeholders::_1, vendor->getGuid(), std::cref(m_project.items), std::cref(emptyList)));
			return;
		}

		// TODO
		std::vector<proto::VendorItemEntry> list;
		for (const auto &entry : vendorEntry->items())
		{
			list.push_back(entry);
		}

		sendProxyPacket(
			std::bind(game::server_write::listInventory, std::placeholders::_1, vendor->getGuid(), std::cref(m_project.items), std::cref(list)));
	}

	void Player::handleBuyItem(game::Protocol::IncomingPacket &packet)
	{
		UInt64 vendorGuid = 0;
		UInt32 item = 0;
		UInt8 count = 0;
		if (!(game::client_read::buyItem(packet, vendorGuid, item, count)))
		{
			WLOG("Coult not read CMSG_BUY_ITEM packet");
			return;
		}

		buyItemFromVendor(vendorGuid, item, 0, 0xFF, count);
	}

	void Player::handleBuyItemInSlot(game::Protocol::IncomingPacket &packet)
	{
		UInt64 vendorGuid = 0, bagGuid = 0;
		UInt32 item = 0;
		UInt8 slot = 0, count = 0;
		if (!(game::client_read::buyItemInSlot(packet, vendorGuid, item, bagGuid, slot, count)))
		{
			WLOG("Coult not read CMSG_BUY_ITEM_IN_SLOT packet");
			return;
		}

		buyItemFromVendor(vendorGuid, item, bagGuid, slot, count);
	}

	void Player::handleSellItem(game::Protocol::IncomingPacket &packet)
	{
		UInt64 vendorGuid = 0, itemGuid = 0;
		UInt8 count = 0;
		if (!(game::client_read::sellItem(packet, vendorGuid, itemGuid, count)))
		{
			WLOG("Coult not read CMSG_SELL_ITEM packet");
			return;
		}

		UInt16 itemSlot = 0;
		if (!m_character->getInventory().findItemByGUID(itemGuid, itemSlot))
		{
			// Item not found
			return;
		}

		// Find the item by it's guid
		auto item = m_character->getInventory().getItemAtSlot(itemSlot);
		if (!item)
		{
			return;
		}

		UInt32 stack = item->getStackCount();
		UInt32 money = stack * item->getEntry().sellprice();
		if (money == 0)
		{
			WLOG("TODO: Can't sell that item");
			return;
		}

		m_character->getInventory().removeItem(itemSlot, stack);
		m_character->setUInt32Value(character_fields::Coinage, m_character->getUInt32Value(character_fields::Coinage) + money);
	}

	void Player::buyItemFromVendor(UInt64 vendorGuid, UInt32 itemEntry, UInt64 bagGuid, UInt8 slot, UInt8 count)
	{
		if (count < 1) count = 1;

		const UInt8 MaxBagSlots = 36;
		if (slot != 0xFF && slot >= MaxBagSlots)
			return;

		if (!m_character->isAlive())
			return;

		const auto *item = m_project.items.getById(itemEntry);
		if (!item)
			return;

		// Multiply item count
		UInt8 totalCount = count * item->buycount();
		auto *world = m_character->getWorldInstance();
		if (!world)
			return;

		GameCreature *creature = dynamic_cast<GameCreature*>(world->findObjectByGUID(vendorGuid));
		if (!creature)
			return;

		const auto *vendorEntry = m_project.vendors.getById(creature->getEntry().vendorentry());
		if (!vendorEntry || vendorEntry->items().empty())
			return;

		bool validEntry = false;
		for (auto &entry : vendorEntry->items())
		{
			if (entry.item() == item->id())
			{
				validEntry = true;
				break;
			}
		}

		if (!validEntry)
			return;

		// Take money
		UInt32 price = item->buyprice() * count;
		UInt32 money = m_character->getUInt32Value(character_fields::Coinage);
		if (money < price)
		{
			WLOG("Not enough money");
			return;
		}

		std::map<UInt16, UInt16> addedBySlot;
		auto result = m_character->getInventory().createItems(*item, totalCount, &addedBySlot);
		if (result != game::inventory_change_failure::Okay)
		{
			m_character->inventoryChangeFailure(result, nullptr, nullptr);
			return;
		}

		// Take money
		m_character->setUInt32Value(character_fields::Coinage, money - price);

		// Send push notifications
		for (auto &slot : addedBySlot)
		{
			auto item = m_character->getInventory().getItemAtSlot(slot.first);
			if (item)
			{
				sendProxyPacket(
					std::bind(game::server_write::itemPushResult, std::placeholders::_1,
						m_character->getGuid(), std::cref(*item), false, false, slot.first >> 8, slot.first & 0xFF, slot.second, m_character->getInventory().getItemCount(itemEntry)));
			}
		}
	}

	void Player::sendGossipMenu(UInt64 guid)
	{
		auto *world = m_character->getWorldInstance();
		if (!world)
		{
			return;
		}

		GameObject *target = world->findObjectByGUID(guid);
		if (!target)
		{
			return;
		}

		GameCreature *creature = (target->getTypeId() == object_type::Unit ? reinterpret_cast<GameCreature*>(target) : nullptr);
		WorldObject *object = (target->getTypeId() == object_type::GameObject ? reinterpret_cast<WorldObject*>(target) : nullptr);

		// TODO: Build gossip menu, but for now, we check in the following order:
		// Quest giver
		// Trainer
		// Vendor
		std::vector<game::QuestMenuItem> questMenu;
		if (creature)
		{
			for (const auto &questid : creature->getEntry().end_quests())
			{
				auto questStatus = m_character->getQuestStatus(questid);
				if (questStatus == game::quest_status::Incomplete ||
					questStatus == game::quest_status::Complete)
				{
					const auto *quest = m_project.quests.getById(questid);
					if (quest)
					{
						game::QuestMenuItem item;
						item.quest = quest;
						item.menuIcon = (questStatus == game::quest_status::Incomplete ?
							game::questgiver_status::Incomplete : game::questgiver_status::RewardRep);
						item.questLevel = quest->questlevel();
						item.title = quest->name();
						questMenu.emplace_back(std::move(item));
					}
				}
			}
			for (const auto &questid : creature->getEntry().quests())
			{
				auto questStatus = m_character->getQuestStatus(questid);
				if (questStatus == game::quest_status::Available)
				{
					const auto *quest = m_project.quests.getById(questid);
					if (quest)
					{
						game::QuestMenuItem item;
						item.quest = quest;
						item.menuIcon = game::questgiver_status::Chat;
						item.questLevel = quest->questlevel();
						item.title = quest->name();
						questMenu.emplace_back(std::move(item));
					}
				}
			}
		}
		else if (object)
		{
			for (const auto &questid : object->getEntry().end_quests())
			{
				auto questStatus = m_character->getQuestStatus(questid);
				if (questStatus == game::quest_status::Incomplete ||
					questStatus == game::quest_status::Complete)
				{
					const auto *quest = m_project.quests.getById(questid);
					assert(quest);

					game::QuestMenuItem item;
					item.quest = quest;
					item.menuIcon = questStatus == game::quest_status::Incomplete ?
						game::questgiver_status::Incomplete : game::questgiver_status::Reward;
					item.questLevel = quest->questlevel();
					item.title = quest->name();
					questMenu.emplace_back(std::move(item));
				}
			}
			for (const auto &questid : object->getEntry().quests())
			{
				auto questStatus = m_character->getQuestStatus(questid);
				if (questStatus == game::quest_status::Available)
				{
					const auto *quest = m_project.quests.getById(questid);
					assert(quest);

					game::QuestMenuItem item;
					item.quest = quest;
					item.menuIcon = game::questgiver_status::Chat;
					item.questLevel = quest->questlevel();
					item.title = quest->name();
					questMenu.emplace_back(std::move(item));
				}
			}
		}

		// Quests available?
		if (!questMenu.empty())
		{
			// Check if only one quest available and immediatly send quest details if so
			if (questMenu.size() == 1)
			{
				// Determine what packet to send based on quest status
				auto &menuItem = questMenu[0];
				switch(menuItem.menuIcon)
				{
				case game::questgiver_status::Chat:
				case game::questgiver_status::Available:
					sendProxyPacket(
						std::bind(game::server_write::questgiverQuestDetails, std::placeholders::_1, guid, std::cref(m_project.items), std::cref(*menuItem.quest)));
					break;
				case game::questgiver_status::Incomplete:
					if (!menuItem.quest->requestitemstext().empty())
						sendProxyPacket(std::bind(game::server_write::questgiverRequestItems, std::placeholders::_1, guid, true, false, std::cref(m_project.items), std::cref(*menuItem.quest)));
					else
						sendProxyPacket(std::bind(game::server_write::questgiverOfferReward, std::placeholders::_1, guid, false, std::cref(m_project.items), std::cref(*menuItem.quest)));
					break;
				case game::questgiver_status::Reward:
				case game::questgiver_status::RewardRep:
					if (!menuItem.quest->requestitemstext().empty())
						sendProxyPacket(std::bind(game::server_write::questgiverRequestItems, std::placeholders::_1, guid, true, true, std::cref(m_project.items), std::cref(*menuItem.quest)));
					else
						sendProxyPacket(std::bind(game::server_write::questgiverOfferReward, std::placeholders::_1, guid, true, std::cref(m_project.items), std::cref(*menuItem.quest)));
					break;
				}
				
			}
			else
			{
				// Send quest menu
				sendProxyPacket(
					std::bind(game::server_write::questgiverQuestList, std::placeholders::_1, guid, "", 0, 0, std::cref(questMenu)));
			}
		}
		else if(creature)
		{
			const auto *trainerEntry = m_project.trainers.getById(creature->getEntry().trainerentry());
			if (trainerEntry)
			{
				UInt32 titleId = 0;
				if (trainerEntry->type() == proto::TrainerEntry_TrainerType_CLASS_TRAINER)
				{
					if (trainerEntry->classid() != m_character->getClass())
					{
						// Not your class!
						return;
					}

					switch (m_character->getClass())
					{
						case game::char_class::Druid:
							titleId = 4913;
							break;
						case game::char_class::Hunter:
							titleId = 10090;
							break;
						case game::char_class::Mage:
							titleId = 328;
							break;
						case game::char_class::Paladin:
							titleId = 1635;
							break;
						case game::char_class::Priest:
							titleId = 4436;
							break;
						case game::char_class::Rogue:
							titleId = 4797;
							break;
						case game::char_class::Shaman:
							titleId = 5003;
							break;
						case game::char_class::Warlock:
							titleId = 5836;
							break;
						case game::char_class::Warrior:
							titleId = 4985;
							break;
					}
				}

				sendProxyPacket(
					std::bind(game::server_write::gossipMessage, std::placeholders::_1, guid, titleId));
				sendProxyPacket(
					std::bind(game::server_write::trainerList, std::placeholders::_1, std::cref(*m_character), guid, std::cref(*trainerEntry)));

				return;
			}
		}
	}

	void Player::handleGossipHello(game::Protocol::IncomingPacket &packet)
	{
		UInt64 npcGuid = 0;
		if (!(game::client_read::gossipHello(packet, npcGuid)))
		{
			return;
		}

		sendGossipMenu(npcGuid);
	}

	void Player::handleTrainerBuySpell(game::Protocol::IncomingPacket &packet)
	{
		UInt64 npcGuid = 0;
		UInt32 spellId = 0;
		if (!(game::client_read::trainerBuySpell(packet, npcGuid, spellId)))
		{
			return;
		}

		auto *world = m_character->getWorldInstance();
		if (!world)
		{
			return;
		}

		GameCreature *creature = dynamic_cast<GameCreature*>(world->findObjectByGUID(npcGuid));
		if (!creature)
		{
			return;
		}

		const auto *trainerEntry = m_project.trainers.getById(creature->getEntry().trainerentry());
		if (!trainerEntry)
		{
			return;
		}

		if (trainerEntry->type() == proto::TrainerEntry_TrainerType_CLASS_TRAINER &&
			m_character->getClass() != trainerEntry->classid())
		{
			return;
		}

		UInt32 cost = 0;
		const proto::SpellEntry *entry = nullptr;
		for (const auto &spell : trainerEntry->spells())
		{
			if (spell.spell() == spellId)
			{
				entry = m_project.spells.getById(spell.spell());
				cost = spell.spellcost();
				break;
			}
		}

		if (!entry)
		{
			return;
		}

		if (m_character->hasSpell(spellId))
		{
			return;
		}

		sendProxyPacket(
			std::bind(game::server_write::playSpellVisual, std::placeholders::_1, npcGuid, 0xB3));
		sendProxyPacket(
			std::bind(game::server_write::playSpellImpact, std::placeholders::_1, m_character->getGuid(), 0x016A));

		UInt32 money = m_character->getUInt32Value(character_fields::Coinage);
		if (money < cost)
			return;
		m_character->setUInt32Value(character_fields::Coinage, money - cost);

		m_character->addSpell(*entry);
		if ((entry->attributes(0) & game::spell_attributes::Passive) != 0)
		{
			SpellTargetMap targetMap;
			targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
			targetMap.m_unitTarget = m_character->getGuid();
			m_character->castSpell(std::move(targetMap), spellId);
		}

		sendProxyPacket(
			std::bind(game::server_write::learnedSpell, std::placeholders::_1, spellId));
		sendProxyPacket(
			std::bind(game::server_write::trainerBuySucceeded, std::placeholders::_1, npcGuid, spellId));
	}

	void Player::handleQuestgiverStatusQuery(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid = 0;
		if (!(game::client_read::questgiverStatusQuery(packet, guid)))
		{
			return;
		}

		// Can't find world instance
		auto *world = m_character->getWorldInstance();
		if (!world)
		{
			return;
		}

		// Can't find questgiver
		GameObject *questgiver = world->findObjectByGUID(guid);
		if (!questgiver)
		{
			return;
		}

		// Default status: None
		game::QuestgiverStatus status = game::questgiver_status::None;
		switch (questgiver->getTypeId())
		{
			case object_type::Unit:
			{
				GameCreature *creature = reinterpret_cast<GameCreature*>(questgiver);
				status = creature->getQuestgiverStatus(*m_character);
				break;
			}
			case object_type::GameObject:
			{
				WorldObject *object = reinterpret_cast<WorldObject*>(questgiver);
				status = object->getQuestgiverStatus(*m_character);
				break;
			}
			default:
			{
				// Unexpected object type!
				break;
			}
		}

		// Send answer
		sendProxyPacket(
			std::bind(game::server_write::questgiverStatus, std::placeholders::_1, guid, status));
	}

	void Player::handleQuestgiverStatusMultipleQuery(game::Protocol::IncomingPacket & packet)
	{
		std::map<UInt64, game::QuestgiverStatus> statusMap;

		// Find all potential quest givers near our character
		TileIndex2D tile;
		if (m_character->getTileIndex(tile))
		{
			forEachTileInSight(
				m_character->getWorldInstance()->getGrid(),
				tile,
				[&statusMap, this](VisibilityTile & tile) {
				for (auto *object : tile.getGameObjects())
				{
					switch (object->getTypeId())
					{
						case object_type::Unit:
						{
							GameCreature *creature = reinterpret_cast<GameCreature*>(object);
							if ((creature->getEntry().quests_size() || creature->getEntry().end_quests_size()) &&
								!creature->isHostileTo(*m_character))
							{
								statusMap[object->getGuid()] = creature->getQuestgiverStatus(*m_character);
							}
							break;
						}
						case object_type::GameObject:
						{
							WorldObject *worldObject = reinterpret_cast<WorldObject*>(object);
							if (worldObject->getEntry().quests_size() ||
								worldObject->getEntry().end_quests_size())
							{
								statusMap[object->getGuid()] = worldObject->getQuestgiverStatus(*m_character);
							}
							break;
						}
						default:	// Make the compiler happy
							break;
					}
				}
			});
		}

		// Send questgiver status map
		sendProxyPacket(
			std::bind(game::server_write::questgiverStatusMultiple, std::placeholders::_1, std::cref(statusMap)));
	}

	void Player::handleQuestgiverHello(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid = 0;
		if (!(game::client_read::questgiverStatusQuery(packet, guid)))
		{
			return;
		}

		sendGossipMenu(guid);
	}

	void Player::handleQuestgiverQueryQuest(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid = 0;
		UInt32 questId = 0;
		if (!(game::client_read::questgiverQueryQuest(packet, guid, questId)))
		{
			return;
		}

		const auto *quest = m_project.quests.getById(questId);
		if (!quest)
		{
			return;
		}

		GameObject *object = m_character->getWorldInstance()->findObjectByGUID(guid);
		if (!object)
		{
			return;
		}

		if (!object->providesQuest(questId))
		{
			return;
		}

		sendProxyPacket(
			std::bind(game::server_write::questgiverQuestDetails, std::placeholders::_1, guid, std::cref(m_project.items), std::cref(*quest)));
		//DLOG("CMSG_QUESTGIVER_QUERY_QUEST: 0x" << std::hex << std::setw(16) << std::setfill('0') << guid << "; Quest: " << std::dec << questId);
	}

	void Player::handleQuestgiverQuestAutolaunch(game::Protocol::IncomingPacket & packet)
	{
		if (!(game::client_read::questgiverQuestAutolaunch(packet)))
		{
			return;
		}

		DLOG("CMSG_QUESTGIVER_QUEST_AUTO_LAUNCH");
	}

	void Player::handleQuestgiverAcceptQuest(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid = 0;
		UInt32 questId = 0;
		if (!(game::client_read::questgiverAcceptQuest(packet, guid, questId)))
		{
			return;
		}

		// Check if that object exists and provides the requested quest
		GameObject *object = m_character->getWorldInstance()->findObjectByGUID(guid);
		if (!object ||
			!object->providesQuest(questId))
		{
			return;
		}

		// Accept that quest
		if (!m_character->acceptQuest(questId))
		{
			// We need this check since the quest can fail for various other reasons
			if (m_character->isQuestlogFull())
				sendProxyPacket(std::bind(game::server_write::questlogFull, std::placeholders::_1));
			return;
		}

		sendProxyPacket(
			std::bind(game::server_write::gossipComplete, std::placeholders::_1));
	}

	void Player::handleQuestgiverCompleteQuest(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid = 0;
		UInt32 questId = 0;
		if (!(game::client_read::questgiverCompleteQuest(packet, guid, questId)))
		{
			return;
		}

		const auto *quest = m_project.quests.getById(questId);
		if (!quest)
		{
			return;
		}

		GameObject *object = m_character->getWorldInstance()->findObjectByGUID(guid);
		if (!object)
		{
			return;
		}

		if (!object->endsQuest(questId))
		{
			return;
		}

		const bool hasCompleted = (m_character->getQuestStatus(questId) == game::quest_status::Complete);
		if (!quest->requestitemstext().empty())
			sendProxyPacket(std::bind(game::server_write::questgiverRequestItems, std::placeholders::_1, guid, true, hasCompleted, std::cref(m_project.items), std::cref(*quest)));
		else
			sendProxyPacket(std::bind(game::server_write::questgiverOfferReward, std::placeholders::_1, guid, hasCompleted, std::cref(m_project.items), std::cref(*quest)));
	}

	void Player::handleQuestgiverRequestReward(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid = 0;
		UInt32 questId = 0;
		if (!(game::client_read::questgiverRequestReward(packet, guid, questId)))
		{
			return;
		}

		const auto *quest = m_project.quests.getById(questId);
		if (!quest)
		{
			return;
		}

		// Check if that object exists and provides the requested quest
		GameObject *object = m_character->getWorldInstance()->findObjectByGUID(guid);
		if (!object ||
			!object->endsQuest(questId))
		{
			return;
		}

		// Check quest state
		auto state = m_character->getQuestStatus(questId);
		sendProxyPacket(std::bind(game::server_write::questgiverOfferReward, std::placeholders::_1, guid, 
			(state == game::quest_status::Complete), std::cref(m_project.items), std::cref(*quest)));
	}

	void Player::handleQuestgiverChooseReward(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid = 0;
		UInt32 questId = 0, reward = 0;
		if (!(game::client_read::questgiverChooseReward(packet, guid, questId, reward)))
		{
			return;
		}

		const auto *quest = m_project.quests.getById(questId);
		if (!quest)
		{
			return;
		}

		// Validate data
		if (reward > 0 &&
			reward >= static_cast<UInt32>(quest->rewarditemschoice_size()))
		{
			return;
		}

		GameObject *object = m_character->getWorldInstance()->findObjectByGUID(guid);
		if (!object)
		{
			return;
		}

		if (!object->endsQuest(questId))
		{
			return;
		}

		// Reward this quest
		bool result = m_character->rewardQuest(questId, reward, [this, quest](UInt32 xp) {
			sendProxyPacket(
				std::bind(game::server_write::questgiverQuestComplete, std::placeholders::_1, m_character->getLevel() >= 70, xp, std::cref(*quest)));
		});
		if (result)
		{
			// Try to find next quest and if there is one, send quest details
			UInt32 nextQuestId = quest->nextchainquestid();
			if (nextQuestId &&
				object->providesQuest(nextQuestId))
			{
				const auto *nextQuestEntry = m_project.quests.getById(nextQuestId);
				if (nextQuestEntry)
				{
					sendProxyPacket(
						std::bind(game::server_write::questgiverQuestDetails, std::placeholders::_1, guid, std::cref(m_project.items), std::cref(*nextQuestEntry)));
				}
			}
		}
	}

	void Player::handleQuestgiverCancel(game::Protocol::IncomingPacket & packet)
	{
		if (!(game::client_read::questgiverCancel(packet)))
		{
			return;
		}

		DLOG("CMSG_QUESTGIVER_CANCEL");
	}

	void Player::handleQuestlogRemoveQuest(game::Protocol::IncomingPacket & packet)
	{
		UInt8 index = 0;
		if (!(game::client_read::questlogRemoveQuest(packet, index)))
		{
			return;
		}

		if (index < 25)
		{
			UInt32 quest = m_character->getUInt32Value(character_fields::QuestLog1_1 + index * 4);
			if (quest)
			{
				m_character->abandonQuest(quest);
			}
		}
	}

}
