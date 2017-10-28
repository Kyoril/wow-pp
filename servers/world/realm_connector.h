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

#include "common/constants.h"
#include "network/connector.h"
#include "game_protocol/game_protocol.h"
#include "game_protocol/game_incoming_packet.h"
#include "wowpp_protocol/wowpp_connector.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "game/game_character.h"
#include "common/timer_queue.h"
#include "game/mail.h"

namespace wowpp
{
	// Forwards
	class WorldInstanceManager;
	class WorldInstance;
	class PlayerManager;
	struct Configuration;
	class Player;
	namespace proto
	{
		class Project;
	}

	/// This class manages the connection to the realm server.
	class RealmConnector final
		: public pp::IConnectorListener
	{
	private:

		RealmConnector(const RealmConnector &Other) = delete;
		RealmConnector &operator=(const RealmConnector &Other) = delete;

	public:

		simple::signal<void (RealmConnector &connector, DatabaseId characterId, std::shared_ptr<GameCharacter> character, WorldInstance &instance)> worldInstanceEntered;

	public:

		/// 
		/// @param ioService
		/// @param worldInstanceManager
		/// @param config
		/// @param project
		/// @param timer
		explicit RealmConnector(
			boost::asio::io_service &ioService,
			WorldInstanceManager &worldInstanceManager,
			PlayerManager &playerManager,
			const Configuration &config,
			UInt32 realmEntryIndex,
			proto::Project &project,
			TimerQueue &timer);
		~RealmConnector();

		/// @copydoc wowpp::pp::IConnectorListener::connectionLost()
		virtual void connectionLost() override;
		/// @copydoc wowpp::pp::IConnectorListener::connectionMalformedPacket()
		virtual void connectionMalformedPacket() override;
		/// @copydoc wowpp::pp::IConnectorListener::connectionPacketReceived()
		virtual PacketParseResult connectionPacketReceived(pp::Protocol::IncomingPacket &packet) override;
		/// @copydoc wowpp::pp::IConnectorListener::connectionEstablished()
		virtual bool connectionEstablished(bool success) override;

		/// 
		/// @param senderId
		/// @param opCode
		/// @param size
		/// @param buffer
		void sendProxyPacket(DatabaseId senderId, UInt16 opCode, UInt32 size, const std::vector<char> &buffer);
		/// 
		void sendTeleportRequest(DatabaseId characterId, UInt32 map, math::Vector3 location, float o);
		/// 
		void notifyWorldInstanceLeft(DatabaseId characterId, pp::world_realm::WorldLeftReason reason);
		/// 
		const String &getRealmName() const { return m_realmName; }
		/// Sends data of one character to the realm.
		void sendCharacterData(GameCharacter &character);
		/// Sends a group update to the realm.
		void sendCharacterGroupUpdate(GameCharacter &character, const std::vector<UInt64> &nearbyMembers);
		/// 
		void sendQuestData(DatabaseId characterId, UInt32 quest, const QuestStatusData &data);
		/// 
		void sendCharacterSpawnNotification(UInt64 characterId);
		///
		void sendMailDraft(Mail mail, String &receiver);
		///
		void sendMailGetList(DatabaseId characterId);
		///
		void sendMailMarkAsRead(DatabaseId characterId, UInt32 mailId);


	private:

		/// 
		void scheduleConnect();
		/// 
		void tryConnect();
		/// 
		void scheduleKeepAlive();
		/// 
		void onScheduledKeepAlive();

		// Realm packet handlers
		void handleLoginAnswer(pp::Protocol::IncomingPacket &packet);
		void handleCharacterLogin(pp::Protocol::IncomingPacket &packet);
		void handleProxyPacket(pp::Protocol::IncomingPacket &packet);
		void handleChatMessage(pp::Protocol::IncomingPacket &packet);
		void handleLeaveWorldInstance(pp::Protocol::IncomingPacket &packet);
		void handleCharacterGroupChanged(pp::Protocol::IncomingPacket &packet);
		void handleIgnoreList(pp::Protocol::IncomingPacket &packet);
		void handleAddIgnore(pp::Protocol::IncomingPacket &packet);
		void handleRemoveIgnore(pp::Protocol::IncomingPacket &packet);
		void handleItemData(pp::Protocol::IncomingPacket &packet);
		void handleSpellLearned(pp::Protocol::IncomingPacket &packet);
		void handleMoneyChange(pp::Protocol::IncomingPacket &packet);

	private:

		// Proxy packet handlers
		void handleNameQuery(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleLogoutRequest(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleLogoutCancel(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleSetSelection(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleStandStateChange(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleCastSpell(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleCancelCast(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleAttackSwing(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleAttackStop(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleSetSheathed(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleAreaTrigger(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleCancelAura(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleEmote(Player &sender, game::Protocol::IncomingPacket &packet);
		void handleTextEmote(Player &sender, game::Protocol::IncomingPacket &packet);
		void handlePetNameQuery(Player &sender, game::Protocol::IncomingPacket &packet);

	private:

		// Variables
		boost::asio::io_service &m_ioService;
		WorldInstanceManager &m_worldInstanceManager;
		PlayerManager &m_playerManager;
		const Configuration &m_config;
		proto::Project &m_project;
		TimerQueue &m_timer;
		std::shared_ptr<pp::Connector> m_connection;
		UInt32 m_realmEntryIndex;
		String m_realmName;
	};
}
