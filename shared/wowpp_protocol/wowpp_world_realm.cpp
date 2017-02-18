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
#include "wowpp_world_realm.h"
#include "game/game_item.h"
#include "shared/proto_data/items.pb.h"

namespace wowpp
{
	namespace pp
	{
		namespace world_realm
		{
			namespace world_write
			{
				void login(pp::OutgoingPacket &out_packet, const std::vector<UInt32> &mapIds, const std::vector<UInt32> &instanceIds)
				{
					out_packet.start(world_packet::Login);
					out_packet
					        << io::write<NetUInt32>(ProtocolVersion)
					        << io::write_dynamic_range<NetUInt8>(mapIds)
					        << io::write_dynamic_range<NetUInt8>(instanceIds);
					out_packet.finish();
				}

				void keepAlive(pp::OutgoingPacket &out_packet)
				{
					out_packet.start(world_packet::KeepAlive);
					out_packet.finish();
				}


				void worldInstanceEntered(pp::OutgoingPacket &out_packet, DatabaseId requesterDbId, UInt64 worldObjectGuid, UInt32 instanceId, UInt32 mapId, UInt32 zoneId, math::Vector3 location, float o)
				{
					out_packet.start(world_packet::WorldInstanceEntered);
					out_packet
					        << io::write<NetDatabaseId>(requesterDbId)
					        << io::write<NetUInt64>(worldObjectGuid)
					        << io::write<NetUInt32>(instanceId)
					        << io::write<NetUInt32>(mapId)
					        << io::write<NetUInt32>(zoneId)
					        << io::write<float>(location.x)
					        << io::write<float>(location.y)
					        << io::write<float>(location.z)
					        << io::write<float>(o);
					out_packet.finish();
				}

				void worldInstanceLeft(
				    pp::OutgoingPacket &out_packet,
				    DatabaseId requesterDbId,
				    WorldLeftReason reason
				)
				{
					out_packet.start(world_packet::WorldInstanceLeft);
					out_packet
					        << io::write<NetDatabaseId>(requesterDbId)
					        << io::write<NetUInt32>(reason);
					out_packet.finish();
				}

				void worldInstanceError(pp::OutgoingPacket &out_packet, DatabaseId requesterDbId, world_instance_error::Type error)
				{
					out_packet.start(world_packet::WorldInstanceError);
					out_packet
					        << io::write<NetDatabaseId>(requesterDbId)
					        << io::write<NetUInt8>(error);
					out_packet.finish();
				}

				void clientProxyPacket(pp::OutgoingPacket &out_packet, DatabaseId characterId, UInt16 opCode, UInt32 size, const std::vector<char> &packetBuffer)
				{
					out_packet.start(world_packet::ClientProxyPacket);
					out_packet
					        << io::write<NetDatabaseId>(characterId)
					        << io::write<NetUInt16>(opCode)
					        << io::write<NetUInt32>(size)
					        << io::write_dynamic_range<NetUInt32>(packetBuffer);
					out_packet.finish();
				}

				void characterData(pp::OutgoingPacket &out_packet, UInt64 characterId, const GameCharacter &character)
				{
					out_packet.start(world_packet::CharacterData);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << character
					        << io::write<NetUInt32>(character.getSpells().size());
					for (const auto &spell : character.getSpells())
					{
						out_packet
						        << io::write<NetUInt32>(spell->id());
					}
					out_packet.finish();
				}

				void teleportRequest(pp::OutgoingPacket &out_packet, UInt64 characterId, UInt32 map, math::Vector3 location, float o)
				{
					out_packet.start(world_packet::TeleportRequest);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << io::write<NetUInt32>(map)
					        << io::write<float>(location.x)
					        << io::write<float>(location.y)
					        << io::write<float>(location.z)
					        << io::write<float>(o);
					out_packet.finish();
				}

