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

#include "common/typedefs.h"
#include "common/big_number.h"
#include "wowpp_protocol.h"
#include "binary_io/reader.h"
#include "game/game_character.h"
#include "game/defines.h"
#include "game/mail.h"
 
namespace wowpp
{
	namespace pp
	{
		namespace world_realm
		{
			static const UInt32 ProtocolVersion = 0x18;

			namespace world_instance_error
			{
				enum Type
				{
					/// There are too many open instances already.
					TooManyInstances,
					/// This server can not create an instance of the requested map.
					UnsupportedMap,
					/// Something went wrong.
					InternalError
				};
			}

			namespace leave_instance_reason
			{
				enum Type
				{
					/// The connection to the game client was lost by the realm.
					ConnectionLost,
					/// The player wants to enter a new world instance instead.
					Transfer,
					/// The player logged out.
					Logout
				};
			}

			typedef leave_instance_reason::Type LeaveInstanceReason;

			namespace world_packet
			{
				enum Type
				{
					/// Sent by the world to log in at the realm server.
					Login,
					/// Ping message to keep the connection between the realm and the login server alive.
					KeepAlive,
					/// Sent by the world server if a player character successfully entered a world instance.
					WorldInstanceEntered,
					/// Sent by the world server if a world instance is unavailable.
					WorldInstanceError,
					/// Sent by the world server if a player character left a world instance.
					WorldInstanceLeft,
					/// Original packet which the realm should encode and send to the client.
					ClientProxyPacket,
					/// Character data from a world node which should be saved by the realm.
					CharacterData,
					/// A character should be teleported to another world.
					TeleportRequest,
					/// Update packet for a character who is a group member.
					CharacterGroupUpdate,
					/// Update packet for characters quest data.
					QuestUpdate,
					/// Sent by the world server if a player character successfully spawned in a world instance.
					CharacterSpawned,
					/// Sent by the world server when a player sends a mail.
					MailDraft,
					/// Sent by the world server when a player requests its mails.
					MailGetList,
					/// Sent by the world server when a player marks a mail as read.
					MailMarkAsRead
				};
			}

			typedef world_packet::Type WorldPacket;

			namespace realm_packet
			{
				enum Type
				{
					/// Result of the realm login answer.
					LoginAnswer,
					/// Player character wants to login to the world server
					CharacterLogIn,
					/// Original packet which was received by the game client and is redirected to the
					/// world server.
					ClientProxyPacket,
					/// Chat message of a player which needs to be handled by the world server (yell-msg for example).
					ChatMessage,
					/// Notifies the world node that a player should leave the world node (for example on disconnect)
					LeaveWorldInstance,
					/// Notifies the world node about a group change of a character.
					CharacterGroupChanged,
					/// Send complete Ignorelist
					IgnoreList,
					/// Add entry to Ignorelist
					AddIgnore,
					/// Remove entry to Ignorelist
					RemoveIgnore,
					/// Received one or more item data entries for creation and/or updates. Maybe this packet becomes obsoelete.
					ItemData,
					/// One or more items have been deleted. Only slots are specified here.
					ItemsRemoved,
					/// The player character has learned a new spell. This is currently only sent using the WebAPI.
					SpellLearned,
					/// Sent by the realm when money needs to be changed on a character.
					MoneyChange,
					/// Sent by the realm when items need to taken from a mail.
					ItemsMailTake
				};
			}

			typedef realm_packet::Type RealmPacket;

			namespace login_result
			{
				enum Type
				{
					/// World could successfully log in.
					Success,
					/// The realm server does know one of the maps this world server supports.
					UnknownMap,
					/// All map ids of this world server are already handled by another world server.
					MapsAlreadyInUse,
					/// Something went wrong at the realm server...
					InternalError
				};
			}

			typedef login_result::Type LoginResult;

			namespace world_left_reason
			{
				enum Type
				{
					/// The player wants to log out and return to the character screen.
					Logout,
					/// The player was teleported away and thus needs to be transferred to another world instance.
					Teleport,
					/// The player disconnected.
					Disconnect,
					/// Unknown reason (placeholder for more reasons...)
					Unknown
				};
			}

			typedef world_left_reason::Type WorldLeftReason;

			/// Contains methods for writing packets from the world server.
			namespace world_write
			{
				/// Packet used to log in at the realm server.
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param mapIds A list of supported map ids.
				/// @param instanceIds A list of running instance ids.
				void login(
				    pp::OutgoingPacket &out_packet,
				    const std::vector<UInt32> &mapIds,
				    const std::vector<UInt32> &instanceIds
				);

				/// A simple empty packet which is used to keep the connection between the world server
				/// and the realm server alive.
				/// @param out_packet Packet buffer where the data will be written to.
				void keepAlive(
				    pp::OutgoingPacket &out_packet
				);

