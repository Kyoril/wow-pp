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

#pragma once

#include "game_protocol/game_protocol.h"
#include "game_protocol/game_connection.h"
#include "game_protocol/game_crypted_connection.h"
#include "common/big_number.h"
#include "common/countdown.h"
#include "common/linear_set.h"
#include "game/game_character.h"
#include "binary_io/vector_sink.h"
#include "realm_connector.h"
#include "game/each_tile_in_region.h"
#include "game/world_instance.h"
#include "game/tile_subscriber.h"
#include "game/loot_instance.h"
#include "common/macros.h"
#include "trade_data.h"

namespace wowpp
{
	typedef game::trade_status::Type TradeStatus;

	// Forwards
	class PlayerManager;
	class WorldInstanceManager;
	namespace proto
	{
		class Project;
	}

	/// Player connection class.
	class Player final
		: public ITileSubscriber
	{
	private:

		Player(const Player &Other) = delete;
		Player &operator=(const Player &Other) = delete;

	public:

		/// Creates a new instance of the player class and registers itself as the
		/// active connection listener.
		/// @param manager Reference to the player manager which manages all connected players on this realm.
		/// @param loginConnector Reference to the login connector for communication with the login server.
		/// @param worldManager Reference to the world manager which manages all connected world nodes.
		/// @param database Reference to the database system.
		/// @param project Reference to the data project.
		/// @param connection The connection instance as a shared pointer.
		/// @param address The remote address of the player (ip address) as string.
		explicit Player(PlayerManager &manager,
						RealmConnector &realmConnector,
						WorldInstanceManager &worldInstanceManager,
						DatabaseId characterId,
						std::shared_ptr<GameCharacter> character,
						WorldInstance &instance,
						proto::Project &project);

		/// Gets the player manager which manages all connected players.
		PlayerManager &getManager() const { return m_manager; }
		/// Gets the database identifier of the character (which is provided by the realm).
		const DatabaseId &getCharacterId() const { return m_characterId; }
		/// Gets the guid of the character in the world (which is provided by this world server).
		const UInt64 getCharacterGuid() const { return m_character->getGuid(); }
		/// 
		std::shared_ptr<GameCharacter> getCharacter() { return m_character; }
		/// 
		WorldInstance &getWorldInstance() { return m_instance; }
		/// Starts the 20 second logout timer.
		void logoutRequest();
		/// Cancels the 20 second logout timer if it has been set up.
		void cancelLogoutRequest();
		/// Disconnects this player.
		void kick();
		/// 
		void chatMessage(game::ChatMsg type, game::Language lang, const String &receiver, const String &channel, const String &message);
		/// 
		const String &getRealmName() const { return m_realmConnector.getRealmName(); }
		/// 
		TileIndex2D getTileIndex() const;
		/// Gets the player characters fall information.
		void getFallInfo(UInt32 &out_time, float &out_z) { out_time = m_lastFallTime; out_z = m_lastFallZ; }
		/// Updates the player characters fall information.
		void setFallInfo(UInt32 time, float z);
		/// Returns true if a player character's guid is on the ignore list.
		/// @param guid The character guid to check.
		bool isIgnored(UInt64 guid) const override;
		/// Adds a player guid to the players ignore list.
		/// @param guid The player guid to add to the ignore list.
		void addIgnore(UInt64 guid);
		/// Removes a player guid from the players ignore list.
		/// @param guid The player guid to remove from the ignore list.
		void removeIgnore(UInt64 guid);
		/// Converts a timestamp value from another player to this players timestamp value.
		/// @param otherTimestamp The other players timestamp values.
		/// @param otherTick The other players tick count.
		UInt32 convertTimestamp(UInt32 otherTimestamp, UInt32 otherTick) const override;
		/// Updates the player characters group id.
		/// @param group The new group id or 0, if not in a group.
		void updateCharacterGroup(UInt64 group);

		void buyItemFromVendor(UInt64 vendorGuid, UInt32 itemEntry, UInt64 bagGuid, UInt8 slot, UInt8 count);
		
		void sendGossipMenu(UInt64 guid);
		/// Opens a loot window at the client, which will show the provided loot data.
		/// @param loot The loot instance which holds the actual loot data.
		/// @param source The game object which was the source of this loot window.
		void openLootDialog(std::shared_ptr<LootInstance> loot, GameObject &source);
		/// Called when the opened loot was cleared or the loot source (object, corpse, whatever) despawns or
		/// no longer provides this loot.
		void closeLootDialog();
		/// Determines whether the player is currently looting something.
		bool isLooting() const { return m_loot != nullptr; }
		/// Sends an proxy packet to the realm which will then be redirected to the game client.
		/// @param generator The packet writer function.
		template<class F>
		void sendProxyPacket(F generator) const
		{
			std::vector<char> buffer;
			io::VectorSink sink(buffer);

			typename game::Protocol::OutgoingPacket packet(sink);
			generator(packet);

			// Send the proxy packet to the realm server
			m_realmConnector.sendProxyPacket(m_characterId, packet.getOpCode(), packet.getSize(), buffer);
		}
		/// Sends a proxy packet to all tile subscribers near the player character.
		/// @param generator The packet writer function.
		template<class F>
		void broadcastProxyPacket(F generator)
		{
			// Get tile index
			TileIndex2D tile = getTileIndex();

			// Get all subscribers
			forEachTileInSight(
				m_instance.getGrid(),
				tile,
				[&generator](VisibilityTile &tile)
			{
				for (auto * const subscriber : tile.getWatchers().getElements())
				{
					std::vector<char> buffer;
					io::VectorSink sink(buffer);

					typename game::Protocol::OutgoingPacket packet(sink);
					generator(packet);

					subscriber->sendPacket(packet, buffer);
				}
			});
		}
		/// Saves the characters data.
		void saveCharacterData() const;
		/// 
		void sendTradeStatus(TradeStatus status, UInt64 guid = 0, UInt32 errorCode = 0, UInt32 itemCategory = 0);
		/// Determines if there is a pending logout request.
		bool isLogoutPending() const { return m_logoutCountdown.running; }
		/// Determines if this player is currently trading.
		bool isTrading() const { return (m_tradeData.get() != nullptr); }
		/// Tries to initiate a trade with the target, if possible.
		void initiateTrade(UInt64 target);
		/// Tries to cancel the current trade (if any).
		void cancelTrade();

		void setTradeSession(std::shared_ptr<TradeData> data);

	private:

		void updatePlayerTime(bool resetLevelTime = false);

	public:

		/// @copydoc ITileSubscriber::getControlledObject()
		GameCharacter *getControlledObject() override { return m_character.get(); }
		/// @copydoc ITileSubscriber::sendPacket()
		void sendPacket(game::Protocol::OutgoingPacket &packet, const std::vector<char> &buffer) override;

		// Network packet handlers (implemented in separate cpp files like player_XXX_handler.cpp)

		void handleAutoStoreLootItem(game::Protocol::IncomingPacket &packet);
		void handleAutoEquipItem(game::Protocol::IncomingPacket &packet);
		void handleAutoStoreBagItem(game::Protocol::IncomingPacket &packet);
		void handleSwapItem(game::Protocol::IncomingPacket &packet);
		void handleSwapInvItem(game::Protocol::IncomingPacket &packet);
		void handleSplitItem(game::Protocol::IncomingPacket &packet);
		void handleAutoEquipItemSlot(game::Protocol::IncomingPacket &packet);
		void handleDestroyItem(game::Protocol::IncomingPacket &packet);
		void handleRepopRequest(game::Protocol::IncomingPacket &packet);
		void handleLoot(game::Protocol::IncomingPacket &packet);
		void handleLootMoney(game::Protocol::IncomingPacket &packet);
		void handleLootRelease(game::Protocol::IncomingPacket &packet);
		void handleMovementCode(game::Protocol::IncomingPacket &packet, UInt16 opCode);
		void handleTimeSyncResponse(game::Protocol::IncomingPacket &packet);
		void handleLearnTalent(game::Protocol::IncomingPacket &packet);
		void handleUseItem(game::Protocol::IncomingPacket &packet);
		void handleListInventory(game::Protocol::IncomingPacket &packet);
		void handleBuyItem(game::Protocol::IncomingPacket &packet);
		void handleBuyItemInSlot(game::Protocol::IncomingPacket &packet);
		void handleSellItem(game::Protocol::IncomingPacket &packet);
		void handleGossipHello(game::Protocol::IncomingPacket &packet);
		void handleTrainerBuySpell(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverStatusQuery(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverStatusMultipleQuery(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverHello(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverQueryQuest(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverQuestAutolaunch(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverAcceptQuest(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverCompleteQuest(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverRequestReward(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverChooseReward(game::Protocol::IncomingPacket &packet);
		void handleQuestgiverCancel(game::Protocol::IncomingPacket &packet);
		void handleQuestlogRemoveQuest(game::Protocol::IncomingPacket &packet);
		void handleGameObjectUse(game::Protocol::IncomingPacket &packet);
		void handleOpenItem(game::Protocol::IncomingPacket &packet);
		void handleMoveTimeSkipped(game::Protocol::IncomingPacket &packet);
		void handleInitateTrade(game::Protocol::IncomingPacket &packet);
		void handleBeginTrade(game::Protocol::IncomingPacket &packet);
		void handleAcceptTrade(game::Protocol::IncomingPacket &packet);
		void handleUnacceptTrade(game::Protocol::IncomingPacket &packet);
		void handleCancelTrade(game::Protocol::IncomingPacket &packet);
		void handleSetTradeGold(game::Protocol::IncomingPacket &packet);
		void handleSetTradeItem(game::Protocol::IncomingPacket &packet);
		void handleClearTradeItem(game::Protocol::IncomingPacket &packet);
		void handleSetActionBarToggles(game::Protocol::IncomingPacket &packet);
		void handleToggleHelm(game::Protocol::IncomingPacket &packet);
		void handleToggleCloak(game::Protocol::IncomingPacket &packet);
		void handleMailSend(game::Protocol::IncomingPacket &packet);
		void handleMailGetList(game::Protocol::IncomingPacket &packet);
		void handleMailMarkAsRead(game::Protocol::IncomingPacket &packet);
		void handleResurrectResponse(game::Protocol::IncomingPacket &packet);
		void handleCancelChanneling(game::Protocol::IncomingPacket &packet);
		void handlePlayedTime(game::Protocol::IncomingPacket &packet);
		void handleAckCode(game::Protocol::IncomingPacket &packet, UInt16 opCode);
		void handleZoneUpdate(game::Protocol::IncomingPacket &packet);
		void handleRepairItem(game::Protocol::IncomingPacket &packet);

	private:

		/// 
		void onLogout();
		/// Executed when the player character spawned.
		void onSpawn();
		/// Executed when the player character despawned.
		void onDespawn(GameObject &/*despawning*/);
		/// Executed when an auto attack error occurred.
		void onAttackSwingError(AttackSwingError error);
		/// Executed when the player character will move from one grid tile to another one.
		void onTileChangePending(VisibilityTile &oldTile, VisibilityTile &newTile);
		/// Executed when a proficiency of the player character changed.
		void onProficiencyChanged(Int32 itemClass, UInt32 mask);
		/// An inventory error occurred.
		void onInventoryChangeFailure(game::InventoryChangeFailure failure, GameItem *itemA, GameItem *itemB);
		/// Executed when the combo points of the character changed.
		void onComboPointsChanged();
		/// Executed when the character gained experience points.
		void onExperienceGained(UInt64 victimGUID, UInt32 baseXP, UInt32 restXP);
		/// Executed when a spell cast error occurred.
		void onSpellCastError(const proto::SpellEntry &spell, game::SpellCastResult result);
		/// Executed when the character leveled up.
		void onLevelGained(UInt32 previousLevel, Int32 healthDiff, Int32 manaDiff, Int32 statDiff0, Int32 statDiff1, Int32 statDiff2, Int32 statDiff3, Int32 statDiff4);
		/// Executed when an aura of our character was updated.
		void onAuraUpdated(UInt8 slot, UInt32 spellId, Int32 duration, Int32 maxDuration);
		/// Executed when an aura of our character was updated.
		void onTargetAuraUpdated(UInt64 target, UInt8 slot, UInt32 spellId, Int32 duration, Int32 maxDuration);
		/// Executed when the character should be teleported.
		void onTeleport(UInt16 map, math::Vector3 location, float o);
		/// Executed when an item instance was created.
		void onItemCreated(std::shared_ptr<GameItem> item, UInt16 slot);
		/// Executed when an item instance was udpated.
		void onItemUpdated(std::shared_ptr<GameItem> item, UInt16 slot);
		/// Executed when an item instance was destroyed.
		void onItemDestroyed(std::shared_ptr<GameItem> item, UInt16 slot);
		/// Executed when a new spell was learned.
		void onSpellLearned(const proto::SpellEntry &spell);
		/// Executed when a resurrect is being requested.
		void onResurrectRequest(UInt64 objectGUID, const String &sentName, UInt8 typeId);
		/// Executed when the next client sync should be requested.
		void onClientSync();

	private:

		PlayerManager &m_manager;
		RealmConnector &m_realmConnector;
		WorldInstanceManager &m_worldInstanceManager;
		DatabaseId m_characterId;
		std::shared_ptr<GameCharacter> m_character;
		Countdown m_logoutCountdown;
		WorldInstance &m_instance;
		simple::scoped_connection_container m_characterSignals;
		simple::scoped_connection m_onLootInvalidate;
		simple::scoped_connection m_onAtkSwingErr, m_onProfChanged, m_onInvFailure;
		simple::scoped_connection m_onComboPoints, m_onXP, m_onCastError, m_onGainLevel;
		simple::scoped_connection m_onAuraUpdate, m_onTargetAuraUpdate, m_onTeleport, m_standStateChanged;
		simple::scoped_connection m_onUnitStateUpdate, m_onCooldownEvent, m_questChanged, m_questKill;
		simple::scoped_connection m_itemCreated, m_itemUpdated, m_itemDestroyed, m_objectInteraction;
		simple::scoped_connection m_onLootCleared, m_onLootInspect, m_spellModChanged;
		simple::scoped_connection m_onSpellLearned, m_onItemAdded, m_onResurrectRequest;
		simple::scoped_connection_container m_lootSignals;
		AttackSwingError m_lastError;
		UInt32 m_lastFallTime;
		float m_lastFallZ;
		proto::Project &m_project;
		std::shared_ptr<LootInstance> m_loot;
		GameObject *m_lootSource;
		LinearSet<UInt64> m_ignoredGUIDs;
		Countdown m_groupUpdate;
		std::shared_ptr<TradeData> m_tradeData;
		GameTime m_lastPlayTimeUpdate;
		UInt32 m_clientSync, m_serverSync;
		Countdown m_nextClientSync;
		UInt32 m_timeSyncCounter;
		GameTime m_lastTimeSync;
	};
}