				void characterGroupUpdate(pp::OutgoingPacket &out_packet, UInt64 characterId, const std::vector<UInt64> &nearbyMembers, UInt32 health, UInt32 maxHealth, UInt8 powerType, UInt32 power, UInt32 maxPower, UInt8 level, UInt32 map, UInt32 zone, math::Vector3 location, const std::vector<UInt32> &auras)
				{
					out_packet.start(world_packet::CharacterGroupUpdate);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << io::write_dynamic_range<UInt8>(nearbyMembers)
					        << io::write<NetUInt32>(health)
					        << io::write<NetUInt32>(maxHealth)
					        << io::write<NetUInt8>(powerType)
					        << io::write<NetUInt32>(power)
					        << io::write<NetUInt32>(maxPower)
					        << io::write<NetUInt8>(level)
					        << io::write<NetUInt32>(map)
					        << io::write<NetUInt32>(zone)
					        << io::write<float>(location.x)
					        << io::write<float>(location.y)
					        << io::write<float>(location.z)
					        << io::write_dynamic_range<UInt8>(auras);
					out_packet.finish();
				}
				void questUpdate(pp::OutgoingPacket &out_packet, UInt64 characterId, UInt32 questId, const QuestStatusData &data)
				{
					out_packet.start(world_packet::QuestUpdate);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << io::write<NetUInt32>(questId)
					        << io::write<NetUInt32>(data.status)
					        << io::write<NetUInt64>(data.expiration)
					        << io::write<NetUInt8>(data.explored)
					        << io::write_range(data.creatures)
					        << io::write_range(data.objects)
					        << io::write_range(data.items);
					out_packet.finish();
				}
				void characterSpawned(pp::OutgoingPacket & out_packet, UInt64 characterId)
				{
					out_packet.start(world_packet::CharacterSpawned);
					out_packet
						<< io::write<NetUInt64>(characterId);
					out_packet.finish();
				}
				void mailDraft(pp::OutgoingPacket & out_packet, String sender, String receiver, String subject, String body, UInt32 money, UInt32 COD, UInt32 cost, const std::vector<std::shared_ptr<GameItem>>& items)
				{
					out_packet.start(world_packet::MailDraft);
					out_packet
						<< io::write_range(sender)
						<< io::write_range(receiver)
						<< io::write_range(subject)
						<< io::write_range(body)
						<< io::write<NetUInt32>(money)
						<< io::write<NetUInt32>(COD)
						<< io::write<NetUInt32>(cost)
						//<< io::write_dynamic_range<std::shared_ptr<GameItem>>(items)
						;
					out_packet.finish();
				}
			}

			namespace realm_write
			{
				void loginAnswer(pp::OutgoingPacket &out_packet, LoginResult result, const String &realmName)
				{
					out_packet.start(realm_packet::LoginAnswer);
					out_packet
					        << io::write<NetUInt32>(ProtocolVersion)
					        << io::write<NetUInt8>(result);

					if (result == login_result::Success)
					{
						out_packet
						        << io::write_dynamic_range<NetUInt8>(realmName);
					}

					out_packet.finish();
				}

				void characterLogIn(pp::OutgoingPacket &out_packet, DatabaseId characterRealmId, UInt32 instanceId, const GameCharacter &character)
				{
					out_packet.start(realm_packet::CharacterLogIn);
					out_packet
					        << io::write<NetDatabaseId>(characterRealmId)
					        << io::write<NetUInt32>(instanceId)
					        << character;
					out_packet.finish();
				}

				void clientProxyPacket(pp::OutgoingPacket &out_packet, DatabaseId characterId, UInt16 opCode, UInt32 size, const std::vector<char> &packetBuffer)
				{
					out_packet.start(realm_packet::ClientProxyPacket);
					out_packet
					        << io::write<NetDatabaseId>(characterId)
					        << io::write<NetUInt16>(opCode)
					        << io::write<NetUInt32>(size)
					        << io::write_dynamic_range<NetUInt32>(packetBuffer);
					out_packet.finish();
				}

				void chatMessage(pp::OutgoingPacket &out_packet, UInt64 characterGuid, game::ChatMsg type, game::Language language, const String &receiver, const String &channel, const String &message)
				{
					out_packet.start(realm_packet::ChatMessage);
					out_packet
					        << io::write<NetUInt64>(characterGuid)
					        << io::write<NetUInt32>(type)
					        << io::write<NetUInt32>(language)
					        << io::write_dynamic_range<NetUInt8>(receiver)
					        << io::write_dynamic_range<NetUInt8>(channel)
					        << io::write_dynamic_range<NetUInt16>(message);
					out_packet.finish();
				}

