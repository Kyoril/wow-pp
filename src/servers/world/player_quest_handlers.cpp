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

using namespace std;

namespace wowpp
{
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

		if (isItemGUID(guid))
		{
			UInt16 itemSlot = 0;
			if (!m_character->getInventory().findItemByGUID(guid, itemSlot))
			{
				return;
			}

			auto item = m_character->getInventory().getItemAtSlot(itemSlot);
			if (!item)
			{
				return;
			}

			if (item->getEntry().questentry() != questId)
			{
				return;
			}
		}
		else
		{
			GameObject *object = m_character->getWorldInstance()->findObjectByGUID(guid);
			if (!object)
			{
				return;
			}

			if (!object->providesQuest(questId))
			{
				return;
			}
		}

		sendProxyPacket(
			std::bind(game::server_write::questgiverQuestDetails, std::placeholders::_1, m_locale, guid, std::cref(m_project.items), std::cref(*quest)));
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

		// Check if the quest exists
		const proto::QuestEntry* quest = m_project.quests.getById(questId);
		if (!quest)
		{
			WLOG("Tried to accept unknown quest id");
			return;
		}

		UInt16 itemSlot = 0;
		GameObject* questGiver = nullptr;
		std::shared_ptr<GameItem> itemQuestgiver;
		if (isItemGUID(guid))
		{
			if (!m_character->getInventory().findItemByGUID(guid, itemSlot))
			{
				return;
			}

			itemQuestgiver = m_character->getInventory().getItemAtSlot(itemSlot);
			if (!itemQuestgiver)
			{
				return;
			}

			if (itemQuestgiver->getEntry().questentry() != questId)
			{
				return;
			}

			questGiver = itemQuestgiver.get();
		}
		else
		{
			// Check if that object exists and provides the requested quest
			questGiver = m_character->getWorldInstance()->findObjectByGUID(guid);
			if (!questGiver ||
				!questGiver->providesQuest(questId))
			{
				return;
			}
		}

		// We need this check since the quest can fail for various other reasons
		if (m_character->isQuestlogFull())
		{
			sendProxyPacket(std::bind(game::server_write::questlogFull, std::placeholders::_1));
			return;
		}

		// Remove quest item now (we need to do this before accepting the quest as some quests re-add the source quest item)
		if (itemQuestgiver && itemSlot != 0)
		{
			auto result = m_character->getInventory().removeItem(itemSlot);
			if (result != game::inventory_change_failure::Okay)
			{
				m_character->inventoryChangeFailure(result, itemQuestgiver.get(), nullptr);
				return;
			}
		}

		// Accept that quest
		if (!m_character->acceptQuest(questId))
		{
			if (itemQuestgiver && itemSlot != 0)
			{
				// Try to restore previously given quest item (TODO: This is ugly and could be a security issue because, in theory,
				// this could lead to creating the item twice etc.)
				auto result = m_character->getInventory().createItems(itemQuestgiver->getEntry(), itemQuestgiver->getStackCount());
				if (result != game::inventory_change_failure::Okay)
				{
					// Worst case! Player has lost the quest item... this may NEVER EVER happen (need for an inventory transaction system)
					ELOG("PLAYER " << m_character->getGuid() << " ITEM LOSS SINCE QUEST " << questId << " COULD NOT BE ACCEPTED AND QUESTGIVER ITEM "
						<< itemQuestgiver->getStackCount() << "x " << itemQuestgiver->getEntry().id() << " COULD NOT BE RECREATED!");
					ASSERT(false);
				}
			}

			return;
		}

		// Quest accepted, so we will execute all quest triggers
		for (const UInt32 triggerId : quest->starttriggers())
		{
			// Find that trigger entry
			const proto::TriggerEntry* trigger = m_project.triggers.getById(triggerId);
			if (!trigger)
			{
				WLOG("Unknown trigger id " << triggerId << " in start trigger list for quest " << questId);
				continue;
			}

			// Execute the trigger
			if (questGiver)
			{
				if (questGiver->isCreature())
				{
					reinterpret_cast<GameUnit*>(questGiver)->unitTrigger(*trigger, *reinterpret_cast<GameUnit*>(questGiver), m_character.get());
				}
				else if (questGiver->isWorldObject())
				{
					reinterpret_cast<WorldObject*>(questGiver)->objectTrigger(*trigger, *reinterpret_cast<WorldObject*>(questGiver));
				}
			}
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
			sendProxyPacket(std::bind(game::server_write::questgiverRequestItems, std::placeholders::_1, m_locale, guid, true, hasCompleted, std::cref(m_project.items), std::cref(*quest)));
		else
			sendProxyPacket(std::bind(game::server_write::questgiverOfferReward, std::placeholders::_1, m_locale, guid, hasCompleted, std::cref(m_project.items), std::cref(*quest)));
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
		sendProxyPacket(std::bind(game::server_write::questgiverOfferReward, std::placeholders::_1, m_locale, guid,
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
			// If the quest should perform a spell cast on the players character, we will do so now
			if (quest->rewardspellcast())
			{
				// Prepare the spell cast target map
				SpellTargetMap targetMap;
				targetMap.m_unitTarget = m_character->getGuid();
				targetMap.m_targetMap = game::spell_cast_target_flags::Unit;

				// Determine the unit which should cast the spell - if the quest rewarder is a unit, we make
				// him cast the spell on the player. Otherwise, we make the player cast the spell on himself.
				GameUnit* spellCaster = (object->isCreature() ? reinterpret_cast<GameUnit*>(object) : m_character.get());
				ASSERT(spellCaster); 

				// Finally, to the spell cast
				spellCaster->castSpell(std::move(targetMap), quest->rewardspellcast(), { 0,0,0 }, 0, true);
			}


			// Try to find next quest and if there is one, send quest details
			UInt32 nextQuestId = quest->nextchainquestid();
			if (nextQuestId &&
				object->providesQuest(nextQuestId) &&
				m_character->getQuestStatus(nextQuestId) == game::quest_status::Available)
			{
				const auto *nextQuestEntry = m_project.quests.getById(nextQuestId);
				if (nextQuestEntry)
				{
					sendProxyPacket(
						std::bind(game::server_write::questgiverQuestDetails, std::placeholders::_1, m_locale, guid, std::cref(m_project.items), std::cref(*nextQuestEntry)));
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
