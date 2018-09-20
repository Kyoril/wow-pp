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
#include "game/mail.h"
#include "movement_validation.h"
#include "game/cheat_log.h"

using namespace std;

namespace wowpp
{
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
			onInventoryChangeFailure(result, nullptr, nullptr);
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

		// Consume this item
		auto playerGuid = m_character->getGuid();
		m_loot->takeItem(lootSlot, playerGuid);
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
			case game::inventory_type::Head:
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
			case game::inventory_type::Holdable:
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
			case game::inventory_type::Ranged:
			case game::inventory_type::RangedRight:
			case game::inventory_type::Thrown:
				targetSlot = player_equipment_slots::Ranged;
				break;
			default:
				if (entry.itemclass() == game::item_class::Container ||
					entry.itemclass() == game::item_class::Quiver)
				{
					for (UInt16 slot = player_inventory_slots::Start; slot < player_inventory_slots::End; ++slot)
					{
						auto bag = inv.getBagAtSlot(slot | 0xFF00);
						if (!bag)
						{
							targetSlot = slot;
							break;
						}
					}

					if (targetSlot == 0xFF)
					{
						m_character->inventoryChangeFailure(game::inventory_change_failure::NoEquipmentSlotAvailable, item.get(), nullptr);
						return;
					}
				}
				break;
		}

		// Check if valid slot found
		auto absDstSlot = Inventory::getAbsoluteSlot(player_inventory_slots::Bag_0, targetSlot);
		if (!Inventory::isEquipmentSlot(absDstSlot) && !Inventory::isBagPackSlot(absDstSlot))
		{
			ELOG("Invalid target slot: " << targetSlot);
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

		auto &inv = m_character->getInventory();
		auto result = inv.swapItems(
			Inventory::getAbsoluteSlot(srcBag, srcSlot),
			Inventory::getAbsoluteSlot(dstBag, dstSlot));
		if (!result)
		{
			// An error happened
		}
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
				// TODO: Handle pickpocket case?
				WLOG("Target creature is not dead and thus has no loot");
				return;
			}

			// Get loot from creature
			auto loot = creature->getUnitLoot();
			if (loot && !loot->isEmpty())
			{
				openLootDialog(loot, *creature);
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
			WLOG("Player is not looting anything");
			return;
		}

		// Find loot recipients
		auto lootGuid = m_loot->getLootGuid();
		auto *world = m_character->getWorldInstance();
		if (!world)
		{
			WLOG("Player not in world");
			return;
		}

		// Warning: takeGold may make m_loot invalid, because the loot will be reset if it is empty.
		UInt32 lootGold = m_loot->getGold();
		if (lootGold == 0)
		{
			WLOG("No gold to loot");
			return;
		}

		// Check if it's a creature
		std::vector<GameCharacter*> recipients;
		if (m_lootSource->getTypeId() == object_type::Unit)
		{
			// If looting a creature, loot has to be shared between nearby group members
			GameCreature *creature = reinterpret_cast<GameCreature*>(m_lootSource);
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
			// We will be the only recipient
			recipients.push_back(m_character.get());
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
				if (m_lootSource &&
					m_lootSource->getGuid() == lootGuid)
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

		if (m_lootSource &&
			m_lootSource->getGuid() != creatureId)
		{
			WLOG("Loot source mismatch!");
			return;
		}

		closeLootDialog();
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

	void Player::handleMovementCode(game::Protocol::IncomingPacket &packet, UInt16 opCode)
	{
		// NOTE: This seems to be possible when warrior-charging (spell 100) while in the air from a jump.
		if (m_character->getMover().isMoving()/* && opCode != game::client_packet::MoveSplineDone*/)
		{
			WLOG("Received movement packet 0x" << std::hex << opCode << " while spline movement is active - investigate!");
			//return;
		}

		// Read the movement info from the movement packet
		MovementInfo info;
		if (!(packet >> info))
		{
			WLOG("Invalid movement packet received from client");
			kick();
			return;
		}

		// If movement hasn't been initialized yet, this is the first packet that is used.
		// Because of that, we apply the time values that the client sent here.
		if (!m_movementInitialized)
		{
			auto serverInfo = m_character->getMovementInfo();
			serverInfo.time = info.time;
			serverInfo.jumpStartZ = info.z;

			// Player started falling
			if (info.moveFlags & game::movement_flags::Falling)
				info.jumpStartZ = info.z;

			// Detect wrong fall time values
			if (info.fallTime > 500)
			{
				CLOG("Fall time in first movement packet is too high!");
				kick();
				return;
			}

			m_character->setMovementInfo(info);
		}

		// Player starts falling down
		info.jumpStartZ = unitStartsFalling(info, m_character->getMovementInfo()) ? 
			info.z : m_character->getMovementInfo().jumpStartZ;

		// Make sure that there is no timed out pending movement change (lag tolerance)
		if (m_character->hasTimedOutPendingMovementChange())
		{
			CLOG("Player probably tried to skip or delay an ack packet");
			kick();
			return;
		}

		// We need the allowed speed value for the new movement info in order to validate some values like the
		// sent jumpXYspeed value.
		const float allowedNewSpeed = m_character->getExpectedSpeed(info, (info.moveFlags & game::movement_flags::FallingFar) != 0);

		// Validate that the client doesn't send fake data like wrong movement flags
		if (!validateMovementInfo(opCode, info, m_character->getMovementInfo(), allowedNewSpeed))
		{
			WLOG("Invalid movement info received from client!");
			kick();
			return;
		}

		// Validate movement speed
		if (opCode == game::client_packet::MoveSplineDone)
		{
			// TODO: This happens because of bad nav mesh in terms of correct z position which can result in
			// a much higher height after charging
			const auto& target = m_character->getMover().getTarget();
			if (::fabs(info.x - target.x) > FLT_EPSILON || 
				::fabs(info.y - target.y) > FLT_EPSILON || 
				::fabs(info.z - target.z) > 1.1f)	
			{
				CLOG("Invalid target location after move spline:");
				CLOG("\tdelta z: " << ::fabs(info.z - target.z));
			}
		}
		else
		{
			float expectedSpeed = m_character->getExpectedSpeed(m_character->getMovementInfo(), (info.moveFlags & game::movement_flags::FallingFar) != 0);
			if (!validateMovementSpeed(expectedSpeed, info, m_character->getMovementInfo()/*, !m_movementInitialized*/))
			{
				WLOG("Invalid movement speed for packet 0x" << std::hex << opCode);
				kick();
				return;
			}
		}
		
		// Sender guid
		auto guid = m_character->getGuid();

		// Get object location
		const math::Vector3 &location = m_character->getLocation();

		// Remove auras on swimming transitions
		if ((info.moveFlags & game::movement_flags::Swimming) != 0 &&
			(m_character->getMovementInfo().moveFlags & game::movement_flags::Swimming) == 0)
		{
			m_character->getAuras().removeAllAurasDueToInterrupt(game::spell_aura_interrupt_flags::NotAboveWater);
		}
		else if ((info.moveFlags & game::movement_flags::Swimming) == 0 &&
			(m_character->getMovementInfo().moveFlags & game::movement_flags::Swimming) != 0)
		{
			m_character->getAuras().removeAllAurasDueToInterrupt(game::spell_aura_interrupt_flags::NotUnderWater);
		}

		// Now that the movement info has been validated, we store it at the server.
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

		// Get grid tile
		(void)grid.requireTile(gridIndex);

		// Notify all watchers about the new object
		forEachTileInSight(
			getWorldInstance().getGrid(),
			gridIndex,
			[this, &info, opCode, guid](VisibilityTile &tile)
		{
			for (auto &watcher : tile.getWatchers())
			{
				if (watcher != this)
				{
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

							m_character->dealDamage(damage, 0, game::DamageType::Indirect, nullptr, true);
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
		m_character->relocate(math::Vector3(info.x, info.y, info.z), info.o, true);
		
		// Set movement initialized packet
		if (!m_movementInitialized)
		{
			m_movementInitialized = true;
		}

		// On Heartbeat, check for taverns
		if (opCode == game::client_packet::MoveHeartBeat)
		{
			// If the player left the tavern, remove rest state
			if (m_character->getRestType() == rest_type::Tavern && !m_character->isInRestAreaTrigger())
			{
				m_character->setRestType(rest_type::None, nullptr);
			}
		}
	}

	void Player::handleTimeSyncResponse(game::Protocol::IncomingPacket &packet)
	{
		UInt32 counter, ticks;
		if (!game::client_read::timeSyncResponse(packet, counter, ticks))
		{
			WLOG("Could not read packet data");
			return;
		}

		if (counter != m_timeSyncCounter - 1)
		{
			// TODO: What to do here?
			WLOG("TIME SYNC mismatch: Received response for #" << counter << ", but expected " << m_timeSyncCounter - 1);
		}

		GameTime current = getCurrentTime();

		// Determine latency
		Int64 latency = static_cast<Int64>(current) - m_lastTimeSync;
		Int64 halfLatency = latency / 2;

		m_serverSync = static_cast<UInt32>(current) + halfLatency;
		m_clientSync = ticks;

		//DLOG("TIME SYNC RESPONSE " << m_character->getName() << ": Client Sync " << m_clientSync << "; Server Sync: " << m_serverSync);
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
			ASSERT(dependson);

			// Check if we have learned the requested talent rank
			auto dependantRank = dependson->ranks(talent->dependsonrank());
			if (!m_character->hasSpell(dependantRank))
			{
				return;
			}
		}

		// Check if another spell is required
		if (talent->dependsonspell())
		{
			if (!m_character->hasSpell(talent->dependsonspell()))
			{
				return;
			}
		}

		// Check if we have enough talent points
		UInt32 freeTalentPoints = m_character->getUInt32Value(character_fields::CharacterPoints_1);
		if (freeTalentPoints == 0)
		{
			return;
		}

		// Check how many points we have spent in this talent tree already
		UInt32 spentPoints = 0;
		for (const auto &t : m_project.talents.getTemplates().entry())
		{
			// Same tab
			if (t.tab() == talent->tab())
			{
				for (Int32 i = 0; i < t.ranks_size(); ++i)
				{
					if (m_character->hasSpell(t.ranks(i)))
					{
						spentPoints += i + 1;
					}
				}
			}
		}

		if (spentPoints < (talent->row() * 5))
		{
			// Not enough points spent in talent tree
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
		if (m_character->addSpell(*spell))
		{
			if ((spell->attributes(0) & game::spell_attributes::Passive) != 0)
			{
				SpellTargetMap targets;
				targets.m_targetMap = game::spell_cast_target_flags::Unit;
				targets.m_unitTarget = m_character->getGuid();
				m_character->castSpell(std::move(targets), spell->id());
			}
		}
	}

	void Player::handleUseItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 bagId = 0, slotId = 0, spellCount = 0, castCount = 0;
		UInt64 itemGuid = 0;
		SpellTargetMap targetMap;
		if (!game::client_read::useItem(packet, bagId, slotId, spellCount, castCount, itemGuid, targetMap))
		{
			ELOG("Could not read packet");
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
		
		// Find all OnUse spells
		for (int i = 0; i < entry.spells_size(); ++i)
		{
			const auto &spell = entry.spells(i);
			if (!spell.spell())
			{
				WLOG("No spell entry");
				continue;
			}

			// Spell effect has to be triggered "on use", not "on equip" etc.
			if (spell.trigger() != game::item_spell_trigger::OnUse && spell.trigger() != game::item_spell_trigger::OnUseNoDelay)
			{
				continue;
			}

			// Look for the spell entry
			const auto *spellEntry = m_project.spells.getById(spell.spell());
			if (!spellEntry)
			{
				WLOG("Could not find spell by id " << spell.spell());
				continue;
			}
			
			// Cast the spell
			UInt64 time = spellEntry->casttime();
			m_character->castSpell(std::move(targetMap), spell.spell(), { 0, 0, 0 }, time, false, itemGuid, [this, spellEntry](game::SpellCastResult result) {
				if (result != game::spell_cast_result::CastOkay)
				{
					sendProxyPacket(
						std::bind(game::server_write::castFailed, std::placeholders::_1, result, std::cref(*spellEntry), 0));
				}
			});
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

		// Find vendor
		GameObject *vendor = m_instance.findObjectByGUID(vendorGuid);
		if (!vendor ||
			!vendor->isCreature())
		{
			sendProxyPacket(
				std::bind(game::server_write::sellItem, std::placeholders::_1, game::sell_error::CantFindVendor, 0, itemGuid, 0));
			return;
		}

		// Check interaction conditions
		if (!reinterpret_cast<GameUnit*>(vendor)->isInteractableFor(*m_character))
		{
			sendProxyPacket(
				std::bind(game::server_write::sellItem, std::placeholders::_1, game::sell_error::CantFindVendor, 0, itemGuid, 0));
			return;
		}

		UInt16 itemSlot = 0;
		if (!m_character->getInventory().findItemByGUID(itemGuid, itemSlot))
		{
			sendProxyPacket(
				std::bind(game::server_write::sellItem, std::placeholders::_1, game::sell_error::CantFindItem, vendorGuid, itemGuid, 0));
			return;
		}

		// Find the item by it's guid
		auto item = m_character->getInventory().getItemAtSlot(itemSlot);
		if (!item)
		{
			sendProxyPacket(
				std::bind(game::server_write::sellItem, std::placeholders::_1, game::sell_error::CantFindItem, vendorGuid, itemGuid, 0));
			return;
		}

		UInt32 stack = item->getStackCount();
		UInt32 money = stack * item->getEntry().sellprice();
		if (money == 0)
		{
			sendProxyPacket(
				std::bind(game::server_write::sellItem, std::placeholders::_1, game::sell_error::CantSellItem, vendorGuid, itemGuid, 0));
			return;
		}

		m_character->getInventory().removeItem(itemSlot, stack, true);
		m_character->setUInt32Value(character_fields::Coinage, m_character->getUInt32Value(character_fields::Coinage) + money);
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
			std::bind(game::server_write::trainerBuySucceeded, std::placeholders::_1, npcGuid, spellId));
	}

	void Player::handleGameObjectUse(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid = 0;
		if (!(game::client_read::gameobjectUse(packet, guid)))
		{
			return;
		}

		auto *obj = m_instance.findObjectByGUID(guid);
		if (!obj)
		{
			return;
		}

		if (!obj->isWorldObject())
		{
			return;
		}

		// Remove all auras that have to be removed when interacting with game objects
		m_character->getAuras().removeAllAurasDueToInterrupt(game::spell_aura_interrupt_flags::Use);

		sendGossipMenu(guid);

		WorldObject &wobj = reinterpret_cast<WorldObject&>(*obj);
		if (wobj.getEntry().type() == world_object_type::Goober)
		{
			// Possibly quest object
			m_character->onQuestObjectCredit(0, wobj);

			// Check if spell cast is needed
			UInt32 spellId = wobj.getEntry().data(10);
			if (spellId)
			{
				// Cast spell
				SpellTargetMap target;
				target.m_unitTarget = m_character->getGuid();
				target.m_targetMap = game::spell_cast_target_flags::Unit;
				m_character->castSpell(std::move(target), spellId);
			}
		}
		else if (wobj.getEntry().type() == world_object_type::SpellCaster)
		{
			UInt32 spellId = wobj.getEntry().data(0);
			if (spellId)
			{
				// Cast spell
				SpellTargetMap target;
				target.m_unitTarget = m_character->getGuid();
				target.m_targetMap = game::spell_cast_target_flags::Unit;
				m_character->castSpell(std::move(target), spellId);
			}
		}
	}

	void Player::handleOpenItem(game::Protocol::IncomingPacket & packet)
	{
		UInt8 bag = 0, slot = 0;
		if (!(game::client_read::openItem(packet, bag, slot)))
		{
			return;
		}

		auto &inv = m_character->getInventory();

		// Look for the item at the given slot
		auto item = inv.getItemAtSlot(Inventory::getAbsoluteSlot(bag, slot));
		if (!item)
		{
			m_character->inventoryChangeFailure(game::inventory_change_failure::ItemNotFound, nullptr, nullptr);
			return;
		}

		auto itemLoot = item->getLoot();
		if (itemLoot && !itemLoot->isEmpty())
		{
			openLootDialog(itemLoot, *item);
		}
		else
		{
			m_character->inventoryChangeFailure(game::inventory_change_failure::CantLootThatNow, item.get(), nullptr);
		}
	}

	void Player::handleMoveTimeSkipped(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid;
		UInt32 timeSkipped;
		if (!(game::client_read::moveTimeSkipped(packet, guid, timeSkipped)))
		{
			return;
		}

		if (guid != m_character->getGuid())
		{
			CLOG("Received MSG_MOVE_TIME_SKIPPED for non-controlled unit");
			kick();
			return;
		}

		// Time value mustn't be negative
		if (Int32(timeSkipped) < 0)
		{
			CLOG("Received MSG_MOVE_TIME_SKIPPED with negative time value " << Int32(timeSkipped));
			kick();
			return;
		}

		auto moveInfo = m_character->getMovementInfo();
		moveInfo.time += timeSkipped;
		m_character->setMovementInfo(moveInfo);
	}

	void Player::handleSetActionBarToggles(game::Protocol::IncomingPacket & packet)
	{
		UInt8 actionBars;
		if (!(game::client_read::setActionBarToggles(packet, actionBars)))
		{
			return;
		}

		// Save action bars
		m_character->setByteValue(character_fields::FieldBytes, 2, actionBars);
	}

	void Player::handleToggleHelm(game::Protocol::IncomingPacket & packet)
	{
		if (m_character->getUInt32Value(character_fields::CharacterFlags) & 1024)
		{
			m_character->removeFlag(character_fields::CharacterFlags, 1024);
		}
		else
		{
			m_character->addFlag(character_fields::CharacterFlags, 1024);
		}
	}

	void Player::handleToggleCloak(game::Protocol::IncomingPacket & packet)
	{
		if (m_character->getUInt32Value(character_fields::CharacterFlags) & 2048)
		{
			m_character->removeFlag(character_fields::CharacterFlags, 2048);
		}
		else
		{
			m_character->addFlag(character_fields::CharacterFlags, 2048);
		}
	}

	void Player::handleMailSend(game::Protocol::IncomingPacket & packet)
	{
		ObjectGuid currentMailbox;
		MailData mailInfo;

		if (!game::client_read::mailSend(packet, currentMailbox, mailInfo))
		{
			return;
		}

		if (mailInfo.receiver.empty())
		{
			return;
		}

		auto *world = m_character->getWorldInstance();
		if (!world)
		{
			return;
		}

		auto *target = world->findObjectByGUID(currentMailbox);
		if (!target)
		{
			// Checks if object exists and if it's a mailbox (TODO)
			return;
		}

		if (!target->isWorldObject() || reinterpret_cast<WorldObject*>(target)->getUInt32Value(world_object_fields::TypeID) != world_object_type::Mailbox)
		{
			return;
		}

		// TODO distance to mailbox
		//float distance = m_character->getDistanceTo(target);

		if (mailInfo.itemsCount > MaxItemsInMail)
		{
			sendProxyPacket(
				std::bind(game::server_write::mailSendResult, std::placeholders::_1,
						  MailResult(0, mail::response_type::Send, mail::response_result::TooManyAttachments)));
			return;
		}

		UInt32 cost = mailInfo.itemsCount > 0 ? 30 * mailInfo.itemsCount : 30;
		UInt32 reqMoney = cost + mailInfo.money;
		UInt32 plMoney = m_character->getUInt32Value(character_fields::Coinage);
		if (plMoney < reqMoney)
		{
			sendProxyPacket(
				std::bind(game::server_write::mailSendResult, std::placeholders::_1,
					MailResult(0, mail::response_type::Send, mail::response_result::NotEnoughMoney)));
			return;
		}

		auto &inventory = m_character->getInventory();
		UInt16 itemSlot = 0;
		std::vector<std::shared_ptr<GameItem>> items;
		for (UInt8 i = 0; i < mailInfo.itemsCount; ++i)
		{
			UInt64 guid = mailInfo.itemsGuids[i];
			if (!isItemGUID(guid))
			{
				sendProxyPacket(
					std::bind(game::server_write::mailSendResult, std::placeholders::_1,
						MailResult(0, mail::response_type::Send, mail::response_result::AttachmentInvalid)));
				return;
			}

			if (!inventory.findItemByGUID(guid, itemSlot))
			{
				sendProxyPacket(
					std::bind(game::server_write::mailSendResult, std::placeholders::_1,
						MailResult(0, mail::response_type::Send, mail::response_result::AttachmentInvalid)));
				return;
			}

			auto item = inventory.getItemAtSlot(itemSlot);
			if (!item)
			{
				sendProxyPacket(
					std::bind(game::server_write::mailSendResult, std::placeholders::_1,
						MailResult(0, mail::response_type::Send, mail::response_result::AttachmentInvalid)));
				return;
			}

			const auto &itemEntry = item->getEntry();

			if (itemEntry.flags() & game::item_flags::Bound)
			{
				sendProxyPacket(
					std::bind(game::server_write::mailSendResult, std::placeholders::_1,
						MailResult(0, mail::response_type::Send, mail::response_result::Equip, game::inventory_change_failure::MailBoundItem)));
				return;
			}

			if (item->getTypeId() == object_type::Container)
			{
				auto bagPtr = std::static_pointer_cast<GameBag>(item);
				if (bagPtr->isEmpty())
				{
					sendProxyPacket(
						std::bind(game::server_write::mailSendResult, std::placeholders::_1,
							MailResult(0, mail::response_type::Send, mail::response_result::Equip, game::inventory_change_failure::MailBoundItem)));
					return;
				}
			}

			if ((itemEntry.flags() & game::item_flags::Conjured) ||
				(item->getUInt32Value(item_fields::Duration)))
			{
				sendProxyPacket(
					std::bind(game::server_write::mailSendResult, std::placeholders::_1,
						MailResult(0, mail::response_type::Send, mail::response_result::Equip, game::inventory_change_failure::MailBoundItem)));
				return;
			}

			if ((mailInfo.COD) &&
				(itemEntry.flags() & game::item_flags::Wrapped))
			{
				sendProxyPacket(
					std::bind(game::server_write::mailSendResult, std::placeholders::_1,
						MailResult(0, mail::response_type::Send, mail::response_result::CannotSendWrappedCOD)));
				return;
			}

			// TODO other checks
			items.push_back(item);
		}

		// mail subject of more comprobations

		Mail mail(m_character->getGuid(), items, mailInfo, mailInfo.body.empty() ? mail::check_mask::Copied : mail::check_mask::HasBody);

		m_realmConnector.sendMailDraft(std::move(mail), mailInfo.receiver);

		//TODO

		DLOG("CMSG_MAIL_SEND received from client");
	}

	void Player::handleMailGetList(game::Protocol::IncomingPacket & packet)
	{
		ObjectGuid currentMailbox;

		if (!game::client_read::mailGetList(packet, currentMailbox))
		{
			return;
		}

		auto *world = m_character->getWorldInstance();
		if (!world)
		{
			return;
		}

		auto *target = world->findObjectByGUID(currentMailbox);
		if (!target)
		{
			// Checks if object exists and if it's a mailbox (TODO)
			return;
		}

		// TODO distance to mailbox
		//float distance = m_character->getDistanceTo(target);

		m_realmConnector.sendMailGetList(m_characterId);

		DLOG("CMSG_MAIL_GET_LIST receiver from client");
	}

	void Player::handleMailMarkAsRead(game::Protocol::IncomingPacket & packet)
	{
		ObjectGuid currentMailbox;
		UInt32 mailId;

		if (!game::client_read::mailMarkAsRead(packet, currentMailbox, mailId))
		{
			return;
		}

		auto *world = m_character->getWorldInstance();
		if (!world)
		{
			return;
		}

		auto *target = world->findObjectByGUID(currentMailbox);
		if (!target)
		{
			// Checks if object exists and if it's a mailbox (TODO)
			return;
		}

		// TODO distance to mailbox
		//float distance = m_character->getDistanceTo(target);

		m_realmConnector.sendMailMarkAsRead(m_characterId, mailId);
	}

	void Player::handleResurrectResponse(game::Protocol::IncomingPacket & packet)
	{
		UInt64 guid;
		UInt8 status;

		if (!game::client_read::resurrectResponse(packet, guid, status))
		{
			return;
		}

		if (m_character->isAlive())
		{
			return;
		}

		if (status == 0)
		{
			math::Vector3 location;
			m_character->setResurrectRequestData(0, 0, std::move(location), 0, 0);
			return;
		}

		if (!m_character->isResurrectRequestedBy(guid))
		{
			return;
		}

		m_character->resurrectUsingRequestData();
	}

	void Player::handleCancelChanneling(game::Protocol::IncomingPacket & packet)
	{
		if (!game::client_read::cancelChanneling(packet))
		{
			return;
		}

		m_character->finishChanneling();
	}

	void Player::handlePlayedTime(game::Protocol::IncomingPacket & packet)
	{
		updatePlayerTime();
		sendProxyPacket(
			std::bind(game::server_write::playedTime, std::placeholders::_1,
				m_character->getPlayTime(player_time_index::TotalPlayTime),	// Total time played in seconds
				m_character->getPlayTime(player_time_index::LevelPlayTime)	// Time on characters level in seconds
			)
		);
	}

	void Player::handleAckCode(game::Protocol::IncomingPacket & packet, UInt16 opCode)
	{
		// Read the guid values first
		UInt64 guid = 0;
		UInt32 index = 0;
		if (!(packet
			>> io::read<NetUInt64>(guid)
			>> io::read<NetUInt32>(index)))
		{
			WLOG("Could not read ack opcode 0x" << std::hex << opCode);
			return;
		}

		// Ack needs to be for the controlled character
		if (guid != m_character->getGuid())
		{
			CLOG("Received ack opcode from other character! OpCode: 0x" << std::hex << opCode << "; Index: " << std::dec << index);
			kick();
			return;
		}
		
		// Make sure that we have a pending movement change at all
		if (!m_character->hasPendingMovementChange())
		{
			CLOG("Received client ack packet without expecting any!");
			kick();
			return;
		}

		// Try to consume client ack
		PendingMovementChange change = m_character->popPendingMovementChange();
		if (change.counter != index)
		{
			CLOG("Received client ack with wrong index (different index expected)");
			kick();
			return;
		}

		// Read movement info
		MovementInfo info;
		if (opCode != game::client_packet::MoveTeleportAck)
		{
			if (!(packet >> info))
			{
				CLOG("Could not read movement info from ack packet 0x" << std::hex << opCode);
				return;
			}

			// Player starts falling down
			info.jumpStartZ = unitStartsFalling(info, m_character->getMovementInfo()) ?
				info.z : m_character->getMovementInfo().jumpStartZ;

			const float allowedSpeed = m_character->getExpectedSpeed(info, (info.moveFlags & game::movement_flags::FallingFar) != 0);

			// Validate movement info
			if (!validateMovementInfo(opCode, info, m_character->getMovementInfo(), allowedSpeed))
			{
				CLOG("Unexpected movement flags sent from client!");
				kick();
				return;
			}
		}
		
		// Used by speed change acks
		MovementType typeSent = movement_type::Count;
		float receivedSpeed = 0.0f;

		// Check op-code dependant checks and actions
		switch (opCode)
		{
			case game::client_packet::MoveHoverAck:
				if (change.changeType != MovementChangeType::Hover ||
					!validateMoveFlagsOnApply(change.apply, info.moveFlags, game::movement_flags::Hover))
				{
					CLOG("Invalid hover ack received! Expected change type was " << static_cast<UInt32>(change.changeType));
					kick();
					return;
				}
				break;
			case game::client_packet::MoveFeatherFallAck:
				if (change.changeType != MovementChangeType::FeatherFall ||
					!validateMoveFlagsOnApply(change.apply, info.moveFlags, game::movement_flags::SafeFall))
				{
					CLOG("Invalid feather fall ack received! Expected change type was " << static_cast<UInt32>(change.changeType));
					kick();
					return;
				}
				break;
			case game::client_packet::MoveWaterWalkAck:
				if (change.changeType != MovementChangeType::WaterWalk ||
					!validateMoveFlagsOnApply(change.apply, info.moveFlags, game::movement_flags::WaterWalking))
				{
					CLOG("Invalid water walk ack received! Expected change type was " << static_cast<UInt32>(change.changeType));
					kick();
					return;
				}
				break;
			case game::client_packet::ForceMoveRootAck:
			case game::client_packet::ForceMoveUnrootAck:
				if (change.changeType != MovementChangeType::Root ||
					!validateMoveFlagsOnApply(change.apply, info.moveFlags, (game::movement_flags::Root | game::movement_flags::PendingRoot)))
				{
					CLOG("Invalid root ack received! Expected change type was " << static_cast<UInt32>(change.changeType));
					kick();
					return;
				}

				break;
			case game::client_packet::ForceRunSpeedChangeAck:
			case game::client_packet::ForceRunBackSpeedChangeAck:
			case game::client_packet::ForceSwimSpeedChangeAck:
			case game::client_packet::ForceSwimBackSpeedChangeAck:
			case game::client_packet::ForceWalkSpeedChangeAck:
			case game::client_packet::ForceTurnRateChangeAck:
			case game::client_packet::ForceFlightSpeedChangeAck:
			case game::client_packet::ForceFlightBackSpeedChangeAck:
			{
				// Read the additional new speed value (note that this is in units/second).
				if (!(packet >> receivedSpeed))
				{
					WLOG("Incomplete ack packet data received!");
					kick();
					return;
				}

				// Validate that all parameters match the pending movement change and also determine
				// the movement type that should be altered based on the change.
				if (!validateSpeedAck(change, receivedSpeed, typeSent))
				{
					kick();
					return;
				}

				// Used to validate if the opCode matches the determined move code
				static UInt16 speedAckOpCodes[MovementType::Count] = {
					game::client_packet::ForceWalkSpeedChangeAck,
					game::client_packet::ForceRunSpeedChangeAck,
					game::client_packet::ForceRunBackSpeedChangeAck,
					game::client_packet::ForceSwimSpeedChangeAck,
					game::client_packet::ForceSwimBackSpeedChangeAck,
					game::client_packet::ForceTurnRateChangeAck,
					game::client_packet::ForceFlightSpeedChangeAck,
					game::client_packet::ForceFlightBackSpeedChangeAck
				};

				// Validate that the movement type has the expected value based on the opcode
				if (typeSent >= MovementType::Count ||
					opCode != speedAckOpCodes[typeSent])
				{
					WLOG("Wrong movement type in speed ack packet!");
					kick();
					return;
				}

				// Determine the base speed and make sure it's greater than 0, since we want to
				// use it as divider
				const float baseSpeed = m_character->getBaseSpeed(typeSent);
				ASSERT(baseSpeed > 0.0f);

				// Calculate the speed rate
				receivedSpeed /= baseSpeed;
				break;
			}
			case game::client_packet::MoveKnockBackAck:
				// Validate change type
				if (change.changeType != MovementChangeType::KnockBack)
				{
					CLOG("Received wrong ack op-code for expected ack!");
					kick();
					return;
				}

				// Check knock back parameters
				if (std::fabs(change.knockBackInfo.speedXY - info.jumpXYSpeed) > 0.01f ||
					std::fabs(change.knockBackInfo.speedZ - info.jumpVelocity) > 0.01f ||
					std::fabs(change.knockBackInfo.vcos - info.jumpCosAngle) > 0.01f ||
					std::fabs(change.knockBackInfo.vsin - info.jumpSinAngle) > 0.01f)
				{
					CLOG("Received invalid knock back info in MSG_MOVE_KNOCK_BACK_ACK!");
					kick();
					return;
				}

				break;
			case game::client_packet::MoveTeleportAck:
				// Validate change type
				if (change.changeType != MovementChangeType::Teleport)
				{
					CLOG("Received wrong ack op-code for expected ack!");
					kick();
					return;
				}

				DLOG("Received teleport ack towards " << change.teleportInfo.x << "," << change.teleportInfo.y << "," << change.teleportInfo.z);
				break;
			default:
				break;
		}
		
		// Apply movement info
		m_character->setMovementInfo(info);

		// Relocate the character
		auto location = math::Vector3(info.x, info.y, info.z);
		m_character->relocate(location, info.o, true);

		// TODO: Validate what time value needs to be sent to clients in the following 
		// packets to have smooth movement!
		//info.time = m_serverSync + (info.time - m_clientSync);

		// Perform application - we need to do this after all checks have been made
		switch (opCode)
		{
			case game::client_packet::ForceRunSpeedChangeAck:
			case game::client_packet::ForceRunBackSpeedChangeAck:
			case game::client_packet::ForceSwimSpeedChangeAck:
			case game::client_packet::ForceSwimBackSpeedChangeAck:
			case game::client_packet::ForceWalkSpeedChangeAck:
			case game::client_packet::ForceTurnRateChangeAck:
			case game::client_packet::ForceFlightSpeedChangeAck:
			case game::client_packet::ForceFlightBackSpeedChangeAck:
				// Apply speed rate so that anti cheat detection can detect speed hacks
				// properly now since we made sure that the client has received the speed
				// change and all following movement packets need to use these new speed
				// values.
				m_character->applySpeedChange(typeSent, receivedSpeed);
				break;
			case game::client_packet::MoveKnockBackAck:
			{
				// Transform into grid location
				TileIndex2D gridIndex;
				if (!m_character->getTileIndex(gridIndex))
				{
					ELOG("Could not resolve grid location!");
					return;
				}

				auto &grid = getWorldInstance().getGrid();
				(void)grid.requireTile(gridIndex);

				// Create knock back packet for observers
				std::vector<char> buffer;
				io::VectorSink sink(buffer);
				game::Protocol::OutgoingPacket movePacket(sink);
				game::server_write::moveKnockBackWithInfo(movePacket, m_character->getGuid(), info);

				// Notify all watchers
				forEachTileInSight(
					getWorldInstance().getGrid(),
					gridIndex,
					[this, &info, &buffer, &movePacket](VisibilityTile &tile)
				{
					for (auto &watcher : tile.getWatchers())
					{
						// We don't need to inform the player who sent the ack since he already received
						// the forced knockback packet.
						if (watcher != this)
						{
							watcher->sendPacket(movePacket, buffer);
						}
					}
				});
				break;
			}
		}
	}

	void Player::handleZoneUpdate(game::Protocol::IncomingPacket & packet)
	{
		UInt32 zoneId = 0;
		if (!(game::client_read::zoneUpdate(packet, zoneId)))
		{
			return;
		}

		// TODO: Validate zone id

		m_character->setZone(zoneId);
	}

	void Player::handleRepairItem(game::Protocol::IncomingPacket & packet)
	{
		UInt64 npcGuid = 0, itemGuid = 0;
		UInt8 guildBank = 0;
		if (!(game::client_read::repairItem(packet, npcGuid, itemGuid, guildBank)))
		{
			return;
		}

		// Find npc guid and perform some checks
		GameUnit* vendor = dynamic_cast<GameUnit*>(m_character->getWorldInstance()->findObjectByGUID(npcGuid));
		if (vendor == nullptr)
		{
			// Vendor doesn't exist
			WLOG("Vendor doesn't exist");
			return;
		}

		// Also make sure that this npc offers repair support at all
		if ((vendor->getUInt32Value(unit_fields::NpcFlags) & game::unit_npc_flags::Repair) == 0)
		{
			WLOG("Vendor exists and is friendly, but doesn't offer reparation");
			return;
		}

		// Perform interaction checks (distance, los, reputation etc.)
		if (!vendor->isInteractableFor(*m_character))
		{
			WLOG("Vendor isn't interactable!");
			return;
		}

		// TODO: Reduce repair price based on reputation

		UInt32 totalCost = 0;
		if (itemGuid != 0)
		{
			// Repair a single item
			UInt16 itemSlot = 0;
			if (!m_character->getInventory().findItemByGUID(itemGuid, itemSlot))
			{
				WLOG("Item to repair not found in players inventory");
				return;
			}
			
			// Calculate total cost by item at slot
			totalCost = m_character->getInventory().repairItem(itemSlot);
		}
		else
		{
			// Calculate total cost by all items in the inventory
			totalCost = m_character->getInventory().repairAllItems();
		}

		if (totalCost > 0)
		{
			WLOG("Not enough money");
			return;
		}
	}
	void Player::handleBuyBackItem(game::Protocol::IncomingPacket & packet)
	{
		UInt64 vendorGuid = 0;
		UInt32 slot = 0;
		if (!(game::client_read::buyBackItem(packet, vendorGuid, slot)))
		{
			return;
		}

		// Find npc guid and perform some checks
		GameUnit* vendor = dynamic_cast<GameUnit*>(m_character->getWorldInstance()->findObjectByGUID(vendorGuid));
		if (vendor == nullptr)
		{
			// Vendor doesn't exist
			WLOG("Vendor doesn't exist");
			return;
		}

		// Also make sure that this npc offers repair support at all
		if ((vendor->getUInt32Value(unit_fields::NpcFlags) & game::unit_npc_flags::Vendor) == 0)
		{
			WLOG("Vendor exists and is friendly, but doesn't offer reparation");
			return;
		}

		// Perform interaction checks (distance, los, reputation etc.)
		if (!vendor->isInteractableFor(*m_character))
		{
			WLOG("Vendor isn't interactable!");
			return;
		}

		auto &inv = m_character->getInventory();

		// Get the item instance...
		auto itemInst = inv.getItemAtSlot(static_cast<UInt16>(slot));
		if (!itemInst)
		{
			WLOG("Could not find buyback item at slot " << slot);
			return;
		}

		// Get the sell price
		const UInt32 fieldSlot = slot - player_buy_back_slots::Start;
		const UInt32 buyBackPrice = m_character->getUInt32Value(character_fields::BuybackPrice_1 + fieldSlot);
		const UInt32 coinage = m_character->getUInt32Value(character_fields::Coinage);
		if (coinage < buyBackPrice)
		{
			WLOG("Not enough money to buy back item!");
			return;
		}

		// ... clear character field slots...
		m_character->setUInt64Value(character_fields::VendorBuybackSlot_1 + (fieldSlot * 2), 0);
		m_character->setUInt32Value(character_fields::BuybackPrice_1 + fieldSlot, 0);
		m_character->setUInt32Value(character_fields::BuybackTimestamp_1 + fieldSlot, 0);

		// ... notify client about old item instance destruction
		inv.itemInstanceDestroyed(itemInst, slot);

		// ... and add the item back to the inventory!
		auto result = inv.addItem(itemInst);
		if (result != game::inventory_change_failure::Okay)
		{
			m_character->inventoryChangeFailure(result, itemInst.get(), nullptr);
			return;
		}
		else
		{
			// Remove money
			m_character->setUInt32Value(character_fields::Coinage, coinage - buyBackPrice);
		}
	}
}