				void leaveWorldInstance(pp::OutgoingPacket &out_packet, DatabaseId characterRealmId, WorldLeftReason reason)
				{
					out_packet.start(realm_packet::LeaveWorldInstance);
					out_packet
					        << io::write<NetUInt64>(characterRealmId)
					        << io::write<NetUInt32>(reason);
					out_packet.finish();
				}

				void characterGroupChanged(pp::OutgoingPacket &out_packet, UInt64 characterId, UInt64 groupId)
				{
					out_packet.start(realm_packet::CharacterGroupChanged);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << io::write<NetUInt64>(groupId);
					out_packet.finish();
				}

				void ignoreList(pp::OutgoingPacket &out_packet, UInt64 characterId, const std::vector<UInt64> &list)
				{
					out_packet.start(realm_packet::IgnoreList);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << io::write_dynamic_range<NetUInt8>(list);
					out_packet.finish();
				}
				void addIgnore(pp::OutgoingPacket &out_packet, UInt64 characterId, UInt64 ignoreGuid)
				{
					out_packet.start(realm_packet::AddIgnore);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << io::write<NetUInt64>(ignoreGuid);
					out_packet.finish();
				}
				void removeIgnore(pp::OutgoingPacket &out_packet, UInt64 characterId, UInt64 removeGuid)
				{
					out_packet.start(realm_packet::RemoveIgnore);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << io::write<NetUInt64>(removeGuid);
					out_packet.finish();
				}
				void itemData(pp::OutgoingPacket &out_packet, UInt64 characterId, const std::vector<ItemData> &data)
				{
					out_packet.start(realm_packet::ItemData);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << io::write<NetUInt32>(data.size());

					for (const auto &item : data)
					{
						out_packet
						        << item;
					}
					out_packet.finish();
				}
				void itemsRemoved(pp::OutgoingPacket &out_packet, UInt64 characterId, const std::vector<UInt16> &data)
				{
					out_packet.start(realm_packet::ItemsRemoved);
					out_packet
					        << io::write<NetUInt64>(characterId)
					        << io::write_dynamic_range<NetUInt32>(data);
					out_packet.finish();
				}
				void spellLearned(pp::OutgoingPacket & out_packet, UInt64 characterId, UInt32 spellId)
				{
					out_packet.start(realm_packet::SpellLearned);
					out_packet
						<< io::write<NetUInt64>(characterId)
						<< io::write<NetUInt32>(spellId);
					out_packet.finish();
				}
			}

			namespace world_read
			{
				bool login(io::Reader &packet, UInt32 &out_protocol, std::vector<UInt32> &out_mapIds, std::vector<UInt32> &out_instanceIds)
				{
					return packet
					       >> io::read<NetUInt32>(out_protocol)
					       >> io::read_container<NetUInt8>(out_mapIds)
					       >> io::read_container<NetUInt8>(out_instanceIds);
				}

				bool keepAlive(io::Reader &packet)
				{
					return packet;
				}

				bool worldInstanceEntered(io::Reader &packet, DatabaseId &out_requesterDbId, UInt64 &out_worldObjectGuid, UInt32 &out_instanceId, UInt32 &out_mapId, UInt32 &out_zoneId, math::Vector3 &out, float &out_o)
				{
					return packet
					       >> io::read<NetDatabaseId>(out_requesterDbId)
					       >> io::read<NetUInt64>(out_worldObjectGuid)
					       >> io::read<NetUInt32>(out_instanceId)
					       >> io::read<NetUInt32>(out_mapId)
					       >> io::read<NetUInt32>(out_zoneId)
					       >> io::read<float>(out.x)
					       >> io::read<float>(out.y)
					       >> io::read<float>(out.z)
					       >> io::read<float>(out_o);
				}

