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
#include "common/macros.h"
#include "log/default_log_levels.h"
#include "game/world_instance_manager.h"
#include "game/world_instance.h"
#include "game/each_tile_in_sight.h"
#include "game/universe.h"
#include "proto_data/project.h"
#include "game/game_creature.h"
#include "game/game_world_object.h"
#include "game/game_bag.h"
#include "trade_data.h"
#include "game/unit_mover.h"

using namespace std;

namespace wowpp
{
	Player::Player(PlayerManager &manager, RealmConnector &realmConnector, WorldInstanceManager &worldInstanceManager, DatabaseId characterId, std::shared_ptr<GameCharacter> character, WorldInstance &instance, proto::Project &project, auth::AuthLocale locale)
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
		, m_groupUpdate(instance.getUniverse().getTimers())
		, m_lastPlayTimeUpdate(0)
		, m_serverSync(0)
		, m_clientSync(0)
		, m_nextClientSync(instance.getUniverse().getTimers())
		, m_timeSyncCounter(0)
		, m_movementInitialized(false)
		, m_locale(locale)
	{
		// Connect character signals
		m_characterSignals.append({
			m_character->spawned.connect(this, &Player::onSpawn),
			m_character->despawned.connect(this, &Player::onDespawn),
			m_character->tileChangePending.connect(this, &Player::onTileChangePending),
			m_logoutCountdown.ended.connect(this, &Player::onLogout),
			m_nextClientSync.ended.connect(this, &Player::onClientSync)
		});

		m_onProfChanged = m_character->proficiencyChanged.connect(this, &Player::onProficiencyChanged);
		m_onAtkSwingErr = m_character->autoAttackError.connect(this, &Player::onAttackSwingError);
		m_onInvFailure = m_character->inventoryChangeFailure.connect(this, &Player::onInventoryChangeFailure);
		m_onComboPoints = m_character->comboPointsChanged.connect(this, &Player::onComboPointsChanged);
		m_onXP = m_character->experienceGained.connect(this, &Player::onExperienceGained);
		m_onCastError = m_character->spellCastError.connect(this, &Player::onSpellCastError);
		m_onGainLevel = m_character->levelGained.connect(this, &Player::onLevelGained);
		m_onAuraUpdate = m_character->auraUpdated.connect(this, &Player::onAuraUpdated);
		m_onTargetAuraUpdate = m_character->targetAuraUpdated.connect(this, &Player::onTargetAuraUpdated);
		m_onTeleport = m_character->teleport.connect(this, &Player::onTeleport);
		m_onResurrectRequest = m_character->resurrectRequested.connect(this, &Player::onResurrectRequest);
		m_onCooldownEvent = m_character->cooldownEvent.connect([&](UInt32 spellId) {
			sendProxyPacket(std::bind(game::server_write::cooldownEvent, std::placeholders::_1, spellId, m_character->getGuid()));
		});
		m_questChanged = m_character->questDataChanged.connect([&](UInt32 questId, const QuestStatusData &data) {
			m_realmConnector.sendQuestData(m_character->getGuid(), questId, data);
			if (data.status == game::quest_status::Complete ||
				(data.status == game::quest_status::Incomplete && data.explored == true))
			{
				sendProxyPacket(std::bind(game::server_write::questupdateComplete, std::placeholders::_1, questId));
			}
		});
		m_questKill = m_character->questKillCredit.connect([&](const proto::QuestEntry &quest, UInt64 guid, UInt32 entry, UInt32 count, UInt32 total) {
			sendProxyPacket(std::bind(game::server_write::questupdateAddKill, std::placeholders::_1, quest.id(), entry, count, total, guid));
		});
		m_standStateChanged = m_character->standStateChanged.connect([&](UnitStandState state) {
			sendProxyPacket(std::bind(game::server_write::standStateUpdate, std::placeholders::_1, state));
		});
		m_objectInteraction = m_character->objectInteraction.connect([&](WorldObject &object)
		{
			if (object.getEntry().type() == world_object_type::QuestGiver)
				sendGossipMenu(object.getGuid());
		});
		m_onItemAdded = m_character->itemAdded.connect([this](UInt16 slot, UInt16 amount, bool looted, bool created) {
			auto inst = m_character->getInventory().getItemAtSlot(slot);
			if (inst)
			{
				UInt8 bag = 0, subslot = 0;
				Inventory::getRelativeSlots(slot, bag, subslot);
				const auto totalCount = m_character->getInventory().getItemCount(inst->getEntry().id());

				sendProxyPacket(
					std::bind(game::server_write::itemPushResult, std::placeholders::_1,
						m_character->getGuid(), std::cref(*inst), looted, created, bag, subslot, amount, totalCount));
			}
		});

		// Inventory change signals
		auto &inventory = m_character->getInventory();
		m_itemCreated = inventory.itemInstanceCreated.connect(this, &Player::onItemCreated);
		m_itemUpdated = inventory.itemInstanceUpdated.connect(this, &Player::onItemUpdated);
		m_itemDestroyed = inventory.itemInstanceDestroyed.connect(this, &Player::onItemDestroyed);

		// Loot signal
		m_onLootInspect = m_character->lootinspect.connect([&](std::shared_ptr<LootInstance> instance) {
			ASSERT(instance);

			auto *object = m_instance.findObjectByGUID(instance->getLootGuid());
			if (!object)
			{
				WLOG("Could not find loot source object: 0x" << std::hex << instance->getLootGuid());
				return;
			}
			openLootDialog(instance, *object);
		});

		// Root / stun change signals
		auto onRootOrStunUpdate = [&](UInt32 state, bool flag) {
			if (state == unit_state::Rooted || state == unit_state::Stunned)
			{
				if (flag || m_character->isRootedForSpell() || m_character->isStunned())
				{
					// Root the character
					if (m_character->isStunned())
					{
						m_character->addFlag(unit_fields::UnitFlags, game::unit_flags::Stunned);
					}
					/*sendProxyPacket(
						std::bind(game::server_write::forceMoveRoot, std::placeholders::_1, m_character->getGuid(), 2));*/
				}
				else
				{
					if (!m_character->isStunned())
					{
						m_character->removeFlag(unit_fields::UnitFlags, game::unit_flags::Stunned);
					}
					/*sendProxyPacket(
						std::bind(game::server_write::forceMoveUnroot, std::placeholders::_1, m_character->getGuid(), 0));*/
				}
			}
		};
		m_onUnitStateUpdate = m_character->unitStateChanged.connect(onRootOrStunUpdate);

		// Spell modifier applied or misapplied (changed)
		m_spellModChanged = m_character->spellModChanged.connect([&](SpellModType type, UInt8 bit, SpellModOp op, Int32 value) {
			sendProxyPacket(
				type == spell_mod_type::Flat ?
					std::bind(game::server_write::setFlatSpellModifier, std::placeholders::_1, bit, op, value) :
					std::bind(game::server_write::setPctSpellModifier, std::placeholders::_1, bit, op, value)
				);
		});

		// Group update signal
		m_groupUpdate.ended.connect([&]()
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

		// Initialize value
		m_lastPlayTimeUpdate = getCurrentTime();

		// Register as network unit watcher
		m_character->setNetUnitWatcher(this);
	}

	void Player::logoutRequest()
	{
		GameTime logoutTime = getCurrentTime();

		bool instantLogout = (m_character->getRestType() != rest_type::None);
		if (!instantLogout)
		{
			// Make our character sit down
			m_character->setStandState(unit_stand_state::Sit);

			// Root our character
			m_character->addFlag(unit_fields::UnitFlags, 0x00040000);
			m_character->setPendingMovementFlag(MovementChangeType::Root, true);

			// Send answer and engage logout process
			sendProxyPacket(
				std::bind(game::server_write::logoutResponse, std::placeholders::_1, true));

			// Player has to wait 20 seconds
			logoutTime += 20 * constants::OneSecond;
		}
		
		// Setup the logout countdown
		m_logoutCountdown.setEnd(logoutTime);
	}

	void Player::cancelLogoutRequest()
	{
		// Unroot
		m_character->removeFlag(unit_fields::UnitFlags, 0x00040000);
		m_character->setPendingMovementFlag(MovementChangeType::Root, false);

		// Stand up again
		m_character->setStandState(unit_stand_state::Stand);

		// Cancel the countdown
		m_logoutCountdown.cancel();
	}

	void Player::kick()
	{
		// Remove the character from the world
		m_instance.removeGameObject(*m_character);
		m_character.reset();

		// Notify the realm
		m_realmConnector.notifyWorldInstanceLeft(m_characterId, pp::world_realm::world_left_reason::Disconnect);

		// Remove player
		m_manager.playerDisconnected(*this);
	}

	void Player::onLogout()
	{
		// Remove net unit watcher
		m_character->setNetUnitWatcher(nullptr);

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

	void Player::saveCharacterData() const
	{
		if (m_character)
		{
			m_character->getAuras().serializeAuraData(m_character->getAuraData());
			m_realmConnector.sendCharacterData(*m_character);
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

		// Remember health and power values from before
		UInt32 health = m_character->getUInt32Value(unit_fields::Health);
		UInt32 power[5];
		for (Int32 i = 0; i < 5; ++i)
		{
			power[i] = m_character->getUInt32Value(unit_fields::Power1 + i);
		}

		// Create item spawn packets - this actually creates the item instances
		// Note: We create these blocks first, so that the character item fields can be set properly
		// since we want the right values in the spawn packet and we don't want to send another update 
		// packet for this right away
		m_character->getInventory().addSpawnBlocks(blocks);

		// Apply auras from aura data
		m_character->getAuras().restoreAuraData(m_character->getAuraData());

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
				<< io::write<NetUInt8>(updateType)
				<< io::write_packed_guid(guid)
				<< io::write<NetUInt8>(objectTypeId)
				<< io::write<NetUInt8>(updateFlags);

			// Write movement update
			{
				UInt32 moveFlags = 0x00;
				writer
					<< io::write<NetUInt32>(moveFlags)
					<< io::write<NetUInt8>(0x00)
					<< io::write<NetUInt32>(getCurrentTime());

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
			blocks.insert(blocks.begin(), std::move(createBlock));
		}

		// Send the actual spawn packet packet
		sendProxyPacket(
			std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(blocks)));

		// Send time sync request packet (this will also enable character movement at the client)
		onClientSync();

		// Find our tile
		VisibilityTile &tile = m_instance.getGrid().requireTile(getTileIndex());
		tile.getWatchers().add(this);

		// Cast passive spells after spawn, so that SpellMods are sent AFTER the spawn packet
		SpellTargetMap target;
		target.m_targetMap = game::spell_cast_target_flags::Self;
		target.m_unitTarget = m_character->getGuid();
		for (const auto &spell : m_character->getSpells())
		{
			if (spell->attributes(0) & game::spell_attributes::Passive)
			{
				m_character->castSpell(target, spell->id(), { 0, 0, 0 }, 0, true);
			}
			else
			{
				for (auto &eff : spell->effects())
				{
					if (eff.type() == game::spell_effects::Skill)
					{
						const auto *skill = m_project.skills.getById(eff.miscvaluea());
						if (skill)
						{
							m_character->addSkill(*skill);
						}
					}
				}
			}
		}

		// Restore health and power values, but limit them
		m_character->setUInt32Value(unit_fields::Health,
			std::min(m_character->getUInt32Value(unit_fields::MaxHealth), health));
		for (Int32 i = 0; i < 5; ++i)
		{
			m_character->setUInt32Value(unit_fields::Power1 + i,
				std::min(m_character->getUInt32Value(unit_fields::MaxPower1 + i), power[i]));
		}

		// Reset movement initialized flag
		m_movementInitialized = false;

		// Notify realm about this for post-spawn packets
		m_realmConnector.sendCharacterSpawnNotification(m_character->getGuid());

		// Subscribe for spell notifications
		m_onSpellLearned = m_character->spellLearned.connect(this, &Player::onSpellLearned);

		// Trigger regeneration for our character
		m_character->startRegeneration();

		auto moveInfo = m_character->getMovementInfo();
		moveInfo.moveFlags = game::movement_flags::Falling;
		m_character->setMovementInfo(moveInfo);
	}

	void Player::onDespawn(GameObject &/*despawning*/)
	{
		// No longer watch for network events
		m_character->setNetUnitWatcher(nullptr);

		// Cancel trade (if any)
		cancelTrade();

		updatePlayerTime();
		saveCharacterData();

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

		// Despawn old objects
		auto guid = m_character->getGuid();
		forEachTileInSightWithout(
			grid,
			oldTile.getPosition(),
			newTile.getPosition(),
			[this](VisibilityTile &tile)
		{
			for (auto *object : tile.getGameObjects().getElements())
			{
				ASSERT(object);
				if (!object->canSpawnForCharacter(*m_character))
				{
					continue;
				}

				// Dont spawn stealthed targets that we can't see yet
				if (object->isCreature() || object->isGameCharacter())
				{
					GameUnit &unit = reinterpret_cast<GameUnit&>(*object);
					if (unit.isStealthed() && !m_character->canDetectStealth(unit))
					{
						continue;
					}
				}

				this->sendProxyPacket(
					std::bind(game::server_write::destroyObject, std::placeholders::_1, object->getGuid(), false));
			}
		});

		// Spawn new objects
		forEachTileInSightWithout(
			grid,
			newTile.getPosition(),
			oldTile.getPosition(),
			[this](VisibilityTile &tile)
		{
			for (auto *obj : tile.getGameObjects().getElements())
			{
				ASSERT(obj);

				if (!obj->canSpawnForCharacter(*m_character))
				{
					continue;
				}

				// Dont spawn stealthed targets that we can't see yet
				if (obj->isCreature() || obj->isGameCharacter())
				{
					GameUnit &unit = reinterpret_cast<GameUnit&>(*obj);
					if (unit.isStealthed() && !m_character->canDetectStealth(unit))
					{
						continue;
					}
				}

				std::vector<std::vector<char>> createBlock;
				createUpdateBlocks(*obj, *m_character, createBlock);

				this->sendProxyPacket(
					std::bind(game::server_write::compressedUpdateObject, std::placeholders::_1, std::cref(createBlock)));

				// Send movement packets
				if (obj->isCreature() || obj->isGameCharacter())
				{
					reinterpret_cast<GameUnit*>(obj)->getMover().sendMovementPackets(*this);
				}
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
		updatePlayerTime(true);

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

		// Save character info at realm
		saveCharacterData();
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

			const UInt32 ackId = m_character->generateAckId();

			// Push movement change
			PendingMovementChange change;
			change.changeType = MovementChangeType::Teleport;
			change.timestamp = getCurrentTime();
			change.counter = ackId;
			change.teleportInfo.x = location.x;
			change.teleportInfo.y = location.y;
			change.teleportInfo.z = location.z;
			change.teleportInfo.o = o;
			m_character->pushPendingMovementChange(change);

			// Send teleport packet
			sendProxyPacket(
				std::bind(game::server_write::moveTeleportAck, std::placeholders::_1, m_character->getGuid(), std::cref(m_character->getMovementInfo()), ackId));
		}
		else
		{
			// Send transfer pending state. This will show up the loading screen at the client side and will
			// tell the realm where our character should be sent to
			sendProxyPacket(
				std::bind(game::server_write::transferPending, std::placeholders::_1, map, 0, 0));
			
			// Initialize teleport request at realm
			m_realmConnector.sendTeleportRequest(m_characterId, map, location, o);

			// Remove the character from the world (this will save the character)
			m_instance.removeGameObject(*m_character);
			m_character.reset();
			
			// Notify realm that our chracter left this world instance (this will commit the pending teleport request)
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
			UInt8 objectTypeId = item->getTypeId();

			UInt64 guid = item->getGuid();

			// Header with object guid and type
			createItemWriter
				<< io::write<NetUInt8>(updateType)
				<< io::write_packed_guid(guid)
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

	void Player::onItemUpdated(std::shared_ptr<GameItem> item, UInt16)
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
				<< io::write<NetUInt8>(updateType)
				<< io::write_packed_guid(guid);
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

	void Player::onSpellLearned(const proto::SpellEntry & spell)
	{
		if (spell.prevspell() != 0)
		{
			if (!canStackSpellRanksInSpellBook(spell))
			{
				sendProxyPacket(
					std::bind(game::server_write::supercededSpell, std::placeholders::_1, spell.prevspell(), spell.id()));
				return;
			}
		}

		sendProxyPacket(
			std::bind(game::server_write::learnedSpell, std::placeholders::_1, spell.id()));
	}

	void Player::onResurrectRequest(UInt64 objectGUID, const String &sentName, UInt8 typeId)
	{
		sendProxyPacket(
			std::bind(game::server_write::resurrectRequest, std::placeholders::_1, objectGUID, std::cref(sentName), typeId));
	}

	void Player::onClientSync()
	{
		// Send packet
		sendProxyPacket(
			std::bind(game::server_write::timeSyncReq, std::placeholders::_1, m_timeSyncCounter++));

		m_lastTimeSync = getCurrentTime();

		// Next sync in 30 seconds
		m_nextClientSync.setEnd(m_lastTimeSync + constants::OneSecond * 30);
	}

	void Player::onSpeedChangeApplied(MovementType type, float speed, UInt32 ackId)
	{
		sendProxyPacket(
			std::bind(game::server_write::sendForceSpeedChange, std::placeholders::_1, type, m_character->getGuid(), speed, ackId));
	}

	void Player::onCanFlyChangeApplied(bool canFly, UInt32 ackId)
	{
		if (canFly)
		{
			sendProxyPacket(std::bind(game::server_write::moveSetCanFly, std::placeholders::_1, m_character->getGuid(), ackId));
		}
		else
		{
			sendProxyPacket(std::bind(game::server_write::moveUnsetCanFly, std::placeholders::_1, m_character->getGuid(), ackId));
		}
	}

	void Player::onCanWaterWalkChangeApplied(bool canWaterWalk, UInt32 ackId)
	{
		if (canWaterWalk)
		{
			sendProxyPacket(std::bind(game::server_write::moveWaterWalk, std::placeholders::_1, m_character->getGuid(), ackId));
		}
		else
		{
			sendProxyPacket(std::bind(game::server_write::moveLandWalk, std::placeholders::_1, m_character->getGuid(), ackId));
		}
	}

	void Player::onHoverChangeApplied(bool hover, UInt32 ackId)
	{
		if (hover)
		{
			sendProxyPacket(std::bind(game::server_write::moveSetHover, std::placeholders::_1, m_character->getGuid(), ackId));
		}
		else
		{
			sendProxyPacket(std::bind(game::server_write::moveUnsetHover, std::placeholders::_1, m_character->getGuid(), ackId));
		}
	}

	void Player::onFeatherFallChangeApplied(bool featherFall, UInt32 ackId)
	{
		if (featherFall)
		{
			sendProxyPacket(std::bind(game::server_write::moveFeatherFall, std::placeholders::_1, m_character->getGuid(), ackId));
		}
		else
		{
			sendProxyPacket(std::bind(game::server_write::moveNormalFall, std::placeholders::_1, m_character->getGuid(), ackId));
		}
	}

	void Player::onRootChangeApplied(bool rooted, UInt32 ackId)
	{
		if (rooted)
		{
			sendProxyPacket(std::bind(game::server_write::forceMoveRoot, std::placeholders::_1, m_character->getGuid(), ackId));
		}
		else
		{
			sendProxyPacket(std::bind(game::server_write::forceMoveUnroot, std::placeholders::_1, m_character->getGuid(), ackId));
		}
	}

	void Player::setFallInfo(UInt32 time, float z)
	{
		m_lastFallTime = time;
		m_lastFallZ = z;
	}

	bool Player::isIgnored(UInt64 guid) const
	{
		return m_ignoredGUIDs.contains(guid);
	}

	UInt32 Player::convertTimestamp(UInt32 otherTimestamp, UInt32 otherTick) const
	{
		return otherTimestamp;//return (UInt32)(otherTimestamp - m_clientTimeDiff);
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

		GameCreature *creature = (target->isCreature() ? reinterpret_cast<GameCreature*>(target) : nullptr);
		WorldObject *object = (target->isWorldObject() ? reinterpret_cast<WorldObject*>(target) : nullptr);

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
						const String &questName = (quest->name_loc_size() >= m_locale) ? quest->name_loc(m_locale - 1) : quest->name();

						game::QuestMenuItem item;
						item.quest = quest;
						item.menuIcon = (questStatus == game::quest_status::Incomplete ?
							game::questgiver_status::Incomplete : game::questgiver_status::RewardRep);
						item.questLevel = quest->questlevel();
						item.title = questName.empty() ? quest->name() : questName;
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
						const String &questName = (quest->name_loc_size() >= m_locale) ? quest->name_loc(m_locale - 1) : quest->name();

						game::QuestMenuItem item;
						item.quest = quest;
						item.menuIcon = game::questgiver_status::Chat;
						item.questLevel = quest->questlevel();
						item.title = questName.empty() ? quest->name() : questName;
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
					ASSERT(quest);

					const String &questName = (quest->name_loc_size() >= m_locale) ? quest->name_loc(m_locale - 1) : quest->name();

					game::QuestMenuItem item;
					item.quest = quest;
					item.menuIcon = questStatus == game::quest_status::Incomplete ?
						game::questgiver_status::Incomplete : game::questgiver_status::Reward;
					item.questLevel = quest->questlevel();
					item.title = questName.empty() ? quest->name() : questName;
					questMenu.emplace_back(std::move(item));
				}
			}
			for (const auto &questid : object->getEntry().quests())
			{
				auto questStatus = m_character->getQuestStatus(questid);
				if (questStatus == game::quest_status::Available)
				{
					const auto *quest = m_project.quests.getById(questid);
					ASSERT(quest);

					const String &questName = (quest->name_loc_size() >= m_locale) ? quest->name_loc(m_locale - 1) : quest->name();

					game::QuestMenuItem item;
					item.quest = quest;
					item.menuIcon = game::questgiver_status::Chat;
					item.questLevel = quest->questlevel();
					item.title = questName.empty() ? quest->name() : questName;
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
						std::bind(game::server_write::questgiverQuestDetails, std::placeholders::_1, m_locale, guid, std::cref(m_project.items), std::cref(*menuItem.quest)));
					break;
				case game::questgiver_status::Incomplete:
					if (!menuItem.quest->requestitemstext().empty())
						sendProxyPacket(std::bind(game::server_write::questgiverRequestItems, std::placeholders::_1, m_locale, guid, true, false, std::cref(m_project.items), std::cref(*menuItem.quest)));
					else
						sendProxyPacket(std::bind(game::server_write::questgiverOfferReward, std::placeholders::_1, m_locale, guid, false, std::cref(m_project.items), std::cref(*menuItem.quest)));
					break;
				case game::questgiver_status::Reward:
				case game::questgiver_status::RewardRep:
					if (!menuItem.quest->requestitemstext().empty())
						sendProxyPacket(std::bind(game::server_write::questgiverRequestItems, std::placeholders::_1, m_locale, guid, true, true, std::cref(m_project.items), std::cref(*menuItem.quest)));
					else
						sendProxyPacket(std::bind(game::server_write::questgiverOfferReward, std::placeholders::_1, m_locale, guid, true, std::cref(m_project.items), std::cref(*menuItem.quest)));
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
			// Is battlemaster?
			if (creature->getUInt32Value(unit_fields::NpcFlags) & game::unit_npc_flags::Battlemaster)
			{
				sendProxyPacket(
					std::bind(game::server_write::gossipMessage, std::placeholders::_1, creature->getGuid(), 7599));
				return;
			}

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

			// Check if that vendor has the vendor flag
			if ((creature->getUInt32Value(unit_fields::NpcFlags) & game::unit_npc_flags::Vendor) == 0)
				return;

			// Check if the vendor DO sell items
			const auto *vendorEntry = m_project.vendors.getById(creature->getEntry().vendorentry());
			if (!vendorEntry)
			{
				std::vector<proto::VendorItemEntry> emptyList;
				sendProxyPacket(
					std::bind(game::server_write::listInventory, std::placeholders::_1, creature->getGuid(), std::cref(m_project.items), std::cref(emptyList)));
				return;
			}

			// TODO
			std::vector<proto::VendorItemEntry> list;
			for (const auto &entry : vendorEntry->items())
			{
				list.push_back(entry);
			}

			sendProxyPacket(
				std::bind(game::server_write::listInventory, std::placeholders::_1, creature->getGuid(), std::cref(m_project.items), std::cref(list)));
		}
	}

	void Player::openLootDialog(std::shared_ptr<LootInstance> loot, GameObject & source)
	{
		ASSERT(loot);

		// Close old dialog if any
		closeLootDialog();

		// Remember those parameters
		m_loot = loot;
		m_lootSource = &source;

		// Add the looting flag to our character
		m_character->addFlag(unit_fields::UnitFlags, game::unit_flags::Looting);

		m_character->cancelCast(game::spell_interrupt_flags::None);

		// If the source despawns, we will close the loot window
		m_onLootInvalidate = source.despawned.connect([this](GameObject &despawned)
		{
			closeLootDialog();
		});

		// If the loot is cleared, we will also close the loot window
		m_onLootCleared = m_loot->cleared.connect([this]()
		{
			closeLootDialog();
		});

		m_lootSignals.append({
			m_loot->itemRemoved.connect([this](UInt8 slot)
			{
				sendProxyPacket(
					std::bind(game::server_write::lootRemoved, std::placeholders::_1, slot));
			})
		});

		// Send the actual loot data (TODO: Determine loot type)
		auto guid = source.getGuid();
		auto lootType = game::loot_type::Corpse;
		if (!isItemGUID(guid))
		{
			m_character->getAuras().removeAurasByType(game::aura_type::Mounted);
		}

		UInt64 playerGuid = m_character->getGuid();
		sendProxyPacket(
			std::bind(game::server_write::lootResponse, std::placeholders::_1, guid, lootType, playerGuid, std::cref(*loot)));
	}

	void Player::closeLootDialog()
	{
		// Disconnect all signals
		m_lootSignals.disconnect();

		if (m_loot)
		{
			// Notify loot watchers about this
			m_loot->closed(m_character->getGuid());

			// Notify the client
			sendProxyPacket(
				std::bind(game::server_write::lootReleaseResponse, std::placeholders::_1, m_loot->getLootGuid()));

			// Character is no longer looting
			m_character->removeFlag(unit_fields::UnitFlags, game::unit_flags::Looting);

			// Reset variables
			m_loot = nullptr;
			m_lootSource = nullptr;

			// Disconnect all loot related signals
			m_onLootCleared.disconnect();
			m_onLootInvalidate.disconnect();
		}
	}

	void Player::initiateTrade(UInt64 target)
	{
		// Cancel current trade if any
		if (isTrading())
			cancelTrade();

		// Check for character existance
		if (!m_character)
			return;

		// Can't trade while dead
		if (!m_character->isAlive())
		{
			WLOG("Player is dead and thus can't trade");
			sendTradeStatus(game::trade_status::YouDead);
			return;
		}

		// Can't trade while stunned as well
		if (m_character->isStunned())
		{
			WLOG("Player is stunned and thus can't trade");
			sendTradeStatus(game::trade_status::YouStunned);
			return;
		}

		// Can't trade while logout is pending
		if (isLogoutPending())
		{
			WLOG("Player has a pending logout and thus can't trade");
			sendTradeStatus(game::trade_status::YouLogout);
			return;
		}

		auto *worldInstance = m_character->getWorldInstance();
		if (!worldInstance)
		{
			return;
		}

		UInt64 thisguid = m_character->getGuid();

		// Find other player instance
		auto *otherPlayer = m_manager.getPlayerByCharacterGuid(target);
		if (!otherPlayer)
		{
			WLOG("Can't find target player");
			sendTradeStatus(game::trade_status::NoTarget);
			return;
		}

		// Get other players character (should never be nullptr, but just in case...)
		auto otherCharacter = otherPlayer->getCharacter();
		if (!otherCharacter)
		{
			WLOG("Can't find target players character");
			sendTradeStatus(game::trade_status::NoTarget);
			return;
		}

		// Target has to be alive
		if (!otherCharacter->isAlive())
		{
			WLOG("Target is dead and thus can't trade");
			sendTradeStatus(game::trade_status::TargetDead);
			return;
		}

		// Target may not be stunned
		if (otherCharacter->isStunned())
		{
			WLOG("Target is stunned and thus can't trade");
			sendTradeStatus(game::trade_status::TargetStunned);
			return;
		}

		// Target logout check
		if (otherPlayer->isLogoutPending())
		{
			WLOG("Target has a pending logout and thus can't trade");
			sendTradeStatus(game::trade_status::TargetLogout);
			return;
		}

		// Check if target is busy
		if (otherPlayer->isTrading())
		{
			WLOG("Target is already trading and thus can't trade");
			sendTradeStatus(game::trade_status::Busy2);
			return;
		}

		// Trade distance check (100 = 10*10 because of squared check for performance reasons)
		// TODO: Is this the correct trade distance?
		if (otherCharacter->getSquaredDistanceTo(*m_character, false) > 100.0f)
		{
			WLOG("Player is too far away from target and thus can't trade");
			sendTradeStatus(game::trade_status::TargetTooFar);
			return;
		}

		// Begin trade (both players share the same trade data session)
		auto tradeData = std::make_shared<TradeData>(*this, *otherPlayer);
		setTradeSession(tradeData);
		otherPlayer->setTradeSession(tradeData);
		otherPlayer->sendTradeStatus(game::trade_status::BeginTrade, m_character->getGuid());
	}

	void Player::cancelTrade()
	{
		// Reset trade data
		if (m_tradeData)
		{
			// Calling the cancel-method will automatically reset m_tradeData in this method
			m_tradeData->cancel();
		}
	}

	void Player::setTradeSession(std::shared_ptr<TradeData> data)
	{
		// TODO: Disconnect signals

		m_tradeData = std::move(data);
		if (m_tradeData)
		{
			// TODO: Connect the respective signals
		}
	}

	void Player::updatePlayerTime(bool resetLevelTime/* = false*/)
	{
		// Get current time
		const GameTime now = getCurrentTime();

		// Calculate delta in milliseconds
		const UInt32 diff = static_cast<UInt32>((now - m_lastPlayTimeUpdate) / constants::OneSecond);

		// Reset player time value
		m_lastPlayTimeUpdate = getCurrentTime();
		if (resetLevelTime)
		{
			m_character->setPlayTime(player_time_index::LevelPlayTime, 0);
		}
		else
		{
			m_character->setPlayTime(player_time_index::LevelPlayTime,
				m_character->getPlayTime(player_time_index::LevelPlayTime) + diff);
		}

		m_character->setPlayTime(player_time_index::TotalPlayTime, 
			m_character->getPlayTime(player_time_index::TotalPlayTime) + diff);

	}

	void Player::sendTradeStatus(TradeStatus status, UInt64 guid/* = 0*/, UInt32 errorCode/* = 0*/, UInt32 itemCategory/* = 0*/)
	{
		sendProxyPacket(
			std::bind(game::server_write::sendTradeStatus, std::placeholders::_1, static_cast<UInt32>(status), guid, errorCode != 0, errorCode, itemCategory));
	}
}