				///
				void worldInstanceEntered(
				    pp::OutgoingPacket &out_packet,
				    DatabaseId requesterDbId,
				    UInt64 worldObjectGuid,
				    UInt32 instanceId,
				    UInt32 mapId,
				    UInt32 zoneId,
				    math::Vector3 location,
				    float o
				);

				///
				void worldInstanceLeft(
				    pp::OutgoingPacket &out_packet,
				    DatabaseId requesterDbId,
				    WorldLeftReason reason
				);

				///
				void worldInstanceError(
				    pp::OutgoingPacket &out_packet,
				    DatabaseId requesterDbId,
				    world_instance_error::Type error
				);

				///
				void clientProxyPacket(
				    pp::OutgoingPacket &out_packet,
				    DatabaseId characterId,
				    UInt16 opCode,
				    UInt32 size,
				    const std::vector<char> &packetBuffer
				);

				///
				void characterData(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    const GameCharacter &character
				);

				///
				void teleportRequest(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    UInt32 map,
				    math::Vector3 location,
				    float o
				);

				///
				void characterGroupUpdate(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    const std::vector<UInt64> &nearbyMembers,
				    UInt32 health,
				    UInt32 maxHealth,
				    UInt8 powerType,
				    UInt32 power,
				    UInt32 maxPower,
				    UInt8 level,
				    UInt32 map,
				    UInt32 zone,
				    math::Vector3 location,
				    const std::vector<UInt32> &auras
				);

				void questUpdate(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    UInt32 questId,
				    const QuestStatusData &data
				);

				void characterSpawned(
					pp::OutgoingPacket &out_packet,
					UInt64 characterId
				);

				void mailDraft(
					pp::OutgoingPacket &out_packet,
					Mail mail,
					String receiver,
					std::vector<UInt16> itemsSlots
				);

				void mailGetList(
					pp::OutgoingPacket &out_packet,
					DatabaseId characterId
				);

				void mailMarkAsRead(
					pp::OutgoingPacket &out_packet,
					DatabaseId characterId,
					UInt32 mailId
				);
			}

			/// Contains methods for writing packets from the realm server.
			namespace realm_write
			{
				/// Contains an answer of the realm whether this world server will be of any use.
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param result The result of the login attempt of the world server.
				void loginAnswer(
				    pp::OutgoingPacket &out_packet,
				    LoginResult result,
				    const String &realmName
				);

				///
				void characterLogIn(
				    pp::OutgoingPacket &out_packet,
				    DatabaseId characterRealmId,
				    UInt32 instanceId,
				    const GameCharacter &character
				);

				///
				void clientProxyPacket(
				    pp::OutgoingPacket &out_packet,
				    DatabaseId characterId,
				    UInt16 opCode,
				    UInt32 size,
				    const std::vector<char> &packetBuffer
				);

				///
				void chatMessage(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterGuid,
				    game::ChatMsg type,
				    game::Language language,
				    const String &receiver,
				    const String &channel,
				    const String &message
				);

				///
				void leaveWorldInstance(
				    pp::OutgoingPacket &out_packet,
				    DatabaseId characterRealmId,
				    WorldLeftReason reason
				);

				///
				void characterGroupChanged(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    UInt64 groupId
				);

				void ignoreList(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    const std::vector<UInt64> &list
				);
				void addIgnore(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    UInt64 ignoreGuid
				);
				void removeIgnore(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    UInt64 removeGuid
				);
				void itemData(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    const std::vector<ItemData> &data
				);
				void itemsRemoved(
				    pp::OutgoingPacket &out_packet,
				    UInt64 characterId,
				    const std::vector<UInt16> &data,
					const std::vector<UInt32> &stackCount
				);
				void spellLearned(
					pp::OutgoingPacket &out_packet,
					UInt64 characterId,
					UInt32 spellId
				);
				void moneyChange(
					pp::OutgoingPacket &out_packet,
					UInt64 characterId,
					UInt32 money,
					bool remove
				);
				void takeItemMail(
					pp::OutgoingPacket &out_packet,
					UInt64 characterId,
					UInt32 mailId,
					ItemData &item
				);
			}

			/// Contains methods for reading packets coming from the world server.
			namespace world_read
			{
				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @param out_protocol WoW++ network protocol version used by the world server.
				/// @param out_mapIds List of map identifiers which the world server supports.
				/// @param out_instanceIds List of map instances running on this world server.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool login(
				    io::Reader &packet,
				    UInt32 &out_protocol,
				    std::vector<UInt32> &out_mapIds,
				    std::vector<UInt32> &out_instanceIds
				);

				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool keepAlive(
				    io::Reader &packet
				);