				bool worldInstanceLeft(
				    io::Reader &packet,
				    DatabaseId &out_requesterDbId,
				    WorldLeftReason &out_reason
				)
				{
					return packet
					       >> io::read<NetDatabaseId>(out_requesterDbId)
					       >> io::read<NetUInt32>(out_reason);
				}

				bool worldInstanceError(io::Reader &packet, DatabaseId &out_requesterDbId, world_instance_error::Type &out_error)
				{
					return packet
					       >> io::read<NetDatabaseId>(out_requesterDbId)
					       >> io::read<NetUInt8>(out_error);
				}

				bool clientProxyPacket(io::Reader &packet, DatabaseId &out_characterId, UInt16 &out_opCode, UInt32 &out_size, std::vector<char> &out_packetBuffer)
				{
					return packet
					       >> io::read<NetDatabaseId>(out_characterId)
					       >> io::read<NetUInt16>(out_opCode)
					       >> io::read<NetUInt32>(out_size)
					       >> io::read_container<NetUInt32>(out_packetBuffer);
				}

				bool characterData(io::Reader &packet, UInt64 &out_characterId, GameCharacter &out_character, std::vector<UInt32> &out_spellIds)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterId)
					       >> out_character
					       >> io::read_container<NetUInt32>(out_spellIds)
					       ;
				}

