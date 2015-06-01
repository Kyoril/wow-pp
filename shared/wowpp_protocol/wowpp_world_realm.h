//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

namespace wowpp
{
	namespace pp
	{
		namespace world_realm
		{
			static const UInt32 ProtocolVersion = 0x01;

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
					/// Original packet which the realm should encode and send to the client.
					ClientProxyPacket,
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
					float x, 
					float y, 
					float z, 
					float o
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
			}

			/// Contains methods for writing packets from the realm server.
			namespace realm_write
			{
				/// Contains an answer of the realm whether this world server will be of any use.
				/// @param out_packet Packet buffer where the data will be written to.
				/// @param result The result of the login attempt of the world server.
				void loginAnswer(
					pp::OutgoingPacket &out_packet,
					LoginResult result
					);

				/// 
				void characterLogIn(
					pp::OutgoingPacket &out_packet,
					DatabaseId characterRealmId,
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
					float &out_x,
					float &out_y,
					float &out_z,
					float &out_o
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
					LoginResult &out_result
					);

				/// 
				bool characterLogIn(
					io::Reader &packet,
					DatabaseId &out_characterRealmId,
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
			}
		}
	}
}