				///
				bool worldInstanceEntered(
				    io::Reader &packet,
				    DatabaseId &out_requesterDbId,
				    UInt64 &out_worldObjectGuid,
				    UInt32 &out_instanceId,
				    UInt32 &out_mapId,
				    UInt32 &out_zoneId,
				    math::Vector3 &out,
				    float &out_o
				);

				///
				bool worldInstanceLeft(
				    io::Reader &packet,
				    DatabaseId &out_requesterDbId,
				    WorldLeftReason &out_reason
				);

				///
				bool worldInstanceError(
				    io::Reader &packet,
				    DatabaseId &out_requesterDbId,
				    world_instance_error::Type &out_error
				);

				///
				bool clientProxyPacket(
				    io::Reader &packet,
				    DatabaseId &out_characterId,
				    UInt16 &out_opCode,
				    UInt32 &out_size,
				    std::vector<char> &out_packetBuffer
				);

				///
				bool characterData(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    GameCharacter &out_character,
				    std::vector<UInt32> &out_spellIds
				);

				///
				bool teleportRequest(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    UInt32 &out_map,
				    math::Vector3 &out,
				    float &out_o
				);

				///
				bool characterGroupUpdate(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    std::vector<UInt64> &out_nearbyMembers,
				    UInt32 &out_health,
				    UInt32 &out_maxHealth,
				    UInt8 &out_powerType,
				    UInt32 &out_power,
				    UInt32 &out_maxPower,
				    UInt8 &out_level,
				    UInt32 &out_map,
				    UInt32 &out_zone,
				    math::Vector3 &out,
				    std::vector<UInt32> &out_auras
				);

				bool questUpdate(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    UInt32 &out_questId,
				    QuestStatusData &out_data
				);

				bool characterSpawned(
					io::Reader &packet,
					UInt64 &out_characterId
				);

				bool mailDraft(
					io::Reader &packet,
					Mail &out_mail,
					String &out_receiver,
					std::vector<UInt16> &out_itemsSlots
				);	

				bool mailGetList(
					io::Reader &packet,
					DatabaseId &out_characterId
				);

				bool mailMarkAsRead(
					io::Reader &packet,
					DatabaseId &out_characterId,
					UInt32 &out_mailId
				);
			}

			/// Contains methods for reading packets coming from the realm server.
			namespace realm_read
			{
				/// TODO: ADD DESCRIPTION
				/// @param packet Packet buffer where the data will be read from.
				/// @param out_result Result of the login attempt.
				/// @param out_protocol Protocol version used by the login server.
				/// @returns false if the packet has not enough data or if there was an error
				/// reading the packet's content.
				bool loginAnswer(
				    io::Reader &packet,
				    UInt32 &out_protocol,
				    LoginResult &out_result,
				    String &out_realmName
				);

				///
				bool characterLogIn(
				    io::Reader &packet,
				    DatabaseId &out_characterRealmId,
				    UInt32 &out_instanceId,
				    GameCharacter *out_character
				);

				///
				bool clientProxyPacket(
				    io::Reader &packet,
				    DatabaseId &out_characterId,
				    UInt16 &out_opCode,
				    UInt32 &out_size,
				    std::vector<char> &out_packetBuffer
				);

				///
				bool chatMessage(
				    io::Reader &packet,
				    UInt64 &out_characterGuid,
				    game::ChatMsg &out_type,
				    game::Language &out_language,
				    String &out_receiver,
				    String &out_channel,
				    String &out_message
				);

				///
				bool leaveWorldInstance(
				    io::Reader &packet,
				    DatabaseId &out_characterRealmId,
				    WorldLeftReason &out_reason
				);

				///
				bool characterGroupChanged(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    UInt64 &out_groupId
				);

				bool ignoreList(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    std::vector<UInt64> &out_list
				);
				bool addIgnore(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    UInt64 &out_ignoreGuid
				);
				bool removeIgnore(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    UInt64 &out_removeGuid
				);
				bool itemData(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    std::vector<ItemData> &out_data
				);
				bool itemsRemoved(
				    io::Reader &packet,
				    UInt64 &out_characterId,
				    std::vector<UInt16> &out_slots,
					std::vector<UInt32> &out_stackCount
				);
				bool spellLearned(
					io::Reader &packet,
					UInt64 &out_characterId,
					UInt32 &out_spellId
				);
				bool moneyChange(
					io::Reader &packet,
					UInt64 &out_characterId,
					UInt32 &out_money,
					bool &out_remove
				);
				bool takeItemMail(
					io::Reader &packet,
					UInt64 &out_characterId,
					UInt32 &out_mailId,
					ItemData &out_item
				);
			}
		}
	}
}