				bool teleportRequest(io::Reader &packet, UInt64 &out_characterId, UInt32 &out_map, math::Vector3 &out, float &out_o)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterId)
					       >> io::read<NetUInt32>(out_map)
					       >> io::read<float>(out.x)
					       >> io::read<float>(out.y)
					       >> io::read<float>(out.z)
					       >> io::read<float>(out_o)
					       ;
				}

				bool characterGroupUpdate(io::Reader &packet, UInt64 &out_characterId, std::vector<UInt64> &out_nearbyMembers, UInt32 &out_health, UInt32 &out_maxHealth, UInt8 &out_powerType, UInt32 &out_power, UInt32 &out_maxPower, UInt8 &out_level, UInt32 &out_map, UInt32 &out_zone, math::Vector3 &out, std::vector<UInt32> &out_auras)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterId)
					       >> io::read_container<NetUInt8>(out_nearbyMembers)
					       >> io::read<NetUInt32>(out_health)
					       >> io::read<NetUInt32>(out_maxHealth)
					       >> io::read<NetUInt8>(out_powerType)
					       >> io::read<NetUInt32>(out_power)
					       >> io::read<NetUInt32>(out_maxPower)
					       >> io::read<NetUInt8>(out_level)
					       >> io::read<NetUInt32>(out_map)
					       >> io::read<NetUInt32>(out_zone)
					       >> io::read<float>(out.x)
					       >> io::read<float>(out.y)
					       >> io::read<float>(out.z)
					       >> io::read_container<NetUInt8>(out_auras)
					       ;
				}

				bool questUpdate(io::Reader &packet, UInt64 &out_characterId, UInt32 &out_questId, QuestStatusData &out_data)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterId)
					       >> io::read<NetUInt32>(out_questId)
					       >> io::read<NetUInt32>(out_data.status)
					       >> io::read<NetUInt64>(out_data.expiration)
					       >> io::read<NetUInt8>(out_data.explored)
					       >> io::read_range(out_data.creatures)
					       >> io::read_range(out_data.objects)
					       >> io::read_range(out_data.items);
				}

				bool characterSpawned(io::Reader & packet, UInt64 & out_characterId)
				{
					return packet
						>> io::read<NetUInt64>(out_characterId);
				}
				
				bool mailDraft(io::Reader & packet, UInt32 & out_unk1, UInt32 & out_unk2, String & out_sender, String & out_receiver, String & out_subject, String & out_body, UInt32 & out_money, UInt32 & out_COD, UInt32 & out_cost, const std::vector<std::shared_ptr<GameItem>>& out_items)
				{
					return packet
						>> io::read<NetUInt32>(out_unk1)
						>> io::read<NetUInt32>(out_unk2)
						>> io::read_string(out_sender)
						>> io::read_string(out_receiver)
						>> io::read_string(out_subject)
						>> io::read_string(out_body)
						>> io::read<NetUInt32>(out_money)
						>> io::read<NetUInt32>(out_COD)
						>> io::read<NetUInt32>(out_cost)
						//>> io::read_container<std::shared_ptr<GameItem>>(out_items)
						;
				}
			}

			namespace realm_read
			{
				bool loginAnswer(io::Reader &packet, UInt32 &out_protocol, LoginResult &out_result, String &out_realmName)
				{
					if (!(packet
					        >> io::read<NetUInt32>(out_protocol)
					        >> io::read<NetUInt8>(out_result)))
					{
						return false;
					}

					if (out_result == login_result::Success)
					{
						return packet
						       >> io::read_container<NetUInt8>(out_realmName);
					}

					return true;
				}

				bool characterLogIn(io::Reader &packet, DatabaseId &out_characterRealmId, UInt32 &out_instanceId, GameCharacter *out_character)
				{
					assert(out_character);

					return packet
					       >> io::read<NetDatabaseId>(out_characterRealmId)
					       >> io::read<NetUInt32>(out_instanceId)
					       >> *out_character;
				}

				bool clientProxyPacket(io::Reader &packet, DatabaseId &out_characterId, UInt16 &out_opCode, UInt32 &out_size, std::vector<char> &out_packetBuffer)
				{
					return packet
					       >> io::read<NetDatabaseId>(out_characterId)
					       >> io::read<NetUInt16>(out_opCode)
					       >> io::read<NetUInt32>(out_size)
					       >> io::read_container<NetUInt32>(out_packetBuffer);
				}

				bool chatMessage(io::Reader &packet, UInt64 &out_characterGuid, game::ChatMsg &out_type, game::Language &out_language, String &out_receiver, String &out_channel, String &out_message)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterGuid)
					       >> io::read<NetUInt32>(out_type)
					       >> io::read<NetUInt32>(out_language)
					       >> io::read_container<NetUInt8>(out_receiver)
					       >> io::read_container<NetUInt8>(out_channel)
					       >> io::read_container<NetUInt16>(out_message);
				}

				bool leaveWorldInstance(io::Reader &packet, DatabaseId &out_characterRealmId, WorldLeftReason &out_reason)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterRealmId)
					       >> io::read<NetUInt32>(out_reason);
				}

				bool characterGroupChanged(io::Reader &packet, UInt64 &out_characterId, UInt64 &out_groupId)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterId)
					       >> io::read<NetUInt64>(out_groupId);
				}

				bool ignoreList(io::Reader &packet, UInt64 &out_characterId, std::vector<UInt64> &out_list)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterId)
					       >> io::read_container<NetUInt8>(out_list);
				}
				bool addIgnore(io::Reader &packet, UInt64 &out_characterId, UInt64 &out_ignoreGuid)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterId)
					       >> io::read<NetUInt64>(out_ignoreGuid);
				}
				bool removeIgnore(io::Reader &packet, UInt64 &out_characterId, UInt64 &out_removeGuid)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterId)
					       >> io::read<NetUInt64>(out_removeGuid);
				}

				bool itemData(io::Reader &packet, UInt64 &out_characterId, std::vector<ItemData> &out_data)
				{
					UInt32 count = 0;
					if (!(packet
					        >> io::read<NetUInt64>(out_characterId)
					        >> io::read<NetUInt32>(count)))
					{
						return false;
					}

					out_data.resize(count);
					for (UInt32 i = 0; i < count; ++i)
					{
						ItemData &data = out_data[i];
						if (!(packet
						        >> data))
						{
							return false;
						}
					}

					return packet;
				}

				bool itemsRemoved(io::Reader &packet, UInt64 &out_characterId, std::vector<UInt16> &out_slots)
				{
					return packet
					       >> io::read<NetUInt64>(out_characterId)
					       >> io::read_container<NetUInt32>(out_slots);
				}

				bool spellLearned(io::Reader & packet, UInt64 & out_characterId, UInt32 & out_spellId)
				{
					return packet
						>> io::read<NetUInt64>(out_characterId)
						>> io::read<NetUInt32>(out_spellId);
				}
			}
		}
	}
}
