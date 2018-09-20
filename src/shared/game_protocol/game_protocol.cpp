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
#include "common/endian_convert.h"
#include "common/clock.h"
#include "common/macros.h"
#include "game_protocol.h"
#include "game/game_item.h"
#include "game/loot_instance.h"
#include "log/default_log_levels.h"
#include "binary_io/stream_source.h"
#include "binary_io/stream_sink.h"
#include "binary_io/reader.h"
#include "binary_io/writer.h"
#include "binary_io/vector_sink.h"
#include "game/game_character.h"
#include "proto_data/project.h"

namespace wowpp
{
	namespace game
	{
		namespace server_write
		{
			void invalidatePlayer(game::OutgoingPacket & out_packet, UInt64 guid)
			{
				out_packet.start(game::server_packet::InvalidatePlayer);
				out_packet
					<< io::write<NetUInt64>(guid);
				out_packet.finish();
			}

			void gmTicketSystemStatus(game::OutgoingPacket & out_packet, bool enable)
			{
				out_packet.start(game::server_packet::GmTicketSystemStatus);
				out_packet
					<< io::write<NetUInt32>(enable);
				out_packet.finish();
			}

			void triggerCinematic(game::OutgoingPacket &out_packet, UInt32 cinematicId)
			{
				out_packet.start(game::server_packet::TriggerCinematic);
				out_packet
				        << io::write<NetUInt32>(cinematicId);
				out_packet.finish();
			}

			void authChallenge(game::OutgoingPacket &out_packet, UInt32 seed)
			{
				out_packet.start(game::server_packet::AuthChallenge);
				out_packet
				        << io::write<NetUInt32>(seed);
				out_packet.finish();
			}

			void pong(game::OutgoingPacket &out_packet, UInt32 ping)
			{
				out_packet.start(game::server_packet::Pong);
				out_packet
				        << io::write<NetUInt32>(ping);
				out_packet.finish();
			}

			void addonInfo(game::OutgoingPacket &out_packet, const AddonEntries &addons)
			{
				// Vector initialization
				const std::vector<unsigned char> tdata
				{
					0xC3, 0x5B, 0x50, 0x84, 0xB9, 0x3E, 0x32, 0x42, 0x8C, 0xD0, 0xC7, 0x48, 0xFA, 0x0E, 0x5D, 0x54,
					0x5A, 0xA3, 0x0E, 0x14, 0xBA, 0x9E, 0x0D, 0xB9, 0x5D, 0x8B, 0xEE, 0xB6, 0x84, 0x93, 0x45, 0x75,
					0xFF, 0x31, 0xFE, 0x2F, 0x64, 0x3F, 0x3D, 0x6D, 0x07, 0xD9, 0x44, 0x9B, 0x40, 0x85, 0x59, 0x34,
					0x4E, 0x10, 0xE1, 0xE7, 0x43, 0x69, 0xEF, 0x7C, 0x16, 0xFC, 0xB4, 0xED, 0x1B, 0x95, 0x28, 0xA8,
					0x23, 0x76, 0x51, 0x31, 0x57, 0x30, 0x2B, 0x79, 0x08, 0x50, 0x10, 0x1C, 0x4A, 0x1A, 0x2C, 0xC8,
					0x8B, 0x8F, 0x05, 0x2D, 0x22, 0x3D, 0xDB, 0x5A, 0x24, 0x7A, 0x0F, 0x13, 0x50, 0x37, 0x8F, 0x5A,
					0xCC, 0x9E, 0x04, 0x44, 0x0E, 0x87, 0x01, 0xD4, 0xA3, 0x15, 0x94, 0x16, 0x34, 0xC6, 0xC2, 0xC3,
					0xFB, 0x49, 0xFE, 0xE1, 0xF9, 0xDA, 0x8C, 0x50, 0x3C, 0xBE, 0x2C, 0xBB, 0x57, 0xED, 0x46, 0xB9,
					0xAD, 0x8B, 0xC6, 0xDF, 0x0E, 0xD6, 0x0F, 0xBE, 0x80, 0xB3, 0x8B, 0x1E, 0x77, 0xCF, 0xAD, 0x22,
					0xCF, 0xB7, 0x4B, 0xCF, 0xFB, 0xF0, 0x6B, 0x11, 0x45, 0x2D, 0x7A, 0x81, 0x18, 0xF2, 0x92, 0x7E,
					0x98, 0x56, 0x5D, 0x5E, 0x69, 0x72, 0x0A, 0x0D, 0x03, 0x0A, 0x85, 0xA2, 0x85, 0x9C, 0xCB, 0xFB,
					0x56, 0x6E, 0x8F, 0x44, 0xBB, 0x8F, 0x02, 0x22, 0x68, 0x63, 0x97, 0xBC, 0x85, 0xBA, 0xA8, 0xF7,
					0xB5, 0x40, 0x68, 0x3C, 0x77, 0x86, 0x6F, 0x4B, 0xD7, 0x88, 0xCA, 0x8A, 0xD7, 0xCE, 0x36, 0xF0,
					0x45, 0x6E, 0xD5, 0x64, 0x79, 0x0F, 0x17, 0xFC, 0x64, 0xDD, 0x10, 0x6F, 0xF3, 0xF5, 0xE0, 0xA6,
					0xC3, 0xFB, 0x1B, 0x8C, 0x29, 0xEF, 0x8E, 0xE5, 0x34, 0xCB, 0xD1, 0x2A, 0xCE, 0x79, 0xC3, 0x9A,
					0x0D, 0x36, 0xEA, 0x01, 0xE0, 0xAA, 0x91, 0x20, 0x54, 0xF0, 0x72, 0xD8, 0x1E, 0xC7, 0x89, 0xD2
				};

				out_packet.start(game::server_packet::AddonInfo);
				for (auto &addon : addons)
				{
					UInt8 unk1 = 1;
					out_packet
					        << io::write<NetUInt8>(0x02)
					        << io::write<NetUInt8>(unk1);

					if (unk1)
					{
						UInt8 unk2 = (addon.crc != 0x1c776d01);		// 0x1c776d01 = crc used by blizzards standard addons
						out_packet
						        << io::write<NetUInt8>(unk2);
						if (unk2)
						{
							out_packet
							        << io::write_range(tdata.begin(), tdata.end());
						}

						out_packet
						        << io::write<NetUInt32>(0x00);
					}

					UInt8 unk3 = 0;
					out_packet
					        << io::write<NetUInt8>(unk3);
				}
				out_packet.finish();
			}

			void authResponse(game::OutgoingPacket &out_packet, ResponseCode code, Expansions expansion)
			{
				out_packet.start(game::server_packet::AuthResponse);
				out_packet
				        << io::write<NetUInt8>(code);

				if (code == response_code::AuthOk)
				{
					out_packet
					        << io::write<NetUInt32>(0x00)		// BillingTimeRemaining in seconds
					        << io::write<NetUInt8>(0x00)		// BillingPlanFlags
					        << io::write<NetUInt32>(0x00)		// BillingTimeRested in seconds
					        << io::write<NetUInt8>(expansion);	// 0 = Classic, 1 = TBC
				}

				out_packet.finish();
			}

			void charEnum(game::OutgoingPacket &out_packet, const game::CharEntries &characters)
			{
				out_packet.start(game::server_packet::CharEnum);

				out_packet
				        << io::write<NetUInt8>(characters.size());

				for (const game::CharEntry &entry : characters)
				{
					out_packet
					        << io::write<NetUInt64>(entry.id)
					        << io::write_range(entry.name.begin(), entry.name.end())
					        << io::write<NetUInt8>(0x00)	// 0-terminated c-style string
					        << io::write<NetUInt8>(entry.race)
					        << io::write<NetUInt8>(entry.class_)
					        << io::write<NetUInt8>(entry.gender)
					        << io::write<NetUInt8>(entry.skin)
					        << io::write<NetUInt8>(entry.face)
					        << io::write<NetUInt8>(entry.hairStyle)
					        << io::write<NetUInt8>(entry.hairColor)
					        << io::write<NetUInt8>(entry.facialHair)
					        << io::write<NetUInt8>(entry.level)
					        << io::write<NetUInt32>(entry.zoneId)				// zone
					        << io::write<NetUInt32>(entry.mapId)				// map
					        << io::write<float>(entry.location.x)				// x
					        << io::write<float>(entry.location.y)				// y
					        << io::write<float>(entry.location.z)				// z
					        << io::write<NetUInt32>(0x00);						// guild guid

					UInt32 charFlags = UInt32(entry.flags);
					if (entry.atLogin & atlogin_flags::Rename)
					{
						charFlags |= character_flags::Rename;
					}
					//UInt32 playerFlags = 0;
					//UInt32 atLoginFlags = 0x20;

					out_packet
					        << io::write<NetUInt32>(charFlags)
					        << io::write<NetUInt8>(1);			// only 1 if atLoginFlags == 0x20

					// Pets info
					{
						UInt32 petDisplayId = 0;
						UInt32 petLevel = 0;
						UInt32 petFamily = 0;

						out_packet
						        << io::write<NetUInt32>(petDisplayId)
						        << io::write<NetUInt32>(petLevel)
						        << io::write<NetUInt32>(petFamily);
					}

					// Equipment
					{
						for (size_t i = 0; i < 19; ++i)	// 19 = max equipment slots
						{
							auto it = entry.equipment.find(i);
							if (it == entry.equipment.end())
							{
								out_packet
								        << io::write<NetUInt32>(0)
								        << io::write<NetUInt8>(0)
								        << io::write<NetUInt32>(0);
							}
							else
							{
								out_packet
								        << io::write<NetUInt32>(it->second->displayid())
								        << io::write<NetUInt8>(it->second->inventorytype())
								        << io::write<NetUInt32>(0);
							}
						}

						out_packet
						        << io::write<NetUInt32>(0x00)	// first bag display id
						        << io::write<NetUInt8>(0x00)	// first bag inventory type
						        << io::write<NetUInt32>(0);		// enchant?
					}
				}

				out_packet.finish();
			}

			void charCreate(game::OutgoingPacket &out_packet, ResponseCode code)
			{
				out_packet.start(game::server_packet::CharCreate);
				out_packet
				        << io::write<NetUInt8>(code);
				out_packet.finish();
			}

			void charDelete(game::OutgoingPacket &out_packet, ResponseCode code)
			{
				out_packet.start(game::server_packet::CharDelete);
				out_packet
				        << io::write<NetUInt8>(code);
				out_packet.finish();
			}

			void charLoginFailed(game::OutgoingPacket &out_packet, ResponseCode code)
			{
				// Write packet
				out_packet.start(server_packet::CharacterLoginFailed);
				out_packet
				        << io::write<NetUInt8>(code);
				out_packet.finish();
			}

			void accountDataTimes(game::OutgoingPacket &out_packet, const std::array<UInt32, 32> &times)
			{
				out_packet.start(game::server_packet::AccountDataTimes);
				out_packet
				        << io::write_range(times);
				out_packet.finish();
			}

			void initWorldStates(game::OutgoingPacket &out_packet, UInt32 mapId, UInt32 zoneId /*TODO */)
			{
				const std::vector<UInt8> data =
				{
					0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x57, 0x00, 0x00, 0x00, 0x06, 0x00, 0xD8, 0x08,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD7, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD6, 0x08,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD5, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD4, 0x08,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD3, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
				};

				out_packet.start(game::server_packet::InitWorldStates);
				out_packet
				        << io::write_range(data);
				/*<< io::write<NetUInt32>(mapId)
				<< io::write<NetUInt32>(zoneId)
				<< io::write<NetUInt16>(0x00);*/
				out_packet.finish();
			}

			void loginVerifyWorld(game::OutgoingPacket &out_packet, UInt32 mapId, math::Vector3 location, float o)
			{
				out_packet.start(game::server_packet::LoginVerifyWorld);
				out_packet
				        << io::write<NetUInt32>(mapId)
				        << io::write<float>(location.x)
				        << io::write<float>(location.y)
				        << io::write<float>(location.z)
				        << io::write<float>(o);
				out_packet.finish();
			}

			void loginSetTimeSpeed(game::OutgoingPacket &out_packet, GameTime time_)
			{
				//TODO
				time_t secs = ::time(nullptr);
				tm *lt = localtime(&secs);

				// Convert
				UInt32 convertedTime =
				    static_cast<UInt32>((lt->tm_year - 100) << 24 | lt->tm_mon << 20 | (lt->tm_mday - 1) << 14 | lt->tm_wday << 11 | lt->tm_hour << 6 | lt->tm_min);

				out_packet.start(game::server_packet::LoginSetTimeSpeed);
				out_packet
				        << io::write<UInt32>(convertedTime)
				        << io::write<float>(1.0f / 60.0f);	// Update frequency is at 60 ticks / second
				out_packet.finish();
			}

			void tutorialFlags(game::OutgoingPacket &out_packet, const std::array<UInt32, 8> &tutorialData)
			{
				out_packet.start(game::server_packet::TutorialFlags);
				out_packet
				        << io::write_range(tutorialData);
				out_packet.finish();
			}

			void bindPointUpdate(game::OutgoingPacket &out_packet, UInt32 mapId, UInt32 areaId, const math::Vector3 &location)
			{
				out_packet.start(game::server_packet::BindPointUpdate);
				out_packet
				        << io::write<float>(location.x)
				        << io::write<float>(location.y)
				        << io::write<float>(location.z)
				        << io::write<UInt32>(mapId)
				        << io::write<UInt32>(areaId);
				out_packet.finish();
			}

			void setRestStart(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::SetRestStart);
				out_packet
				        << io::write<NetUInt32>(0x00);		// Unknown
				out_packet.finish();
			}

			void initializeFactions(game::OutgoingPacket &out_packet, const GameCharacter &character)
			{
				const UInt32 factionCount = 128;
				out_packet.start(game::server_packet::InitializeFactions);
				out_packet
				        << io::write<NetUInt32>(factionCount);

				// Build faction rep list (TODO: Build this statically somewhere)
				std::map<UInt32, UInt32> factionRepList;
				for (const auto &faction : character.getProject().factions.getTemplates().entry())
				{
					if (faction.replistid() >= 0)
						factionRepList[faction.replistid()] = faction.id();
				}

				// For each faction...
				for (UInt32 i = 0; i < factionCount; ++i)
				{
					auto it = factionRepList.find(i);
					if (it == factionRepList.end())
					{
						out_packet
							<< io::write<NetUInt8>(0x00)
							<< io::write<NetUInt32>(0x00000000);
					}
					else
					{
						// Get flags and reputation
						game::FactionFlags flags = character.getBaseFlags(it->second);
						Int32 repValue = character.getBaseReputation(it->second);
						out_packet
							<< io::write<NetUInt8>(flags)
							<< io::write<NetUInt32>(repValue);
					}
				}

				out_packet.finish();
			}

			void initialSpells(game::OutgoingPacket &out_packet, const proto::Project &project, const std::vector<const proto::SpellEntry *> &spells, const GameUnit::CooldownMap &cooldowns)
			{
				out_packet.start(game::server_packet::InitialSpells);

				out_packet
				        << io::write<NetUInt8>(0x00);

				const UInt16 spellCount = spells.size();
				out_packet
				        << io::write<NetUInt16>(spellCount);

				for (UInt16 i = 0; i < spellCount; ++i)
				{
					out_packet
					        << io::write<NetUInt16>(spells[i]->id())
					        << io::write<NetUInt16>(0x00);				// On Cooldown?
				}

				const UInt16 spellCooldowns = cooldowns.size();
				out_packet
				        << io::write<NetUInt16>(spellCooldowns);
				for (const auto &cooldown : cooldowns)
				{
					out_packet
					        << io::write<NetUInt16>(cooldown.first);
					const auto *spell = project.spells.getById(cooldown.first);
					if (!spell)
					{
						out_packet
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0);
					}
					else
					{
						out_packet
						        << io::write<NetUInt16>(0)
						        << io::write<NetUInt16>(spell->category());

						const GameTime time = getCurrentTime();
						const UInt32 remaining = (cooldown.second > time ? cooldown.second - time : 0);
						if (spell->category())
						{
							out_packet
							        << io::write<NetUInt32>(0)
							        << io::write<NetUInt32>(remaining);
						}
						else
						{
							out_packet
							        << io::write<NetUInt32>(remaining)
							        << io::write<NetUInt32>(0);
						}
					}
				}

				out_packet.finish();
			}

			void updateObject(game::OutgoingPacket &out_packet, const std::vector<std::vector<char>> &blocks)
			{
				out_packet.start(game::server_packet::UpdateObject);

				// Write blocks
				out_packet
				        << io::write<NetUInt32>(blocks.size())
				        << io::write<NetUInt8>(0x00);			// hasTransport = false

				// Append blocks uncompressed
				for (auto &block : blocks)
				{
					out_packet
					        << io::write_range(block);
				}

				out_packet.finish();
			}

			void unlearnSpells(game::OutgoingPacket &out_packet /*TODO */)
			{
				out_packet.start(game::server_packet::UnlearnSpells);
				out_packet
				        << io::write<NetUInt32>(0x00);	// Spell count

				out_packet.finish();
			}

			void motd(
			    game::OutgoingPacket &out_packet,
			    const std::string &motd
			)
			{
				out_packet.start(game::server_packet::Motd);
				const UInt32 sizePos = out_packet.sink().position();
				out_packet
				        << io::write<NetUInt32>(0x00);	// Line count

				std::size_t lineCount = 0;
				std::stringstream strm(motd);
				std::string line;
				
				while (std::getline(strm, line))
				{
					out_packet
					        << io::write_range(line) << io::write<NetUInt8>(0);
					++lineCount;
				}

				// Write correct line count
				out_packet.writePOD(sizePos, static_cast<UInt32>(lineCount));
				out_packet.finish();
			}

			void featureSystemStatus(
			    game::OutgoingPacket &out_packet
			)
			{
				out_packet.start(game::server_packet::FeatureSystemStatus);
				out_packet
				        << io::write<NetUInt16>(0x02);
				out_packet.finish();
			}

			void setDungeonDifficulty(
			    game::OutgoingPacket &out_packet
			)
			{
				out_packet.start(game::server_packet::SetDungeonDifficulty);
				const std::vector<UInt8> data =
				{
					0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
				};

				out_packet
				        << io::write_range(data);
				out_packet.finish();
			}

			void compressedUpdateObject(game::OutgoingPacket &out_packet, const std::vector<std::vector<char>> &blocks)
			{
				// Create buffer
				std::stringstream strm;
				io::StreamSink sink(strm);
				io::Writer writer(sink);

				// Write blocks
				writer
				        << io::write<NetUInt32>(blocks.size())
				        << io::write<NetUInt8>(0x00);			// hasTransport = false

				// Append blocks uncompressed
				for (auto &block : blocks)
				{
					writer
					        << io::write_range(block);
				}

				// Get original size
				size_t origSize = writer.sink().position();

				// Compress using ZLib
				boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
				in.push(boost::iostreams::zlib_compressor(boost::iostreams::zlib::best_compression));
				in.push(strm);

				// Copy to output stream
				std::stringstream outStrm;
				boost::iostreams::copy(in, outStrm);
				outStrm.seekg(0, std::ios::beg);

				String buffer = outStrm.str();

				// Write packet
				out_packet.start(server_packet::CompressedUpdateObject);
				out_packet
				        << io::write<NetUInt32>(origSize)
				        << io::write_range(buffer);
				out_packet.finish();
			}

			void nameQueryResponse(game::OutgoingPacket &out_packet, UInt64 objectGuid, const String &name, const String &realmName, UInt32 raceId, UInt32 genderId, UInt32 classId)
			{
				out_packet.start(server_packet::NameQueryResponse);
				out_packet
				        << io::write<NetUInt64>(objectGuid)
				        << io::write_range(name) << io::write<NetUInt8>(0x00)	// Terminator
				        << io::write_range(realmName) << io::write<NetUInt8>(0x00)	// Terminator realm name
				        << io::write<NetUInt32>(raceId)
				        << io::write<NetUInt32>(genderId)
				        << io::write<NetUInt32>(classId)
				        << io::write<NetUInt8>(0x00);
				out_packet.finish();
			}

			void setProficiency(game::OutgoingPacket &out_packet, UInt8 itemClass, UInt32 itemSubclassMask)
			{
				out_packet.start(server_packet::SetProficiency);
				out_packet
				        << io::write<NetUInt8>(itemClass)
				        << io::write<NetUInt32>(itemSubclassMask);
				out_packet.finish();
			}

			void actionButtons(game::OutgoingPacket &out_packet, const ActionButtons &buttons)
			{
				const UInt8 maxButtons = 132;

				out_packet.start(server_packet::ActionButtons);

				// For each possible action button the player could have
				for (UInt8 button = 0; button < maxButtons; ++button)
				{
					auto btn = buttons.find(button);
					if (btn != buttons.end())
					{
						out_packet
						        << io::write<NetUInt16>(btn->second.action)
						        << io::write<NetUInt8>(btn->second.misc)
						        << io::write<NetUInt8>(btn->second.type);
					}
					else
					{
						out_packet
						        << io::write<NetUInt32>(0);
					}
				}

				out_packet.finish();
			}

			void contactList(game::OutgoingPacket &out_packet, const game::SocialInfoMap &contacts)
			{
				out_packet.start(server_packet::ContactList);
				out_packet
				        << io::write<NetUInt32>(game::Friend | game::Ignored | game::Muted)
				        << io::write<NetUInt32>(contacts.size());

				for (const auto &contact : contacts)
				{
					out_packet
					        << io::write<NetUInt64>(contact.first)
					        << io::write<NetUInt32>(contact.second.flags)
					        << io::write_range(contact.second.note) << io::write<NetUInt8>(0);

					if (contact.second.flags & game::Friend)
					{
						out_packet
						        << io::write<NetUInt8>(contact.second.status);
						if (contact.second.status)
						{
							out_packet
							        << io::write<NetUInt32>(contact.second.area)
							        << io::write<NetUInt32>(contact.second.level)
							        << io::write<NetUInt32>(contact.second.class_);
						}
					}
				}
				out_packet.finish();
			}

			void creatureQueryResponse(game::OutgoingPacket &out_packet, Int32 localeIndex, const proto::UnitEntry &unit)
			{
				const String &name = (unit.name_loc_size() >= localeIndex) ?
					unit.name_loc(localeIndex - 1) : unit.name();
				const String &subname = (unit.subname_loc_size() >= localeIndex) ?
					unit.subname_loc(localeIndex - 1) : unit.subname();

				out_packet.start(server_packet::CreatureQueryResponse);
				out_packet
				        << io::write<NetUInt32>(unit.id())
				        << io::write_range(name.empty() ? unit.name() : name) << io::write<NetUInt8>(0x00)	// Terminator: name
				        << io::write<NetUInt8>(0x00)	// Terminator: name2 (always empty)
				        << io::write<NetUInt8>(0x00)	// Terminator: name3 (always empty)
				        << io::write<NetUInt8>(0x00)	// Terminator: name4 (always empty)
				        << io::write_range(subname.empty() ? unit.subname() : subname) << io::write<NetUInt8>(0x00)	// Terminator: name4 (always empty)
				        << io::write<NetUInt8>(0x00)
				        << io::write<NetUInt32>(unit.creaturetypeflags())
				        << io::write<NetUInt32>(unit.type())
				        << io::write<NetUInt32>(unit.family())
				        << io::write<NetUInt32>(unit.rank())
				        << io::write<NetUInt32>(0x00)	// Unknown
				        << io::write<NetUInt32>(0x00)	//TODO: CreatureSpellData
				        << io::write<NetUInt32>(unit.malemodel())	//TODO
				        << io::write<NetUInt32>(unit.malemodel())	//TODO
				        << io::write<NetUInt32>(unit.malemodel())	//TODO
				        << io::write<NetUInt32>(unit.malemodel())	//TODO
				        << io::write<float>(1.0f)	//TODO
				        << io::write<float>(1.0f)	//TODO
				        << io::write<NetUInt8>(0x00)	//TODO Is Racial Leader
				        ;

				out_packet.finish();
			}

			void timeSyncReq(
			    game::OutgoingPacket &out_packet,
			    UInt32 counter
			)
			{
				out_packet.start(server_packet::TimeSyncReq);
				out_packet
				        << io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void monsterMove(
			    game::OutgoingPacket &out_packet,
			    UInt64 guid,
			    const math::Vector3 &oldPosition,
			    const std::vector<math::Vector3> &path,
			    UInt32 time
			)
			{
				ASSERT(!path.empty());
				ASSERT(path.size() >= 1);

				out_packet.start(server_packet::MonsterMove);
				out_packet
					<< io::write_packed_guid(guid)
				    << io::write<float>(oldPosition.x)
				    << io::write<float>(oldPosition.y)
				    << io::write<float>(oldPosition.z)
				    << io::write<NetUInt32>(getCurrentTime())
				    << io::write<NetUInt8>(0);
				// Movement flags
				out_packet
					<< io::write<NetUInt32>(256)
					<< io::write<NetUInt32>(time)
					<< io::write<NetUInt32>(path.size() - 1);	// Number of points between target and start, without start point
				// Write destination point (this counts as the first point)
				const auto &pt = path.back();
				out_packet
					<< io::write<float>(pt.x)
					<< io::write<float>(pt.y)
					<< io::write<float>(pt.z);
				// Write points in between (if any)
				if (path.size() > 1)
				{
					// all other points are relative to the center of the path
					const math::Vector3 mid = (oldPosition + pt) * 0.5f;
					for (UInt32 i = 1; i < path.size() - 1; ++i)
					{
						auto &p = path[i];
						UInt32 packed = 0;
						packed |= ((int)((mid.x - p.x) / 0.25f) & 0x7FF);
						packed |= ((int)((mid.y - p.y) / 0.25f) & 0x7FF) << 11;
						packed |= ((int)((mid.z - p.z) / 0.25f) & 0x3FF) << 22;
						out_packet
							<< io::write<NetUInt32>(packed);
					}
				}
				out_packet.finish();
			}

			void logoutResponse(
			    game::OutgoingPacket &out_packet,
			    bool success
			)
			{
				out_packet.start(server_packet::LogoutResponse);
				if (!success)
				{
					out_packet
					        << io::write<NetUInt8>(0x0C);
				}
				out_packet
				        << io::write<NetUInt32>(0)
				        << io::write<NetUInt8>(0);
				out_packet.finish();
			}

			void logoutCancelAck(
			    game::OutgoingPacket &out_packet
			)
			{
				out_packet.start(server_packet::LogoutCancelAck);
				out_packet.finish();
			}

			void logoutComplete(
			    game::OutgoingPacket &out_packet
			)
			{
				out_packet.start(server_packet::LogoutComplete);
				out_packet.finish();
			}

			void messageChat(game::OutgoingPacket &out_packet, ChatMsg type, Language language, const String &channelname, UInt64 targetGUID, const String &message, GameUnit *speaker)
			{
				out_packet.start(server_packet::MessageChat);
				out_packet
				        << io::write<NetUInt8>(type);

				// Language
				if ((type != chat_msg::Channel && type != chat_msg::Whisper) || language == language::Addon)
				{
					out_packet << io::write<NetUInt32>(language);
				}
				else
				{
					out_packet << io::write<NetUInt32>(language::Universal);
				}

				const String speakerName = (speaker ? speaker->getName() : "UNKNOWN");
				switch (type)
				{
				case chat_msg::MonsterSay:
				case chat_msg::MonsterParty:
				case chat_msg::MonsterYell:
				case chat_msg::MonsterWhisper:
				case chat_msg::MonsterEmote:
				case chat_msg::RaidBossWhisper:
				case chat_msg::RaidBossEmote:
					{
						ASSERT(speaker);
						out_packet
							<< io::write<NetUInt64>(speaker->getGuid())
							<< io::write<NetUInt32>(0x00)
							<< io::write<NetUInt32>(speakerName.size() + 1)
							<< io::write_range(speakerName) << io::write<NetUInt8>(0)
							<< io::write<NetUInt64>(targetGUID);				// listener guid
						if (targetGUID && !isPlayerGUID(targetGUID) && !isPetGUID(targetGUID))
						{
							out_packet
								<< io::write<NetUInt32>(channelname.size() + 1)
								<< io::write_range(channelname) << io::write<NetUInt8>(0);
						}
						out_packet
						    << io::write<NetUInt32>(message.size() + 1)
						    << io::write_range(message) << io::write<NetUInt8>(0)
						    << io::write<NetUInt8>(0);				// Chat-Tag always 0 since it's a creature which can't be AFK, DND etc.
						out_packet.finish();
						return;
					}
				default:
					out_packet
						<< io::write<NetUInt64>(targetGUID)
						<< io::write<NetUInt32>(0);

					if (type == chat_msg::Channel)
					{
						out_packet
							<< io::write_range(channelname) << io::write<NetUInt8>(0);
					}
					out_packet
						<< io::write<NetUInt64>(targetGUID)
						<< io::write<NetUInt32>(message.size() + 1)
						<< io::write_range(message) << io::write<NetUInt8>(0)
						<< io::write<NetUInt8>(0);			// Chat tag: 1: AFK  2: DND  4: GM
					out_packet.finish();
					return;
				}
			}

			void movePacket(game::OutgoingPacket &out_packet, UInt16 opCode, UInt64 guid, const MovementInfo &movement)
			{
				//TODO: Check given op-code
				out_packet.start(opCode);
				out_packet
				        << io::write_packed_guid(guid)
				        << movement;
				out_packet.finish();
			}

			void moveTeleport(game::OutgoingPacket & out_packet, UInt64 guid, const MovementInfo & movement, UInt32 ackIndex)
			{
				out_packet.start(game::server_packet::MoveTeleport);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(ackIndex)
					<< movement;
				out_packet.finish();
			}

			void moveTeleportAck(game::OutgoingPacket &out_packet, UInt64 guid, const MovementInfo &movement, UInt32 ackId)
			{
				out_packet.start(game::server_packet::MoveTeleportAck);
				out_packet
						<< io::write_packed_guid(guid)
				        << io::write<NetUInt32>(ackId)
				        << movement;
				out_packet.finish();
			}

			void destroyObject(game::OutgoingPacket &out_packet, UInt64 guid, bool death)
			{
				out_packet.start(game::server_packet::DestroyObject);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt8>(death ? 1 : 0);
				out_packet.finish();
			}

			void standStateUpdate(game::OutgoingPacket &out_packet, UnitStandState standState)
			{
				out_packet.start(game::server_packet::StandStateUpdate);
				out_packet
				        << io::write<NetUInt8>(standState);
				out_packet.finish();
			}

			void friendStatus(game::OutgoingPacket &out_packet, UInt64 guid, game::FriendResult result, const game::SocialInfo &info)
			{
				out_packet.start(game::server_packet::FriendStatus);
				out_packet
				        << io::write<NetUInt8>(result)
				        << io::write<NetUInt64>(guid);

				switch (result)
				{
				case game::friend_result::AddedOffline:
				case game::friend_result::AddedOnline:
					out_packet << io::write_range(info.note) << io::write<NetUInt8>(0);
					break;
				default:
					break;
				}

				switch (result)
				{
				case game::friend_result::AddedOnline:
				case game::friend_result::Online:
					out_packet
					        << io::write<NetUInt8>(info.status)
					        << io::write<NetUInt32>(info.area)
					        << io::write<NetUInt32>(info.level)
					        << io::write<NetUInt32>(info.class_);
					break;
				default:
					break;
				}

				out_packet.finish();
			}

			void chatPlayerNotFound(game::OutgoingPacket &out_packet, const String &name)
			{
				out_packet.start(game::server_packet::ChatPlayerNotFound);
				out_packet
				        << io::write_range(name) << io::write<NetUInt8>(0);
				out_packet.finish();
			}

			void minimapPing(game::OutgoingPacket & out_packet, UInt64 senderGuid, float x, float y)
			{
				out_packet.start(game::server_packet::MinimapPing);
				out_packet
					<< io::write<NetUInt64>(senderGuid)
					<< io::write<float>(x)
					<< io::write<float>(y);
				out_packet.finish();
			}

			void castFailed(game::OutgoingPacket &out_packet, game::SpellCastResult result, const proto::SpellEntry &spell, UInt8 castCount)
			{
				out_packet.start(game::server_packet::CastFailed);
				out_packet
						<< io::write<NetUInt32>(spell.id())
						<< io::write<NetUInt8>(result)
						<< io::write<NetUInt8>(castCount);
				switch (result)
				{
					case game::spell_cast_result::FailedPreventedByMechanic:
						out_packet
							<< io::write<NetUInt32>(spell.mechanic());
						break;
					case game::spell_cast_result::FailedRequiresSpellFocus:
						out_packet
							<< io::write<NetUInt32>(spell.focusobject());
						break;
					default:
						break;
				}

				// TODO: Send more informations based on the cast result code (which area is required etc.)
				out_packet.finish();
			}

			void spellStart(game::OutgoingPacket &out_packet, UInt64 casterGUID, UInt64 casterItemGUID, const proto::SpellEntry &spell, const SpellTargetMap &targets, game::SpellCastFlags castFlags, Int32 castTime, UInt8 castCount)
			{
				out_packet.start(game::server_packet::SpellStart);

				// Cast item GUID (or caster GUID if not caused by item)
				out_packet
					<< io::write_packed_guid(casterItemGUID)
					<< io::write_packed_guid(casterGUID)
				    << io::write<NetUInt32>(spell.id())
				    << io::write<NetUInt8>(castCount)
				    << io::write<NetUInt16>(castFlags)
				    << io::write<NetInt32>(castTime)
				    << targets;

				if (castFlags && game::spell_cast_flags::Ammo)
				{
					//TODO: Ammo ids
				}
				out_packet.finish();
			}

			void spellGo(game::OutgoingPacket &out_packet, UInt64 casterGUID, UInt64 casterItemGUID, const proto::SpellEntry &spell, const SpellTargetMap &targets, game::SpellCastFlags castFlags /*TODO: HitInformation */ /*TODO: AmmoInformation */)
			{
				out_packet.start(game::server_packet::SpellGo);
				out_packet
					<< io::write_packed_guid(casterItemGUID)
					<< io::write_packed_guid(casterGUID)
				    << io::write<NetUInt32>(spell.id())
				    << io::write<NetUInt16>(castFlags)
				    << io::write<NetUInt32>(mTimeStamp());

				// TODO: Hit information
				{
					UInt8 unitHitCount = (targets.hasUnitTarget() ? 1 : 0);
					UInt8 goHitCount = (targets.hasGOTarget() ? 1 : 0);
					out_packet
					        << io::write<NetUInt8>(unitHitCount + goHitCount);	// Hit count
					if (unitHitCount > 0)
					{
						out_packet
						        << io::write<NetUInt64>(targets.getUnitTarget());
					}
					if (goHitCount > 0)
					{
						out_packet
						        << io::write<NetUInt64>(targets.getGOTarget());
					}

					out_packet
					        << io::write<NetUInt8>(0);	// Miss count
				}

				out_packet
				        << targets;

				if (castFlags && game::spell_cast_flags::Ammo)
				{
					//TODO: Ammo ids
				}

				out_packet.finish();
			}

			void spellFailure(game::OutgoingPacket &out_packet, UInt64 casterGUID, UInt32 spellId, game::SpellCastResult result)
			{
				out_packet.start(game::server_packet::SpellFailure);
				out_packet
						<< io::write_packed_guid(casterGUID)
						<< io::write<NetUInt32>(spellId)
						<< io::write<NetUInt8>(result);
					
				out_packet.finish();
			}

			void spellFailedOther(game::OutgoingPacket &out_packet, UInt64 casterGUID, UInt32 spellId)
			{
				out_packet.start(game::server_packet::SpellFailedOther);
				out_packet
					<< io::write_packed_guid(casterGUID)
					<< io::write<NetUInt32>(spellId);
				out_packet.finish();
			}

			void spellCooldown(game::OutgoingPacket &out_packet, UInt64 targetGUID, UInt8 flags, const std::map<UInt32, UInt32> &spellCooldownTimesMS)
			{
				out_packet.start(game::server_packet::SpellCooldown);
				out_packet
				        << io::write<NetUInt64>(targetGUID)
				        << io::write<NetUInt8>(flags);
				for (const auto &pair : spellCooldownTimesMS)
				{
					out_packet
					        << io::write<NetUInt32>(pair.first)			// Spell ID
					        << io::write<NetUInt32>(pair.second);		// Cooldown time in milliseconds where 0 is equal to now (no cooldown)
				}
				out_packet.finish();
			}

			void cooldownEvent(game::OutgoingPacket &out_packet, UInt32 spellID, UInt64 objectGUID)
			{
				out_packet.start(game::server_packet::CooldownEvent);
				out_packet
				        << io::write<NetUInt32>(spellID)
				        << io::write<NetUInt64>(objectGUID);
				out_packet.finish();
			}

			void clearCooldown(game::OutgoingPacket &out_packet, UInt32 spellID, UInt64 targetGUID)
			{
				out_packet.start(game::server_packet::ClearCooldown);
				out_packet
				        << io::write<NetUInt32>(spellID)
				        << io::write<NetUInt64>(targetGUID);
				out_packet.finish();
			}

			void spellNonMeleeDamageLog(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt64 casterGuid, UInt32 spellID, UInt32 damage, UInt8 damageSchoolMask, UInt32 absorbedDamage, UInt32 resistedDamage, bool PhysicalDamage, UInt32 blockedDamage, bool criticalHit)
			{
				out_packet.start(game::server_packet::SpellNonMeleeDamageLog);
				out_packet
					<< io::write_packed_guid(targetGuid)
					<< io::write_packed_guid(casterGuid)
				    << io::write<NetUInt32>(spellID)
				    << io::write<NetUInt32>(damage - absorbedDamage - resistedDamage - blockedDamage)
				    << io::write<NetUInt8>(damageSchoolMask)
				    << io::write<NetUInt32>(absorbedDamage)
				    << io::write<NetUInt32>(resistedDamage)
				    << io::write<NetUInt8>(PhysicalDamage)
				    << io::write<NetUInt8>(0)
				    << io::write<NetUInt32>(blockedDamage)
				    << io::write<NetUInt32>(criticalHit ? 0x27 : 0x25)
				    << io::write<NetUInt8>(0);
				out_packet.finish();
			}

			void itemQuerySingleResponse(game::OutgoingPacket &out_packet, Int32 localeIndex, const proto::ItemEntry &item)
			{
				const String &name = (item.name_loc_size() >= localeIndex) ?
					item.name_loc(localeIndex - 1) : item.name();
				const String &desc = (item.description_loc_size() >= localeIndex) ?
					item.description_loc(localeIndex - 1) : item.description();

				out_packet.start(game::server_packet::ItemQuerySingleResponse);
				out_packet
				        << io::write<NetUInt32>(item.id())
				        << io::write<NetUInt32>(item.itemclass())
				        << io::write<NetUInt32>(item.subclass())
				        << io::write<NetUInt32>(-1)				// SoundClassOverride (TODO)
				        << io::write_range(name.empty() ? item.name() : name) << io::write<NetUInt8>(0)
				        << io::write<NetUInt8>(0)				// Second name?
				        << io::write<NetUInt8>(0)				// Third name?
				        << io::write<NetUInt8>(0)				// Fourth name?
				        << io::write<NetUInt32>(item.displayid())
				        << io::write<NetUInt32>(item.quality())
				        << io::write<NetUInt32>(item.flags())
				        << io::write<NetUInt32>(item.buyprice())
				        << io::write<NetUInt32>(item.sellprice())
				        << io::write<NetUInt32>(item.inventorytype())
				        << io::write<NetUInt32>(item.allowedclasses())
				        << io::write<NetUInt32>(item.allowedraces())
				        << io::write<NetUInt32>(item.itemlevel())
				        << io::write<NetUInt32>(item.requiredlevel())
				        << io::write<NetUInt32>(item.requiredskill())
				        << io::write<NetUInt32>(item.requiredskillrank())
				        << io::write<NetUInt32>(item.requiredspell())
				        << io::write<NetUInt32>(item.requiredhonorrank())
				        << io::write<NetUInt32>(item.requiredcityrank())
				        << io::write<NetUInt32>(item.requiredrep())
				        << io::write<NetUInt32>(item.requiredreprank())
				        << io::write<NetUInt32>(item.maxcount())
				        << io::write<NetUInt32>(item.maxstack())
				        << io::write<NetUInt32>(item.containerslots());
				for (int i = 0; i < 10; ++i)
				{
					if (i < item.stats_size())
					{
						out_packet
						        << io::write<NetUInt32>(item.stats(i).type())
						        << io::write<NetUInt32>(item.stats(i).value());
					}
					else
					{
						out_packet
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0);
					}
				}
				for (int i = 0; i < 5; ++i)
				{
					if (i < item.damage_size())
					{
						out_packet
						        << io::write<float>(item.damage(i).mindmg())
						        << io::write<float>(item.damage(i).maxdmg())
						        << io::write<NetUInt32>(item.damage(i).type());
					}
					else
					{
						out_packet
						        << io::write<float>(0.0f)
						        << io::write<float>(0.0f)
						        << io::write<NetUInt32>(0);
					}
				}
				out_packet
				        << io::write<NetUInt32>(item.armor())
				        << io::write<NetUInt32>(item.holyres())
				        << io::write<NetUInt32>(item.fireres())
				        << io::write<NetUInt32>(item.natureres())
				        << io::write<NetUInt32>(item.frostres())
				        << io::write<NetUInt32>(item.shadowres())
				        << io::write<NetUInt32>(item.arcaneres())
				        << io::write<NetUInt32>(item.delay())
				        << io::write<NetUInt32>(item.ammotype())
				        << io::write<float>(item.rangedrange())
				        ;
				for (int i = 0; i < 5; ++i)
				{
					if (i < item.spells_size())
					{
						const auto &spell = item.spells(i);
						out_packet
						        << io::write<NetUInt32>(spell.spell())
						        << io::write<NetUInt32>(spell.trigger())
						        << io::write<NetUInt32>(-::abs((int)spell.charges()))
						        ;
						const bool useDBData = spell.cooldown() >= 0 || spell.categorycooldown() >= 0;
						if (useDBData)
						{
							out_packet
							        << io::write<NetUInt32>(spell.cooldown())
							        << io::write<NetUInt32>(spell.category())
							        << io::write<NetUInt32>(spell.categorycooldown());
						}
						else
						{
							out_packet
							        << io::write<NetUInt32>(-1)						// TODO
							        << io::write<NetUInt32>(0)						// TODO
							        << io::write<NetUInt32>(-1);					// TODO
						}
					}
					else
					{
						out_packet
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(-1)
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(-1);
					}
				}
				out_packet
				        << io::write<NetUInt32>(item.bonding())
				        << io::write_range(desc.empty() ? item.description() : desc) << io::write<NetUInt8>(0)
				        << io::write<NetUInt32>(0)					// TODO: Page Text
				        << io::write<NetUInt32>(0)					// TODO: Language ID
				        << io::write<NetUInt32>(0)					// TODO: Page Material
				        << io::write<NetUInt32>(item.questentry())
				        << io::write<NetUInt32>(item.lockid())
				        << io::write<NetUInt32>(item.material())
				        << io::write<NetUInt32>(item.sheath())
				        << io::write<NetUInt32>(item.randomproperty())
				        << io::write<NetUInt32>(item.randomsuffix())
				        << io::write<NetUInt32>(item.block())
				        << io::write<NetUInt32>(item.itemset())
				        << io::write<NetUInt32>(item.durability())
				        << io::write<NetUInt32>(item.area())
				        << io::write<NetUInt32>(item.map())			// Added in 1.12.X
				        << io::write<NetUInt32>(item.bagfamily())
				        << io::write<NetUInt32>(item.totemcategory())
				        ;
				for (size_t i = 0; i < 3; ++i)
				{
					if (i >= item.sockets_size())
					{
						out_packet
							<< io::write<NetUInt32>(0)
							<< io::write<NetUInt32>(0);
					}
					else
					{
						const auto &socket = item.sockets(i);
						out_packet
							<< io::write<NetUInt32>(socket.color())
							<< io::write<NetUInt32>(socket.content());
					}
				}
				out_packet
				        << io::write<NetUInt32>(item.socketbonus())
				        << io::write<NetUInt32>(item.gemproperties())
				        << io::write<NetUInt32>(item.disenchantskillval())
				        << io::write<float>(0.0f)					// TODO: Armor Damage Modifier
				        << io::write<NetUInt32>(0);					// Added in 2.4.2.8209
				out_packet.finish();
			}

			void spellEnergizeLog(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt64 casterGuid, UInt32 spellID, UInt8 powerType, UInt32 amount)
			{
				out_packet.start(game::server_packet::SpellEnergizeLog);
				out_packet
					<< io::write_packed_guid(targetGuid)
					<< io::write_packed_guid(casterGuid)
				    << io::write<NetUInt32>(spellID)
				    << io::write<NetUInt32>(powerType)
				    << io::write<NetUInt32>(amount);
				out_packet.finish();
			}

			void attackStart(game::OutgoingPacket &out_packet, UInt64 attackerGUID, UInt64 attackedGUID)
			{
				out_packet.start(game::server_packet::AttackStart);
				out_packet
				        << io::write<NetUInt64>(attackerGUID)
				        << io::write<NetUInt64>(attackedGUID);
				out_packet.finish();
			}

			void attackStop(game::OutgoingPacket &out_packet, UInt64 attackerGUID, UInt64 attackedGUID)
			{
				out_packet.start(game::server_packet::AttackStop);
				out_packet
					<< io::write_packed_guid(attackerGUID)
					<< io::write_packed_guid(attackedGUID)
				    << io::write<NetUInt32>(0);		// TODO
				out_packet.finish();
			}

			void attackStateUpdate(game::OutgoingPacket &out_packet, UInt64 attackerGUID, UInt64 attackedGUID, HitInfo hitInfo, UInt32 totalDamage, UInt32 absorbedDamage, UInt32 resistedDamage, UInt32 blockedDamage, VictimState targetState, WeaponAttack swingType, UInt32 damageSchool)
			{
				out_packet.start(game::server_packet::AttackerStateUpdate);
				out_packet
				        << io::write<NetUInt32>(hitInfo);

				const UInt32 realDamage = (totalDamage - absorbedDamage - resistedDamage - blockedDamage);

				out_packet
					<< io::write_packed_guid(attackerGUID)
					<< io::write_packed_guid(attackedGUID)
				    << io::write<NetUInt32>(realDamage)
				    << io::write<NetUInt8>(1)
				    << io::write<NetUInt32>(damageSchool)
				    << io::write<float>(realDamage)				// WTF?
				    << io::write<NetUInt32>(realDamage)
				    << io::write<NetUInt32>(absorbedDamage)
				    << io::write<NetUInt32>(resistedDamage)
				    << io::write<NetUInt32>(targetState)
				    << io::write<NetUInt32>(absorbedDamage == 0 ? 0 : -1)
				    << io::write<NetUInt32>(0)					// Generated rage?
				    << io::write<NetUInt32>(blockedDamage);
				out_packet.finish();
			}

			void attackSwingBadFacing(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::AttackSwingBadFacing);
				out_packet.finish();
			}

			void attackSwingCantAttack(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::AttackSwingCantAttack);
				out_packet.finish();
			}

			void attackSwingDeadTarget(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::AttackSwingDeadTarget);
				out_packet.finish();
			}

			void attackSwingNotStanding(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::AttackSwingNotStanding);
				out_packet.finish();
			}

			void attackSwingNotInRange(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::AttackSwingNotInRange);
				out_packet.finish();
			}

			void inventoryChangeFailure(game::OutgoingPacket &out_packet, InventoryChangeFailure failure, GameItem *itemA, GameItem *itemB)
			{
				out_packet.start(game::server_packet::InventoryChangeFailure);
				out_packet << io::write<NetUInt8>(failure);

				if (failure != inventory_change_failure::Okay)
				{
					out_packet
					        << io::write<NetUInt64>((itemA != nullptr) ? itemA->getGuid() : 0)
					        << io::write<NetUInt64>((itemB != nullptr) ? itemB->getGuid() : 0)
					        << io::write<NetUInt8>(0);

					if (failure == inventory_change_failure::CantEquipLevel)
					{
						UInt32 level = 0;
						if (itemA != nullptr)
						{
							level = itemA->getEntry().requiredlevel();
						}

						out_packet << io::write<NetUInt32>(level);                          // new 2.4.0
					}
				}
				out_packet.finish();
			}

			void updateComboPoints(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt8 comboPoints)
			{
				out_packet.start(game::server_packet::UpdateComboPoints);
				out_packet
					<< io::write_packed_guid(targetGuid)
				    << io::write<NetUInt8>(comboPoints);
				out_packet.finish();
			}

			void spellHealLog(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt64 casterGuid, UInt32 spellId, UInt32 amount, bool critical)
			{
				out_packet.start(game::server_packet::SpellHealLog);
				out_packet
					<< io::write_packed_guid(targetGuid)
					<< io::write_packed_guid(casterGuid)
				    << io::write<NetUInt32>(spellId)
				    << io::write<NetUInt32>(amount)
				    << io::write<NetUInt8>(critical ? 1 : 0)
				    << io::write<NetUInt8>(0);
				out_packet.finish();
			}

			void periodicAuraLog(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt64 casterGuid, UInt32 spellId, UInt32 auraType, UInt32 damage, UInt32 dmgSchool, UInt32 absorbed, UInt32 resisted)
			{
				out_packet.start(game::server_packet::PeriodicAuraLog);
				out_packet
					<< io::write_packed_guid(targetGuid)
					<< io::write_packed_guid(casterGuid)
				    << io::write<NetUInt32>(spellId)
				    << io::write<NetUInt32>(1)
				    << io::write<NetUInt32>(auraType)
				    << io::write<NetUInt32>(damage)
				    << io::write<NetUInt32>(dmgSchool)
				    << io::write<NetUInt32>(absorbed)
				    << io::write<NetUInt32>(resisted);
				out_packet.finish();
			}

			void periodicAuraLog(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt64 casterGuid, UInt32 spellId, UInt32 auraType, UInt32 heal)
			{
				out_packet.start(game::server_packet::PeriodicAuraLog);
				out_packet
					<< io::write_packed_guid(targetGuid)
					<< io::write_packed_guid(casterGuid)
				    << io::write<NetUInt32>(spellId)
				    << io::write<NetUInt32>(1)
				    << io::write<NetUInt32>(auraType)
				    << io::write<NetUInt32>(heal);
				out_packet.finish();
			}

			void periodicAuraLog(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt64 casterGuid, UInt32 spellId, UInt32 auraType, UInt32 powerType, UInt32 amount)
			{
				out_packet.start(game::server_packet::PeriodicAuraLog);
				out_packet
					<< io::write_packed_guid(targetGuid)
					<< io::write_packed_guid(casterGuid)
				    << io::write<NetUInt32>(spellId)
				    << io::write<NetUInt32>(1)
				    << io::write<NetUInt32>(auraType)
				    << io::write<NetUInt32>(powerType)
				    << io::write<NetUInt32>(amount);
				out_packet.finish();
			}

			void updateAuraDuration(game::OutgoingPacket &out_packet, UInt8 slot, UInt32 durationMS)
			{
				out_packet.start(game::server_packet::UpdateAuraDuration);
				out_packet
				        << io::write<NetUInt8>(slot)
				        << io::write<NetUInt32>(durationMS);
				out_packet.finish();
			}

			void setExtraAuraInfo(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt8 slot, UInt32 spellId, UInt32 maxDurationMS, UInt32 durationMS)
			{
				out_packet.start(game::server_packet::SetExtraAuraInfo);
				out_packet
					<< io::write_packed_guid(targetGuid)
				    << io::write<NetUInt8>(slot)
				    << io::write<NetUInt32>(spellId)
				    << io::write<NetUInt32>(maxDurationMS)
				    << io::write<NetUInt32>(durationMS);
				out_packet.finish();
			}

			void setExtraAuraInfoNeedUpdate(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt8 slot, UInt32 spellId, UInt32 maxDurationMS, UInt32 durationMS)
			{
				out_packet.start(game::server_packet::SetExtraAuraInfoNeedUpdate);
				out_packet
					<< io::write_packed_guid(targetGuid)
				    << io::write<NetUInt8>(slot)
				    << io::write<NetUInt32>(spellId)
				    << io::write<NetUInt32>(maxDurationMS)
				    << io::write<NetUInt32>(durationMS);
				out_packet.finish();
			}

			void clearExtraAuraInfo(game::OutgoingPacket & out_packet, UInt64 casterGuid, UInt32 spellId)
			{
				out_packet.start(game::server_packet::ClearExtraAuraInfo);
				out_packet
					<< io::write_packed_guid(casterGuid)
					<< io::write<NetUInt32>(spellId);
				out_packet.finish();
			}

			void logXPGain(game::OutgoingPacket &out_packet, UInt64 victimGUID, UInt32 givenXP, UInt32 restXP, bool hasReferAFriendBonus)
			{
				out_packet.start(game::server_packet::LogXPGain);
				out_packet
				        << io::write<NetUInt64>(victimGUID)
				        << io::write<NetUInt32>(givenXP + restXP)
				        << io::write<NetUInt8>(victimGUID != 0 ? 0 : 1);
				if (victimGUID != 0)
				{
					out_packet
					        << io::write<NetUInt32>(givenXP)
					        << io::write<float>(1.0f);
				}
				out_packet
				        << io::write<NetUInt8>(0);
				out_packet.finish();
			}

			void levelUpInfo(game::OutgoingPacket &out_packet, UInt32 level, Int32 healthGained, Int32 manaGained, Int32 strengthGained, Int32 agilityGained, Int32 staminaGained, Int32 intellectGained, Int32 spiritGained)
			{
				out_packet.start(game::server_packet::LevelUpInfo);
				out_packet
				        << io::write<NetUInt32>(level)
				        << io::write<NetInt32>(healthGained)
				        << io::write<NetInt32>(manaGained)
				        << io::write<NetUInt32>(0)
				        << io::write<NetUInt32>(0)
				        << io::write<NetUInt32>(0)
				        << io::write<NetUInt32>(0)
				        << io::write<NetInt32>(strengthGained)
				        << io::write<NetInt32>(agilityGained)
				        << io::write<NetInt32>(staminaGained)
				        << io::write<NetInt32>(intellectGained)
				        << io::write<NetInt32>(spiritGained);
				out_packet.finish();
			}

			void partyMemberStats(game::OutgoingPacket &out_packet, const GameCharacter &character)
			{
				out_packet.start(game::server_packet::PartyMemberStats);

				UInt64 charGuid = character.getGuid();
				auto updateFlags = character.getGroupUpdateFlags();
				out_packet
					<< io::write_packed_guid(charGuid)
				    << io::write<NetUInt32>(updateFlags);

				if (updateFlags & group_update_flags::Status)
				{
					UInt16 flags = 0;
					flags = group_member_status::Online;
					out_packet
					        << io::write<NetUInt16>(flags);
				}

				if (updateFlags & group_update_flags::CurrentHP) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::Health));
				}

				if (updateFlags & group_update_flags::MaxHP) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::MaxHealth));
				}

				UInt8 powerType = character.getByteValue(unit_fields::Bytes0, 3);
				if (updateFlags & group_update_flags::PowerType) {
					out_packet << io::write<NetUInt8>(powerType);
				}

				if (updateFlags & group_update_flags::CurrentPower) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::Power1 + powerType));
				}

				if (updateFlags & group_update_flags::MaxPower) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::MaxPower1 + powerType));
				}

				if (updateFlags & group_update_flags::Level) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::Level));
				}

				if (updateFlags & group_update_flags::Zone) {
					out_packet << io::write<NetUInt16>(character.getZone());
				}

				if (updateFlags & group_update_flags::Position)
				{
					math::Vector3 location(character.getLocation());
					out_packet << io::write<NetUInt16>(UInt16(location.x)) << io::write<NetUInt16>(UInt16(location.y));
				}

				if (updateFlags & group_update_flags::Auras)
				{
					UInt64 auramask = 0;
					for (UInt32 i = 0; i < 56; ++i)
					{
						if (character.getUInt32Value(unit_fields::AuraEffect + i) != 0)
						{
							auramask |= (UInt64(1) << i);
						}
					}
					out_packet
					        << io::write<NetUInt64>(auramask);
					for (UInt32 i = 0; i < 56; ++i)
					{
						UInt32 aura = character.getUInt32Value(unit_fields::AuraEffect + i);
						if (aura != 0)
						{
							out_packet
								<< io::write<NetUInt16>(aura) << io::write<NetUInt8>(1);
						}
					}
				}

				if (updateFlags & group_update_flags::PetGUID) {
					out_packet << io::write<NetUInt64>(0);
				}

				if (updateFlags & group_update_flags::PetName) {
					out_packet << io::write<NetUInt8>(0);
				}

				if (updateFlags & group_update_flags::PetModelID) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetCurrentHP) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetMaxHP) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetPowerType) {
					out_packet << io::write<NetUInt8>(0);
				}

				if (updateFlags & group_update_flags::PetCurrentPower) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetMaxPower) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetAuras) {
					out_packet << io::write<NetUInt64>(0);
				}

				out_packet.finish();
			}

			void partyMemberStatsFull(game::OutgoingPacket &out_packet, const GameCharacter &character)
			{
				out_packet.start(game::server_packet::PartyMemberStatsFull);

				UInt64 charGuid = character.getGuid();

				// Full player update
				auto updateFlags = group_update_flags::Full;
				updateFlags = static_cast<GroupUpdateFlags>(updateFlags & ~group_update_flags::Pet);
				out_packet
					<< io::write_packed_guid(charGuid)
				    << io::write<NetUInt32>(updateFlags);

				if (updateFlags & group_update_flags::Status)
				{
					UInt16 flags = 0;
					flags = group_member_status::Online;
					out_packet
					        << io::write<NetUInt16>(flags);
				}

				if (updateFlags & group_update_flags::CurrentHP) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::Health));
				}

				if (updateFlags & group_update_flags::MaxHP) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::MaxHealth));
				}

				UInt8 powerType = character.getByteValue(unit_fields::Bytes0, 3);
				if (updateFlags & group_update_flags::PowerType) {
					out_packet << io::write<NetUInt8>(powerType);
				}

				if (updateFlags & group_update_flags::CurrentPower) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::Power1 + powerType));
				}

				if (updateFlags & group_update_flags::MaxPower) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::MaxPower1 + powerType));
				}

				if (updateFlags & group_update_flags::Level) {
					out_packet << io::write<NetUInt16>(character.getUInt32Value(unit_fields::Level));
				}

				if (updateFlags & group_update_flags::Zone) {
					out_packet << io::write<NetUInt16>(character.getZone());
				}

				if (updateFlags & group_update_flags::Position)
				{
					math::Vector3 location(character.getLocation());
					out_packet << io::write<NetUInt16>(UInt16(location.x)) << io::write<NetUInt16>(UInt16(location.y));
				}

				if (updateFlags & group_update_flags::Auras)
				{
					UInt64 auramask = 0;
					for (UInt32 i = 0; i < 56; ++i)
					{
						if (character.getUInt32Value(unit_fields::AuraEffect + i) != 0)
						{
							auramask |= (UInt64(1) << i);
						}
					}
					out_packet
					        << io::write<NetUInt64>(auramask);
					for (UInt32 i = 0; i < 56; ++i)
					{
						UInt32 aura = character.getUInt32Value(unit_fields::AuraEffect + i);
						if (aura != 0)
						{
							out_packet
							        << io::write<NetUInt16>(aura) << io::write<NetUInt8>(1);
						}
					}
				}

				if (updateFlags & group_update_flags::PetGUID) {
					out_packet << io::write<NetUInt64>(0);
				}

				if (updateFlags & group_update_flags::PetName) {
					out_packet << io::write<NetUInt8>(0);
				}

				if (updateFlags & group_update_flags::PetModelID) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetCurrentHP) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetMaxHP) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetPowerType) {
					out_packet << io::write<NetUInt8>(0);
				}

				if (updateFlags & group_update_flags::PetCurrentPower) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetMaxPower) {
					out_packet << io::write<NetUInt16>(0);
				}

				if (updateFlags & group_update_flags::PetAuras) {
					out_packet << io::write<NetUInt64>(0);
				}

				out_packet.finish();
			}

			void partyMemberStatsFullOffline(game::OutgoingPacket &out_packet, UInt64 offlineGUID)
			{
				out_packet.start(game::server_packet::PartyMemberStatsFull);
				out_packet
					<< io::write_packed_guid(offlineGUID)
				    << io::write<NetUInt32>(group_update_flags::Status)
				    << io::write<NetUInt16>(group_member_status::Offline);
				out_packet.finish();
			}

			void partyCommandResult(game::OutgoingPacket &out_packet, PartyOperation operation, const String &member, PartyResult result)
			{
				out_packet.start(game::server_packet::PartyCommandResult);
				out_packet
				        << io::write<NetUInt32>(operation)
				        << io::write_range(member) << io::write<NetUInt8>(0)
				        << io::write<NetInt32>(result);
				out_packet.finish();
			}

			void groupInvite(game::OutgoingPacket &out_packet, const String &inviterName)
			{
				out_packet.start(game::server_packet::GroupInvite);
				out_packet
				        << io::write_range(inviterName) << io::write<NetUInt8>(0);
				out_packet.finish();
			}

			void groupDecline(game::OutgoingPacket &out_packet, const String &inviterName)
			{
				out_packet.start(game::server_packet::GroupDecline);
				out_packet
				        << io::write_range(inviterName) << io::write<NetUInt8>(0);
				out_packet.finish();
			}

			void groupUninvite(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::GroupUninvite);
				out_packet.finish();
			}

			void groupSetLeader(game::OutgoingPacket &out_packet, const String &slotName)
			{
				out_packet.start(game::server_packet::GroupSetLeader);
				out_packet
				        << io::write_range(slotName) << io::write<NetUInt8>(0);
				out_packet.finish();
			}

			void groupDestroyed(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::GroupDestroyed);
				out_packet.finish();
			}

			void groupList(game::OutgoingPacket &out_packet, UInt64 receiver, UInt8 groupType, bool isBattlegroundGroup, UInt8 groupId, UInt8 assistant, UInt64 data1, const std::map<UInt64, GroupMemberSlot> &groupMembers, UInt64 leaderGuid, UInt8 lootMethod, UInt64 lootMasterGUID, UInt8 lootTreshold, UInt8 difficulty)
			{
				out_packet.start(game::server_packet::GroupList);
				out_packet
				        << io::write<NetUInt8>(groupType)
				        << io::write<NetUInt8>(isBattlegroundGroup ? 1 : 0)
				        << io::write<NetUInt8>(groupId)
				        << io::write<NetUInt8>(assistant)
				        << io::write<NetUInt64>(data1)
				        << io::write<NetUInt32>(groupMembers.size() - 1);
				for (auto &it : groupMembers)
				{
					if (it.first == receiver) {
						continue;
					}

					out_packet
					        << io::write_range(it.second.name) << io::write<NetUInt8>(0)
					        << io::write<NetUInt64>(it.first)
					        << io::write<NetUInt8>(it.second.status)
					        << io::write<NetUInt8>(it.second.group)
					        << io::write<NetUInt8>(it.second.assistant ? 1 : 0);
				}
				out_packet
				        << io::write<NetUInt64>(leaderGuid);
				if (groupMembers.size() > 1)
				{
					out_packet
					        << io::write<NetUInt8>(lootMethod)
					        << io::write<NetUInt64>(lootMasterGUID)
					        << io::write<NetUInt8>(lootTreshold)
					        << io::write<NetUInt8>(difficulty);

				}
				out_packet.finish();
			}


			void groupListRemoved(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::GroupList);
				out_packet
				        << io::write<NetUInt64>(0)
				        << io::write<NetUInt64>(0)
				        << io::write<NetUInt64>(0);
				out_packet.finish();
			}


			void newWorld(game::OutgoingPacket &out_packet, UInt32 newMap, math::Vector3 location, float o)
			{
				out_packet.start(game::server_packet::NewWorld);
				out_packet
				        << io::write<NetUInt32>(newMap)
				        << io::write<float>(location.x)
				        << io::write<float>(location.y)
				        << io::write<float>(location.z)
				        << io::write<float>(o);
				out_packet.finish();
			}

			void transferPending(game::OutgoingPacket &out_packet, UInt32 newMap, UInt32 transportId, UInt32 oldMap)
			{
				out_packet.start(game::server_packet::TransferPending);
				out_packet
				        << io::write<NetUInt32>(newMap);
				if (transportId != 0)
				{
					out_packet
					        << io::write<NetUInt32>(transportId)
					        << io::write<NetUInt32>(oldMap);
				}
				out_packet.finish();
			}

			void transferAborted(game::OutgoingPacket &out_packet, UInt32 map, TransferAbortReason reason)
			{
				out_packet.start(game::server_packet::TransferAborted);
				out_packet
				        << io::write<NetUInt32>(map)
				        << io::write<NetUInt16>(reason);
				out_packet.finish();
			}

			void chatWrongFaction(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::ChatWrongFaction);
				out_packet.finish();
			}

			void gameObjectQueryResponse(game::OutgoingPacket &out_packet, Int32 localeIndex, const proto::ObjectEntry &entry)
			{
				const String &name = (entry.name_loc_size() >= localeIndex) ?
					entry.name_loc(localeIndex - 1) : entry.name();
				const String &caption = (entry.caption_loc_size() >= localeIndex) ?
					entry.caption_loc(localeIndex - 1) : entry.caption();

				out_packet.start(game::server_packet::GameObjectQueryResponse);
				out_packet
				        << io::write<NetUInt32>(entry.id())
				        << io::write<NetUInt32>(entry.type())
				        << io::write<NetUInt32>(entry.displayid())
				        << io::write_range(name.empty() ? entry.name() : name) << io::write<NetUInt8>(0)
				        << io::write<NetUInt8>(0)
				        << io::write<NetUInt8>(0)
				        << io::write<NetUInt8>(0)
				        << io::write<NetUInt8>(0)
				        << io::write_range(caption.empty() ? entry.caption() : caption) << io::write<NetUInt8>(0)
				        << io::write<NetUInt8>(0)
				        << io::write_range(entry.data())
				        << io::write<float>(entry.scale());
				out_packet.finish();
			}

			void gameObjectQueryResponseEmpty(game::OutgoingPacket &out_packet, const UInt32 entry)
			{
				out_packet.start(game::server_packet::GameObjectQueryResponse);
				out_packet
				        << io::write<NetUInt32>(entry | 0x80000000);
				out_packet.finish();
			}

			void forceMoveRoot(game::OutgoingPacket &out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::ForceMoveRoot);
				out_packet
					<< io::write_packed_guid(guid)
				    << io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void forceMoveUnroot(game::OutgoingPacket &out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::ForceMoveUnroot);
				out_packet
					<< io::write_packed_guid(guid)
				    << io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void emote(game::OutgoingPacket &out_packet, UInt32 animId, UInt64 guid)
			{
				out_packet.start(game::server_packet::Emote);
				out_packet
				        << io::write<NetUInt32>(animId)
				        << io::write<NetUInt64>(guid);
				out_packet.finish();
			}

			void textEmote(game::OutgoingPacket &out_packet, UInt64 guid, UInt32 textEmote, UInt32 emoteNum, const String &name)
			{
				out_packet.start(game::server_packet::TextEmote);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt32>(textEmote)
				        << io::write<NetUInt32>(emoteNum)
				        << io::write<NetUInt32>(name.size() + 1);
				if (name.size() > 0)
				{
					out_packet
					        << io::write_range(name) << io::write<NetUInt8>(0);
				}
				else
				{
					out_packet
					        << io::write<NetUInt8>(0);
				}
				out_packet.finish();
			}

			void environmentalDamageLog(game::OutgoingPacket &out_packet, UInt64 guid, UInt8 type, UInt32 damage, UInt32 absorb, UInt32 resist)
			{
				out_packet.start(game::server_packet::EnvironmentalDamageLog);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt8>(type)
				        << io::write<NetUInt32>(damage)
				        << io::write<NetUInt32>(absorb)
				        << io::write<NetUInt32>(resist);
				out_packet.finish();
			}

			void durabilityDamageDeath(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::DurabilityDamageDeath);
				out_packet.finish();
			}

			void playSound(game::OutgoingPacket &out_packet, UInt32 soundId)
			{
				out_packet.start(game::server_packet::PlaySound);
				out_packet
				        << io::write<NetUInt32>(soundId);
				out_packet.finish();
			}

			void explorationExperience(game::OutgoingPacket &out_packet, UInt32 areaId, UInt32 experience)
			{
				out_packet.start(game::server_packet::ExplorationExperience);
				out_packet
				        << io::write<NetUInt32>(areaId)
				        << io::write<NetUInt32>(experience);
				out_packet.finish();
			}

			void aiReaction(game::OutgoingPacket &out_packet, UInt64 creatureGUID, UInt32 reaction)
			{
				out_packet.start(game::server_packet::AiReaction);
				out_packet
				        << io::write<NetUInt64>(creatureGUID)
				        << io::write<NetUInt32>(reaction);
				out_packet.finish();
			}

			void spellDamageShield(game::OutgoingPacket &out_packet, UInt64 targetGuid, UInt64 casterGuid, UInt32 spellId, UInt32 damage, UInt32 dmgSchool)
			{
				out_packet.start(game::server_packet::SpellDamageShield);
				out_packet
				        << io::write<NetUInt64>(targetGuid)
				        << io::write<NetUInt64>(casterGuid)
				        << io::write<NetUInt32>(spellId)
				        << io::write<NetUInt32>(damage)
				        << io::write<NetUInt32>(dmgSchool);
				out_packet.finish();
			}

			void lootResponseError(game::OutgoingPacket &out_packet, UInt64 guid, loot_type::Type type, loot_error::Type error)
			{
				out_packet.start(game::server_packet::LootResponse);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt8>(type)
				        << io::write<NetUInt8>(error);
				out_packet.finish();
			}

			void lootResponse(game::OutgoingPacket &out_packet, UInt64 guid, loot_type::Type type, UInt64 playerGuid, const LootInstance &loot)
			{
				out_packet.start(game::server_packet::LootResponse);
				out_packet
					<< io::write<NetUInt64>(guid)
					<< io::write<NetUInt8>(type);
				loot.serialize(out_packet, playerGuid);
				out_packet.finish();
			}

			void lootReleaseResponse(game::OutgoingPacket &out_packet, UInt64 guid)
			{
				out_packet.start(game::server_packet::LootReleaseResponse);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt8>(1);
				out_packet.finish();
			}

			void lootRemoved(game::OutgoingPacket &out_packet, UInt8 slot)
			{
				out_packet.start(game::server_packet::LootRemoved);
				out_packet
				        << io::write<NetUInt8>(slot);
				out_packet.finish();
			}

			void lootMoneyNotify(game::OutgoingPacket &out_packet, UInt32 moneyPerPlayer)
			{
				out_packet.start(game::server_packet::LootMoneyNotify);
				out_packet
				        << io::write<NetUInt32>(moneyPerPlayer);
				out_packet.finish();
			}

			void lootItemNotify(game::OutgoingPacket &out_packet /* TODO */)
			{
				out_packet.start(game::server_packet::LootItemNotify);
				out_packet.finish();
			}

			void lootClearMoney(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::LootClearMoney);
				out_packet.finish();
			}

			void raidTargetUpdateList(game::OutgoingPacket &out_packet, const std::array<UInt64, 8> &list)
			{
				out_packet.start(game::server_packet::RaidTargetUpdate);
				out_packet
				        << io::write<NetUInt8>(0x01);	// List mode
				UInt8 slot = 0;
				for (auto &guid : list)
				{
					if (guid != 0)
					{
						out_packet
						        << io::write<NetUInt8>(slot)
						        << io::write<NetUInt64>(guid);
					}

					slot++;
				}
				out_packet.finish();
			}

			void raidTargetUpdate(game::OutgoingPacket &out_packet, UInt8 slot, UInt64 guid)
			{
				out_packet.start(game::server_packet::RaidTargetUpdate);
				out_packet
				        << io::write<NetUInt8>(0x00)	// No list
				        << io::write<NetUInt8>(slot)
				        << io::write<NetUInt64>(guid)
				        ;
				out_packet.finish();
			}

			void raidReadyCheck(game::OutgoingPacket &out_packet, UInt64 guid)
			{
				out_packet.start(game::server_packet::RaidReadyCheck);
				out_packet
				        << io::write<NetUInt64>(guid)
				        ;
				out_packet.finish();
			}

			void raidReadyCheckConfirm(game::OutgoingPacket &out_packet, UInt64 guid, UInt8 state)
			{
				out_packet.start(game::server_packet::RaidReadyCheckConfirm);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt8>(state)
				        ;
				out_packet.finish();
			}

			void raidReadyCheckFinished(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::RaidReadyCheckFinished);
				out_packet.finish();
			}

			void learnedSpell(game::OutgoingPacket &out_packet, UInt32 spellId)
			{
				out_packet.start(game::server_packet::LearnedSpell);
				out_packet
				        << io::write<NetUInt32>(spellId)
				        ;
				out_packet.finish();
			}

			void supercededSpell(game::OutgoingPacket & out_packet, UInt32 prevSpellId, UInt32 newSpellId)
			{
				out_packet.start(game::server_packet::SupercededSpell);
				out_packet
					<< io::write<NetUInt16>(prevSpellId)
					<< io::write<NetUInt16>(newSpellId)
					;
				out_packet.finish();
			}

			void itemPushResult(game::OutgoingPacket &out_packet, UInt64 playerGuid, const GameItem &item, bool wasLooted, bool wasCreated, UInt8 bagSlot, UInt8 slot, UInt32 addedCount, UInt32 totalCount)
			{
				out_packet.start(game::server_packet::ItemPushResult);
				out_packet
				        << io::write<NetUInt64>(playerGuid)
				        << io::write<NetUInt32>(wasLooted ? 0 : 1)
				        << io::write<NetUInt32>(wasCreated ? 1 : 0)
				        << io::write<NetUInt32>(1)
				        << io::write<NetUInt8>(bagSlot)
				        << io::write<NetUInt32>((addedCount == item.getUInt32Value(item_fields::StackCount)) ? slot : -1)
				        << io::write<NetUInt32>(item.getEntry().id())
				        << io::write<NetUInt32>(0)
				        << io::write<NetInt32>(0)
				        << io::write<NetUInt32>(addedCount)
				        << io::write<NetUInt32>(totalCount)
				        ;
				out_packet.finish();
			}

			void listInventory(game::OutgoingPacket &out_packet, UInt64 vendorGuid,
			                   const proto::ItemManager &itemManager, const std::vector<proto::VendorItemEntry> &itemList)
			{
				out_packet.start(game::server_packet::ListInventory);
				out_packet
				        << io::write<NetUInt64>(vendorGuid)
				        << io::write<NetUInt8>(itemList.size());
				if (itemList.empty())
				{
					out_packet << io::write<NetUInt8>(0);		// Error code: 0 (no error, just an empty list)
				}

				UInt32 index = 0;
				for (const auto &entry : itemList)
				{
					const auto *item = itemManager.getById(entry.item());
					if (!item) {
						continue;
					}

					out_packet
					        << io::write<NetUInt32>(++index)
					        << io::write<NetUInt32>(entry.item())
					        << io::write<NetUInt32>(item->displayid())
					        << io::write<NetUInt32>(entry.maxcount() <= 0 ? 0xFFFFFFFF : entry.maxcount())		// TODO
					        << io::write<NetUInt32>(item->buyprice())	// TODO: reputation discount
					        << io::write<NetUInt32>(item->durability())
					        << io::write<NetUInt32>(item->buycount())
					        << io::write<NetUInt32>(entry.extendedcost())
					        ;
				}
				out_packet.finish();
			}

			void trainerList(game::OutgoingPacket &out_packet, const GameCharacter &character, UInt64 trainerGuid, const proto::TrainerEntry &trainerEntry)
			{
				out_packet.start(game::server_packet::TrainerList);
				out_packet
					<< io::write<NetUInt64>(trainerGuid)
					<< io::write<NetUInt32>(trainerEntry.type());

				size_t spellCountPos = out_packet.sink().position();
				out_packet
					<< io::write<NetUInt32>(trainerEntry.spells_size());

				const UInt8 GreenSpell = 0;
				const UInt8 RedSpell = 1;
				const UInt8 GreySpell = 2;

				UInt32 spellCount = trainerEntry.spells_size();
				for (int i = 0; i < trainerEntry.spells_size(); ++i)
				{
					const auto &spell = trainerEntry.spells(i);
					const auto *entry = character.getProject().spells.getById(spell.spell());
					if (!entry)
					{
						WLOG("Unable to find spell " << spell.spell());
						spellCount--;
						continue;
					}

					const UInt32 raceMask = 1 << (character.getRace() - 1);
					if (entry->racemask() != 0 && (entry->racemask() & raceMask) == 0)
					{
						spellCount--;
						continue;
					}

					const UInt32 classMask = 1 << (character.getClass() - 1);
					if (entry->classmask() != 0 && (entry->classmask() & classMask) == 0)
					{
						spellCount--;
						continue;
					}

					UInt8 state = GreenSpell;
					if (character.hasSpell(spell.spell())) {
						state = GreySpell;
					}
					else if (character.getLevel() < spell.reqlevel() && spell.reqlevel() > 0) {
						state = RedSpell;
					}
					if (spell.reqskill() > 0)
					{
						UInt16 cur = 0, max = 0;
						if (!character.getSkillValue(spell.reqskill(), cur, max) ||
							cur < spell.reqskillval())
						{
							state = RedSpell;
						}
					}
					if (entry->prevspell() != 0 && !character.hasSpell(entry->prevspell()))
					{
						state = RedSpell;
					}
					// TODO More checks
					out_packet
					        << io::write<NetUInt32>(spell.spell())		// Spell ID
					        << io::write<NetUInt8>(state)					// Spell State
					        << io::write<NetUInt32>(spell.spellcost())		// Spell cost
					        << io::write<NetUInt32>(0)		// TODO			// Primary Prof
					        << io::write<NetUInt32>(0)		// TODO			// Must be equal to previous field
					        << io::write<NetUInt8>(spell.reqlevel())			// Required character level
					        << io::write<NetUInt32>(spell.reqskill())	// Required skill id
					        << io::write<NetUInt32>(spell.reqskillval())	// Required skill value
					        << io::write<NetUInt32>(entry->prevspell())		// Previous spell
					        << io::write<NetUInt32>(0)						// Required spell
					        << io::write<NetUInt32>(0)						// Unknown
					        ;
				}

				out_packet << io::write_range(trainerEntry.title()) << io::write<NetUInt8>(0);
				out_packet.writePOD(spellCountPos, spellCount);
				out_packet.finish();
			}

			void gossipMessage(game::OutgoingPacket &out_packet, UInt64 objectGuid, UInt32 titleTextId)
			{
				out_packet.start(game::server_packet::GossipMessage);
				out_packet
				        << io::write<NetUInt64>(objectGuid)
				        << io::write<NetUInt32>(0)				// Menu ID
				        << io::write<NetUInt32>(titleTextId)
				        << io::write<NetUInt32>(0)				// Menu item count
				        << io::write<NetUInt32>(0);				// Quest item count
				out_packet.finish();
			}

			void trainerBuySucceeded(game::OutgoingPacket &out_packet, UInt64 trainerGuid, UInt32 spellId)
			{
				out_packet.start(game::server_packet::TrainerBuySucceeded);
				out_packet
				        << io::write<NetUInt64>(trainerGuid)
				        << io::write<NetUInt32>(spellId);
				out_packet.finish();
			}

			void playSpellVisual(game::OutgoingPacket &out_packet, UInt64 unitGuid, UInt32 visualId)
			{
				out_packet.start(game::server_packet::PlaySpellVisual);
				out_packet
				        << io::write<NetUInt64>(unitGuid)
				        << io::write<NetUInt32>(visualId);
				out_packet.finish();
			}

			void playSpellImpact(game::OutgoingPacket &out_packet, UInt64 unitGuid, UInt32 spellId)
			{
				out_packet.start(game::server_packet::PlaySpellImpact);
				out_packet
				        << io::write<NetUInt64>(unitGuid)
				        << io::write<NetUInt32>(spellId);
				out_packet.finish();
			}

			void charRename(game::OutgoingPacket &out_packet, game::ResponseCode response, UInt64 unitGuid, const String &newName)
			{
				out_packet.start(game::server_packet::CharRename);
				out_packet
				        << io::write<NetUInt8>(response);

				if (response == game::response_code::Success)
				{
					out_packet
					        << io::write<NetUInt64>(unitGuid)
					        << io::write_range(newName) << io::write<NetUInt8>(0);
				}
				out_packet.finish();
			}

			// Force / Without Force opcodes to send for each movement type
			static const UInt32 moveOpCodes[MovementType::Count][2] = {
				{ game::server_packet::ForceWalkSpeedChange, game::server_packet::MoveSetWalkSpeed },
				{ game::server_packet::ForceRunSpeedChange, game::server_packet::MoveSetRunSpeed },
				{ game::server_packet::ForceRunBackSpeedChange, game::server_packet::MoveSetRunBackSpeed },
				{ game::server_packet::ForceSwimSpeedChange, game::server_packet::MoveSetSwimSpeed },
				{ game::server_packet::ForceSwimBackSpeedChange, game::server_packet::MoveSetSwimBackSpeed },
				{ game::server_packet::ForceTurnRateChange, game::server_packet::MoveSetTurnRate },
				{ game::server_packet::ForceFlightSpeedChange, game::server_packet::SetFlightSpeed },
				{ game::server_packet::ForceFlightBackSpeedChange, game::server_packet::SetFlightBackSpeed },
			};

			void sendForceSpeedChange(game::OutgoingPacket &out_packet, MovementType moveType, UInt64 guid, float speed, UInt32 counter)
			{
				ASSERT(moveType < MovementType::Count);
				out_packet.start(moveOpCodes[moveType][0]);
				out_packet
					<< io::write_packed_guid(guid)
				    << io::write<NetUInt32>(counter);
				if (moveType == movement_type::Run)
				{
					// If this is set to true, the client stores the speed value for some
					// yet unknown reason which seems to be transport related.
					out_packet << io::write<NetUInt8>(0);
				}
				out_packet << float(speed);
				out_packet.finish();
			}

			void sendSpeedChange(game::OutgoingPacket & out_packet, MovementType moveType, UInt64 guid, const MovementInfo & info, float newRate)
			{
				ASSERT(moveType < MovementType::Count);
				out_packet.start(moveOpCodes[moveType][1]);
				out_packet
					<< io::write_packed_guid(guid)
					<< info;
				out_packet << newRate;
				out_packet.finish();
			}

			void questgiverStatus(game::OutgoingPacket &out_packet, UInt64 guid, game::QuestgiverStatus status)
			{
				out_packet.start(game::server_packet::QuestgiverStatus);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt8>(status);
				out_packet.finish();
			}

			void questgiverQuestList(game::OutgoingPacket &out_packet, UInt64 guid, const String &title, UInt32 emoteDelay, UInt32 emote, const std::vector<QuestMenuItem> &menu)
			{
				out_packet.start(game::server_packet::QuestgiverQuestList);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write_range(title) << io::write<NetUInt8>(0)
				        << io::write<NetUInt32>(emoteDelay)
				        << io::write<NetUInt32>(emote)
				        << io::write<NetUInt8>(menu.size());
				for (const auto &menuItem : menu)
				{
					ASSERT(menuItem.quest);
					out_packet
					        << io::write<NetUInt32>(menuItem.quest->id())
					        << io::write<NetUInt32>(menuItem.menuIcon)
					        << io::write<NetInt32>(menuItem.questLevel)
					        << io::write_range(menuItem.title) << io::write<NetUInt8>(0);
				}
				out_packet.finish();
			}

			void questgiverQuestDetails(game::OutgoingPacket &out_packet, Int32 localeIndex, UInt64 guid, const proto::ItemManager &items, const proto::QuestEntry &quest)
			{
				const String &name = (quest.name_loc_size() >= localeIndex) ? quest.name_loc(localeIndex - 1) : quest.name();
				const String &detailstext = (quest.detailstext_loc_size() >= localeIndex) ? quest.detailstext_loc(localeIndex - 1) : quest.detailstext();
				const String &objectivestext = (quest.objectivestext_loc_size() >= localeIndex) ? quest.objectivestext_loc(localeIndex - 1) : quest.objectivestext();
				
				out_packet.start(game::server_packet::QuestgiverQuestDetails);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt32>(quest.id())
				        << io::write_range(name.empty() ? quest.name() : name) << io::write<NetUInt8>(0)
				        << io::write_range(detailstext.empty() ? quest.detailstext() : detailstext) << io::write<NetUInt8>(0)
				        << io::write_range(objectivestext.empty() ? quest.objectivestext() : objectivestext) << io::write<NetUInt8>(0)
				        << io::write<NetUInt32>(1)		// Activate accept button? (true)
				        << io::write<NetUInt32>(quest.suggestedplayers());

				if (quest.flags() & game::quest_flags::HiddenRewards)
				{
					out_packet
					        << io::write<NetUInt32>(0)		// Rewarded chosen items
					        << io::write<NetUInt32>(0)		// Rewarded additional items
					        << io::write<NetUInt32>(0);		// Rewarded money
				}
				else
				{
					// Choice item count
					out_packet
					        << io::write<NetUInt32>(quest.rewarditemschoice_size());
					for (const auto &reward : quest.rewarditemschoice())
					{
						out_packet
						        << io::write<NetUInt32>(reward.itemid())
						        << io::write<NetUInt32>(reward.count());
						const auto *item = items.getById(reward.itemid());
						out_packet
						        << io::write<NetUInt32>(item ? item->displayid() : 0);
					}

					// Additional item count
					out_packet
					        << io::write<NetUInt32>(quest.rewarditems_size());
					for (const auto &reward : quest.rewarditems())
					{
						out_packet
						        << io::write<NetUInt32>(reward.itemid())
						        << io::write<NetUInt32>(reward.count());
						const auto *item = items.getById(reward.itemid());
						out_packet
						        << io::write<NetUInt32>(item ? item->displayid() : 0);
					}

					// Money
					out_packet
					        << io::write<NetUInt32>(quest.rewardmoney());
				}

				// Additional data
				out_packet
				        << io::write<NetUInt32>(quest.rewardhonorkills())
				        << io::write<NetUInt32>(quest.rewardspell())
				        << io::write<NetUInt32>(quest.rewardspellcast())
				        << io::write<NetUInt32>(quest.rewtitleid());

				// Emote count?
				out_packet
				        << io::write<NetUInt32>(4);
				for (UInt32 i = 0; i < 4; ++i)
				{
					out_packet
					        << io::write<NetUInt32>(0)		// Emote
					        << io::write<NetUInt32>(0);		// Delay
				}
				out_packet.finish();
			}

			void questQueryResponse(game::OutgoingPacket &out_packet, Int32 localeIndex, const proto::QuestEntry &quest)
			{
				out_packet.start(game::server_packet::QuestQueryResponse);
				out_packet
				        << io::write<NetUInt32>(quest.id())
				        << io::write<NetUInt32>(quest.method())
				        << io::write<NetInt32>(quest.questlevel())
				        << io::write<NetUInt32>(quest.zone())

				        << io::write<NetUInt32>(quest.type())
				        << io::write<NetUInt32>(quest.suggestedplayers())

				        << io::write<NetUInt32>(0)	// TODO: RepObjectiveFaction
				        << io::write<NetUInt32>(0)	// TODO: RepObjectiveValue

				        << io::write<NetUInt32>(0)	// TODO
				        << io::write<NetUInt32>(0)	// TODO

				        << io::write<NetUInt32>(quest.nextchainquestid());

				if (quest.flags() & game::quest_flags::HiddenRewards)
					out_packet << io::write<NetUInt32>(0);	// Money reward hidden
				else
					out_packet << io::write<NetUInt32>(quest.rewardmoney());

				out_packet
				        << io::write<NetUInt32>(quest.rewardmoneymaxlevel())
				        << io::write<NetUInt32>(quest.rewardspell())
				        << io::write<NetUInt32>(quest.rewardspellcast())

				        << io::write<NetUInt32>(quest.rewardhonorkills())
				        << io::write<NetUInt32>(quest.srcitemid())
				        << io::write<NetUInt32>(quest.flags())
				        << io::write<NetUInt32>(quest.rewtitleid());

				if (quest.flags() & game::quest_flags::HiddenRewards)
				{
					for (int i = 0; i < 4; ++i)
					{
						out_packet
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0);
					}
					for (int i = 0; i < 6; ++i)
					{
						out_packet
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0);
					}
				}
				else
				{
					for (int i = 0; i < 4; ++i)
					{
						out_packet
						        << io::write<NetUInt32>(i < quest.rewarditems_size() ? quest.rewarditems(i).itemid() : 0)
						        << io::write<NetUInt32>(i < quest.rewarditems_size() ? quest.rewarditems(i).count() : 0);
					}
					for (int i = 0; i < 6; ++i)
					{
						out_packet
						        << io::write<NetUInt32>(i < quest.rewarditemschoice_size() ? quest.rewarditemschoice(i).itemid() : 0)
						        << io::write<NetUInt32>(i < quest.rewarditemschoice_size() ? quest.rewarditemschoice(i).count() : 0);
					}
				}


				const String &name = (quest.name_loc_size() >= localeIndex) ? quest.name_loc(localeIndex - 1) : quest.name();
				const String &detailstext = (quest.detailstext_loc_size() >= localeIndex) ? quest.detailstext_loc(localeIndex - 1) : quest.detailstext();
				const String &objectivestext = (quest.objectivestext_loc_size() >= localeIndex) ? quest.objectivestext_loc(localeIndex - 1) : quest.objectivestext();
				const String &endtext = (quest.endtext_loc_size() >= localeIndex) ? quest.endtext_loc(localeIndex - 1) : quest.endtext();
				
				out_packet
				        << io::write<NetUInt32>(quest.pointmapid())
				        << io::write<float>(quest.pointx())
				        << io::write<float>(quest.pointy())
				        << io::write<NetUInt32>(quest.pointopt())
				        << io::write_range(name.empty() ? quest.name() : name) << io::write<NetUInt8>(0)
				        << io::write_range(detailstext.empty() ? quest.detailstext() : detailstext) << io::write<NetUInt8>(0)
				        << io::write_range(objectivestext.empty() ? quest.objectivestext() : objectivestext) << io::write<NetUInt8>(0)
				        << io::write_range(endtext.empty() ? quest.endtext() : endtext) << io::write<NetUInt8>(0);
				for (int i = 0; i < 4; ++i)
				{
					if (i < quest.requirements_size())
					{
						const auto &req = quest.requirements(i);
						if (req.objectid() != 0)
						{
							out_packet
							        << io::write<NetUInt32>(req.objectid() | 0x80000000)
							        << io::write<NetUInt32>(req.objectcount());
						}
						else
						{
							out_packet
							        << io::write<NetUInt32>(req.creatureid())
							        << io::write<NetUInt32>(req.creaturecount());
						}
						out_packet
						        << io::write<NetUInt32>(req.itemid())
						        << io::write<NetUInt32>(req.itemcount());
					}
					else
					{
						out_packet
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0)
						        << io::write<NetUInt32>(0);
					}
				}
				for (int i = 0; i < 4; ++i)
				{
					if (i < quest.requirements_size()) {
						out_packet << io::write_range(quest.requirements(i).text());
					}
					out_packet << io::write<NetUInt8>(0);
				}
				out_packet.finish();
			}

			void gossipComplete(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::GossipComplete);
				out_packet.finish();
			}

			void questgiverStatusMultiple(game::OutgoingPacket &out_packet, const std::map<UInt64, game::QuestgiverStatus> &status)
			{
				out_packet.start(game::server_packet::QuestgiverStatusMultiple);
				out_packet
				        << io::write<NetUInt32>(status.size());
				for (const auto &pair : status)
				{
					out_packet
					        << io::write<NetUInt64>(pair.first)
					        << io::write<NetUInt8>(pair.second);
				}
				out_packet.finish();
			}

			void questgiverRequestItems(game::OutgoingPacket &out_packet, Int32 localeIndex, UInt64 guid, bool closeOnCancel, bool enableNext, const proto::ItemManager &items, const proto::QuestEntry &quest)
			{
				const String &name = (quest.name_loc_size() >= localeIndex) ? quest.name_loc(localeIndex - 1) : quest.name();
				const String &requesttext = (quest.requestitemstext_loc_size() >= localeIndex) ? quest.requestitemstext_loc(localeIndex - 1) : quest.requestitemstext();

				out_packet.start(game::server_packet::QuestgiverRequestItems);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt32>(quest.id())
				        << io::write_range(name.empty() ? quest.name() : name) << io::write<NetUInt8>(0)
				        << io::write_range(requesttext.empty() ? quest.requestitemstext() : requesttext) << io::write<NetUInt8>(0)
				        << io::write<NetUInt32>(0)		// Unknown
				        << io::write<NetUInt32>(0)		// Emote
				        << io::write<NetUInt32>(closeOnCancel ? 1 : 0)
				        << io::write<NetUInt32>(0)		// Unknown
				        << io::write<NetUInt32>(0);		// Required money
				const size_t itemCountPos = out_packet.sink().position();
				out_packet
				        << io::write<NetUInt32>(quest.requirements_size());
				int realCount = quest.requirements_size();
				for (const auto &req : quest.requirements())
				{
					if (!req.itemid())
					{
						realCount--;
						continue;
					}

					const auto *item = items.getById(req.itemid());
					out_packet
					        << io::write<NetUInt32>(req.itemid())
					        << io::write<NetUInt32>(req.itemcount())
					        << io::write<NetUInt32>(item ? item->displayid() : 0);
				}
				// Overwrite real number of items if not matching
				if (realCount != quest.requirements_size()) {
					out_packet.sink().overwrite(itemCountPos, reinterpret_cast<const char *>(&realCount), sizeof(int));
				}

				out_packet
				        << io::write<NetUInt32>(enableNext ? 3 : 0)
				        << io::write<NetUInt32>(0x04)				// Unknown
				        << io::write<NetUInt32>(0x08)				// Unknown
				        << io::write<NetUInt32>(0x10);				// Unknown
				out_packet.finish();
			}
			void questgiverOfferReward(game::OutgoingPacket &out_packet, Int32 localeIndex, UInt64 guid, bool enableNext, const proto::ItemManager &items, const proto::QuestEntry &quest)
			{
				const String &name = (quest.name_loc_size() >= localeIndex) ? quest.name_loc(localeIndex - 1) : quest.name();
				const String &rewardtext = (quest.offerrewardtext_loc_size() >= localeIndex) ? quest.offerrewardtext_loc(localeIndex - 1) : quest.offerrewardtext();
	
				out_packet.start(game::server_packet::QuestgiverOfferReward);
				out_packet
				        << io::write<NetUInt64>(guid)
				        << io::write<NetUInt32>(quest.id())
				        << io::write_range(name.empty() ? quest.name() : name) << io::write<NetUInt8>(0)
				        << io::write_range(rewardtext.empty() ? quest.offerrewardtext() : rewardtext) << io::write<NetUInt8>(0)
				        << io::write<NetUInt32>(enableNext ? 1 : 0)
				        << io::write<NetUInt32>(0)		// Unknown
				        << io::write<NetUInt32>(0)		// Emote count
				        << io::write<NetUInt32>(quest.rewarditemschoice_size());
				for (const auto &rew : quest.rewarditemschoice())
				{
					const auto *item = items.getById(rew.itemid());
					out_packet
					        << io::write<NetUInt32>(rew.itemid())
					        << io::write<NetUInt32>(rew.count())
					        << io::write<NetUInt32>(item ? item->displayid() : 0);
				}
				out_packet
				        << io::write<NetUInt32>(quest.rewarditems_size());
				for (const auto &rew : quest.rewarditems())
				{
					const auto *item = items.getById(rew.itemid());
					out_packet
					        << io::write<NetUInt32>(rew.itemid())
					        << io::write<NetUInt32>(rew.count())
					        << io::write<NetUInt32>(item ? item->displayid() : 0);
				}
				out_packet
				        << io::write<NetUInt32>(quest.rewardmoney())
				        << io::write<NetUInt32>(quest.rewardhonorkills())
				        << io::write<NetUInt32>(0x08)				// Unknown
				        << io::write<NetUInt32>(quest.rewardspell())
				        << io::write<NetUInt32>(quest.rewardspellcast())
				        << io::write<NetUInt32>(0)					// Unknown
				        ;
				out_packet.finish();
			}
			void questgiverQuestComplete(game::OutgoingPacket &out_packet, bool isMaxLevel, UInt32 xp, const proto::QuestEntry &quest)
			{
				out_packet.start(game::server_packet::QuestgiverQuestComplete);
				out_packet
				        << io::write<NetUInt32>(quest.id())
				        << io::write<NetUInt32>(0x03);
				if (!isMaxLevel)
				{
					out_packet
					        << io::write<NetUInt32>(xp)
					        << io::write<NetUInt32>(quest.rewardmoney());
				}
				else
				{
					out_packet
					        << io::write<NetUInt32>(0)
					        << io::write<NetUInt32>(quest.rewardmoney() + quest.rewardmoneymaxlevel());
				}
				out_packet
				        << io::write<NetUInt32>(0)				// Unknown
				        << io::write<NetUInt32>(quest.rewarditems_size());
				for (const auto &rew : quest.rewarditems())
				{
					out_packet
					        << io::write<NetUInt32>(rew.itemid())
					        << io::write<NetUInt32>(rew.count());
				}
				out_packet.finish();
			}
			void questlogFull(game::OutgoingPacket &out_packet)
			{
				out_packet.start(game::server_packet::QuestlogFull);
				out_packet.finish();
			}
			void questupdateAddKill(game::OutgoingPacket &out_packet, UInt32 questId, UInt32 entry, UInt32 totalCount, UInt32 maxCount, UInt64 guid)
			{
				out_packet.start(game::server_packet::QuestupdateAddKill);
				out_packet
				        << io::write<NetUInt32>(questId)
				        << io::write<NetUInt32>(entry)
				        << io::write<NetUInt32>(totalCount)
				        << io::write<NetUInt32>(maxCount)
				        << io::write<NetUInt64>(guid)
				        ;
				out_packet.finish();
			}
			void setFlatSpellModifier(game::OutgoingPacket & out_packet, UInt8 bit, UInt8 modOp, Int32 value)
			{
				out_packet.start(game::server_packet::SetFlatSpellModifier);
				out_packet
					<< io::write<NetUInt8>(bit)
					<< io::write<NetUInt8>(modOp)
					<< io::write<NetInt32>(value)
					;
				out_packet.finish();
			}
			void setPctSpellModifier(game::OutgoingPacket & out_packet, UInt8 bit, UInt8 modOp, Int32 value)
			{
				out_packet.start(game::server_packet::SetPctSpellModifier);
				out_packet
					<< io::write<NetUInt8>(bit)
					<< io::write<NetUInt8>(modOp)
					<< io::write<NetInt32>(value)
					;
				out_packet.finish();
			}

			void spellDelayed(game::OutgoingPacket &out_packet, UInt64 guid, UInt32 delayTimeMS)
			{
				out_packet.start(game::server_packet::SpellDelayed);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(delayTimeMS)
					;
				out_packet.finish();
			}

			void sellItem(game::OutgoingPacket & out_packet, SellError errorCode, UInt64 vendorGuid, UInt64 itemGuid, UInt32 param)
			{
				out_packet.start(game::server_packet::SellItem);
				out_packet
					<< io::write<NetUInt64>(vendorGuid)
					<< io::write<NetUInt64>(itemGuid);
				if (param > 0)
				{
					out_packet << io::write<NetUInt32>(param);
				}
				out_packet
					<< io::write<NetUInt8>(errorCode)
					;
				out_packet.finish();
			}
			void questupdateComplete(game::OutgoingPacket & out_packet, UInt32 questId)
			{
				out_packet.start(game::server_packet::QuestupdateComplete);
				out_packet
					<< io::write<NetUInt64>(questId);
				out_packet.finish();
			}
			void moveTimeSkipped(game::OutgoingPacket & out_packet, UInt64 guid, UInt32 timeSkipped)
			{
				out_packet.start(game::server_packet::MoveTimeSkipped);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(timeSkipped);
				out_packet.finish();
			}

			void whoRequestResponse(game::OutgoingPacket &out_packet, const game::WhoResponse &response, UInt32 matchcount)
			{
				out_packet.start(game::server_packet::WhoResponse);
				out_packet
					<< io::write<NetUInt32>(matchcount)						// Match count
					<< io::write<NetUInt32>(response.entries.size());		// Display count
				for (const auto &entry : response.entries)
				{
					out_packet
						<< io::write_range(entry.name) << io::write<NetUInt8>(0)
						<< io::write_range(entry.guild) << io::write<NetUInt8>(0)
						<< io::write<NetUInt32>(entry.level)
						<< io::write<NetUInt32>(entry.class_)
						<< io::write<NetUInt32>(entry.race)
						<< io::write<NetUInt8>(entry.gender)
						<< io::write<NetUInt32>(entry.zone);
				}
				out_packet.finish();
			}

			void spellLogMiss(game::OutgoingPacket & out_packet, UInt32 spellID, UInt64 caster, UInt8 unknown, const std::map<UInt64, game::SpellMissInfo>& missedTargetGUIDs)
			{
				out_packet.start(game::server_packet::SpellLogMiss);
				out_packet
					<< io::write<NetUInt32>(spellID)
					<< io::write<NetUInt64>(caster)
					<< io::write<NetUInt8>(unknown)
					<< io::write<NetUInt32>(missedTargetGUIDs.size());
				for (auto &pair : missedTargetGUIDs)
				{
					out_packet
						<< io::write<NetUInt64>(pair.first)
						<< io::write<NetUInt8>(pair.second);
				}
				out_packet.finish();
			}

			void sendTradeStatus(game::OutgoingPacket &out_packet, UInt32 status, UInt64 guid, bool sendError/* = false*/, UInt32 errorCode/* = 0*/, UInt32 itemCategoryEntry/* = 0*/)
			{
				out_packet.start(game::server_packet::TradeStatus);
				out_packet << io::write<NetUInt32>(status);
				switch (status)
				{
					case game::trade_status::BeginTrade:
						out_packet << io::write<NetUInt64>(guid);
						break;
					case game::trade_status::OpenWindow:
						out_packet << io::write<NetUInt32>(0);
						break;
					case game::trade_status::CloseWindow:
						out_packet 
							<< io::write<NetUInt32>(errorCode)				// Error-Code
							<< io::write<NetUInt8>(sendError ? 1 : 0)		// Display error code? (1:0)
							<< io::write<NetUInt32>(itemCategoryEntry);		// ItemLimitCategory dbc entry id in case error code is EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED
						break;
					case game::trade_status::WrongRealm:
						out_packet << io::write<NetUInt8>(0);
						break;
					default:
						break;
				}
				out_packet.finish();
			}

			void sendUpdateTrade(game::OutgoingPacket &out_packet,
					     UInt8 state,
					     UInt32 tradeID,
					     UInt32 next_slot,
					     UInt32 prev_slot,
					     UInt32 gold,
					     UInt32 spell,
						 std::array<std::shared_ptr<GameItem>, 7> items)
			{
				out_packet.start(game::server_packet::TradeStatusExtended);
				out_packet 
					<< io::write<NetUInt8>(state)
					<< io::write<NetUInt32>(tradeID)
					<< io::write<NetUInt32>(next_slot)
					<< io::write<NetUInt32>(prev_slot)
					<< io::write<NetUInt32>(gold)
					<< io::write<NetUInt32>(spell);
				for (UInt8 i = 0; i < items.size(); i++)
				{
					out_packet << io::write<NetUInt8>(i);

					auto item = items[i];
					if (item)
					{
						auto const &entry = item->getEntry();
						out_packet << io::write<NetUInt32>(entry.id());
						out_packet << io::write<NetUInt32>(entry.displayid());
						out_packet << io::write<NetUInt32>(item->getStackCount());
						out_packet << io::write<NetUInt32>(item->hasFlag(ItemFields::Flags, ItemFlags::Wrapped) ? 1 : 0 );
						out_packet << io::write<NetUInt64>(item->getUInt32Value(ItemFields::GiftCreator));
						out_packet << io::write<NetUInt32>(item->getUInt32Value(ItemFields::Enchantment));

						// Sockets
						for (UInt32 enchant_slot = 2; enchant_slot < 2 + 3; ++enchant_slot)
						{
							out_packet << io::write<NetUInt32>(item->getUInt32Value(ItemEnchantmentType(enchant_slot))); //TODO
						}

						out_packet << io::write<NetUInt64>(item->getUInt32Value(ItemFields::Creator));
						out_packet << io::write<NetUInt32>(item->getUInt32Value(ItemFields::SpellCharges)); //spell charges
						out_packet << io::write<NetUInt32>(entry.randomsuffix()); //suffix
						out_packet << io::write<NetUInt32>(item->getUInt32Value(ItemFields::RandomPropertiesID));
						out_packet << io::write<NetUInt32>(entry.lockid());
						out_packet << io::write<NetUInt32>(item->getUInt32Value(ItemFields::MaxDurability));
						out_packet << io::write<NetUInt32>(item->getUInt32Value(ItemFields::Durability));
					}
					else
					{
						// Write empty fields
						for (UInt8 j = 0; j < 18; ++j)
							out_packet << io::write<NetUInt32>(0);
					}
				}
				out_packet.finish();
			}

			void petNameQueryResponse(game::OutgoingPacket & out_packet, UInt32 petNumber, const String & petName, UInt32 petNameTimestmap)
			{
				out_packet.start(game::server_packet::PetNameQueryResponse);
				out_packet
					<< io::write<NetUInt32>(petNumber)
					<< io::write_range(petName) << io::write<NetUInt8>(0)
					<< io::write<NetUInt32>(petNameTimestmap)
					<< io::write<NetUInt8>(0)
					;
				out_packet.finish();
			}
			void petSpells(game::OutgoingPacket & out_packet, UInt64 petGUID)
			{
				out_packet.start(game::server_packet::PetSpells);
				out_packet
					<< io::write<NetUInt64>(petGUID);
				const std::vector<UInt8> data =
				{
					0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
					0x02, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x07, 0x26, 0x0C, 0x00, 0x81,
					0x00, 0x00, 0x00, 0x81, 0x00, 0x00, 0x00, 0x81, 0x00, 0x00, 0x00, 0x81, 0x02, 0x00, 0x00, 0x06,
					0x01, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x06, 0x06, 0x26, 0x0C, 0x00, 0xC1, 0x28, 0x49, 0x00,
					0x00, 0x31, 0x49, 0x00, 0x00, 0x71, 0x8B, 0x00, 0x00, 0x34, 0x49, 0x00, 0x00, 0x6F, 0x8B, 0x00,
					0x00, 0x00
				};
				out_packet << io::write_range(data);
				out_packet.finish();
			}

			void mailListResult(game::OutgoingPacket & out_packet, std::list<Mail> mails)
			{
				out_packet.start(game::server_packet::MailListResult);

				out_packet
					<< io::write<NetUInt8>(mails.size());

				if (mails.empty())
				{
					out_packet
						<< io::write<NetUInt8>(0);
					out_packet.finish();
				}
				else
				{
					for (auto &mail : mails)
					{
						std::vector<std::shared_ptr<GameItem>> items = mail.getItems();
						UInt8 messageType = mail.getMessageType();
						UInt32 mailId = mail.getMailId();

						size_t sizePos = out_packet.sink().position();
						out_packet
							// Placeholder for mailSize
							<< io::write<NetUInt16>(0)
							<< io::write<NetUInt32>(mailId)
							<< io::write<NetUInt8>(messageType);
						
						switch (messageType)
						{
							case mail::message_type::Normal:
								out_packet << io::write<NetUInt64>(mail.getSenderGuid());
								break;
							case mail::message_type::Creature:
							case mail::message_type::GameObject:
							case mail::message_type::Auction:
								// TODO handle these (NetUInt32)
								break;
							case mail::message_type::Item:
								out_packet << io::write<NetUInt32>(0);
								break;
							default:
								out_packet << io::write<NetUInt32>(0);
								break;
						}

						out_packet
							<< io::write<NetUInt32>(mail.getCOD())
							// TODO letter item id (sending mailId for body, client stores them based on id)
							<< io::write<NetUInt32>(mailId)
							// unknown
							<< io::write<NetUInt32>(0)
							<< io::write<NetUInt32>(mail.getStationery())
							<< io::write<NetUInt32>(mail.getMoney())
							<< io::write<NetUInt32>(mail.getCheckMasks())
							// TODO time until expires
							<< io::write<float>(30.0f)
							// TOOD mail template from dbc
							<< io::write<NetUInt32>(0)
							<< io::write_range(mail.getSubject()) << io::write<NetUInt8>(0)
							// TODO handle items sent
							<< io::write<NetUInt8>(0);
						//	<< io::write<NetUInt8>(items.size());

						UInt16 mailSize = static_cast<UInt16>(out_packet.sink().position() - sizePos);
						out_packet.writePOD(sizePos, mailSize);
						out_packet.finish();
					}
				}
			}

			void mailSendResult(game::OutgoingPacket & out_packet, MailResult mailResult)
			{
				out_packet.start(game::server_packet::MailSendResult);

				out_packet
					<< io::write<NetUInt32>(mailResult.mailId)
					<< io::write<NetUInt32>(mailResult.mailAction)
					<< io::write<NetUInt32>(mailResult.mailError);

				if (mailResult.mailError == mail::response_result::Equip)
				{
					out_packet
						<< io::write<NetUInt32>(mailResult.equipError);
				}
				else if (mailResult.mailAction == mail::response_type::ItemTaken)
				{
					out_packet
						<< io::write<NetUInt32>(mailResult.itemGuid)
						<< io::write<NetUInt32>(mailResult.itemsCount);
				}

				out_packet.finish();
			}

			void mailQueryNextTime(game::OutgoingPacket & out_packet, UInt32 unreadMails, std::list<Mail> & mails)
			{
				out_packet.start(game::server_packet::MailQueryNextTime);
				if (unreadMails > 0)
				{
					out_packet
						<< io::write<NetUInt32>(0);
						// Placeholder for mail count
					const UInt32 mailCountPos = out_packet.sink().position();
					out_packet
						<< io::write<NetUInt32>(0);

					UInt32 mailCount = 0;

					for (auto &mail : mails)
					{
						if (mail.hasCheckMask(mail::check_mask::Read))
						{
							continue;
						}

						out_packet
							<< io::write<NetUInt64>(mail.getSenderGuid())
							// TODO: check mail type
							<< io::write<NetUInt32>(0)
							<< io::write<NetUInt32>(0)
							// TODO stationery mail (GM, auction, etc, 41 is default)
							<< io::write<NetUInt32>(41)
							// unknown constant 
							<< io::write<NetUInt32>(0xC6000000);

						mailCount++;
						if (mailCount == 2)
						{
							break;
						}
					}
					out_packet.writePOD(mailCountPos, mailCount);
				}
				else
				{
					out_packet
						// unknown constant 
						<< io::write<NetUInt32>(0xC7A8C000)
						<< io::write<NetUInt32>(0);
				}

				out_packet.finish();
			}

			void mailReceived(game::OutgoingPacket & out_packet, UInt32 mailId)
			{
				out_packet.start(game::server_packet::MailReceived);
				out_packet
					<< io::write<NetUInt32>(mailId);
				out_packet.finish();
			}

			void mailSendBody(game::OutgoingPacket & out_packet, UInt32 mailTextId, const String & body)
			{
				out_packet.start(game::server_packet::MailSendBody);
				out_packet
					<< io::write<NetUInt32>(mailTextId)
					<< io::write_range(body)
					<< io::write<NetUInt8>(0);
				out_packet.finish();
			}

			void moveSetCanFly(game::OutgoingPacket & out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::MoveSetCanFly);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void moveUnsetCanFly(game::OutgoingPacket & out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::MoveUnsetCanFly);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void moveFeatherFall(game::OutgoingPacket & out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::MoveFeatherFall);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void moveNormalFall(game::OutgoingPacket & out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::MoveNormalFall);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(counter);
				out_packet.finish();
			}
			
			void moveSetHover(game::OutgoingPacket & out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::MoveSetHover);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void moveUnsetHover(game::OutgoingPacket & out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::MoveUnsetHover);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void moveWaterWalk(game::OutgoingPacket & out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::MoveWaterWalk);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void moveLandWalk(game::OutgoingPacket & out_packet, UInt64 guid, UInt32 counter)
			{
				out_packet.start(game::server_packet::MoveLandWalk);
				out_packet
					<< io::write_packed_guid(guid)
					<< io::write<NetUInt32>(counter);
				out_packet.finish();
			}

			void lootStartRoll(game::OutgoingPacket & out_packet, UInt64 itemGuid, UInt32 playerCount, UInt32 itemEntry, UInt32 itemSuffix, UInt32 itemPropId, UInt32 countdown)
			{
				out_packet.start(game::server_packet::MoveLandWalk);
				out_packet
					<< io::write<NetUInt64>(itemGuid)
					<< io::write<NetUInt32>(playerCount)
					<< io::write<NetUInt32>(itemEntry)
					<< io::write<NetUInt32>(itemSuffix)
					<< io::write<NetUInt32>(itemPropId)
					<< io::write<NetUInt32>(countdown);
				out_packet.finish();
			}

			void playedTime(game::OutgoingPacket & out_packet, UInt32 totalTimeInSecs, UInt32 levelTimeInSecs)
			{
				out_packet.start(game::server_packet::PlayedTime);
				out_packet
					<< io::write<NetUInt32>(totalTimeInSecs)
					<< io::write<NetUInt32>(levelTimeInSecs);
				out_packet.finish();
			}

			void resurrectRequest(game::OutgoingPacket & out_packet, UInt64 objectGUID, const String &sentName, UInt8 typeId)
			{
				out_packet.start(game::server_packet::ResurrectRequest);
				out_packet
					<< io::write<NetUInt64>(objectGUID)
					<< io::write<NetUInt32>(static_cast<UInt32>(sentName.length() + 1))
					<< io::write_range(sentName)
					<< io::write<NetUInt8>(0)
					<< io::write<NetUInt8>(typeId == object_type::Character ? 0 : 1);
				out_packet.finish();
			}

			void channelStart(game::OutgoingPacket & out_packet, UInt64 casterGUID, UInt32 spellId, Int32 duration)
			{
				out_packet.start(game::server_packet::ChannelStart);
				out_packet
					<< io::write_packed_guid(casterGUID)
					<< io::write<NetUInt32>(spellId)
					<< io::write<NetInt32>(duration);

				out_packet.finish();
			}

			void channelUpdate(game::OutgoingPacket & out_packet, UInt64 casterGUID, Int32 castTime)
			{
				out_packet.start(game::server_packet::ChannelUpdate);
				out_packet
					<< io::write_packed_guid(casterGUID)
					<< io::write<NetInt32>(castTime);

				out_packet.finish();
			}

			void moveKnockBack(game::OutgoingPacket & out_packet, UInt64 targetGUID, float vcos, float vsin, float speedxy, float speedz)
			{
				out_packet.start(game::server_packet::MoveKnockBack);
				out_packet
					<< io::write_packed_guid(targetGUID)
					<< io::write<NetUInt32>(0)
					<< io::write<float>(vcos)
					<< io::write<float>(vsin)
					<< io::write<float>(speedxy)
					<< io::write<float>(-speedz);

				out_packet.finish();
			}
			void moveKnockBackWithInfo(game::OutgoingPacket & out_packet, UInt64 targetGUID, const MovementInfo & movementInfo)
			{
				out_packet.start(game::server_packet::MoveKnockBack2);
				out_packet
					<< io::write_packed_guid(targetGUID)
					<< movementInfo
					<< io::write<float>(movementInfo.jumpSinAngle)
					<< io::write<float>(movementInfo.jumpCosAngle)
					<< io::write<float>(movementInfo.jumpXYSpeed)
					<< io::write<float>(movementInfo.jumpVelocity);
				out_packet.finish();
			}
			void itemNameQueryResponse(game::OutgoingPacket & out_packet, UInt32 entryId, const String & name, UInt32 inventoryType)
			{
				out_packet.start(game::server_packet::ItemNameQueryResponse);
				out_packet
					<< io::write<NetUInt32>(entryId)
					<< io::write_range(name) << io::write<NetUInt8>(0)
					<< io::write<NetUInt32>(inventoryType);
				out_packet.finish();
			}
			void notification(game::OutgoingPacket & out_packet, const String & message)
			{
				out_packet.start(game::server_packet::Notification);
				out_packet
					<< io::write_range(message) << io::write<NetUInt8>(0);
				out_packet.finish();
			}
		}

		namespace client_read
		{
			bool ping(io::Reader &packet, UInt32 &out_ping, UInt32 &out_latency)
			{
				return packet
				       >> io::read<NetUInt32>(out_ping)
				       >> io::read<NetUInt32>(out_latency);
			}

			bool optOutOfLoot(io::Reader & packet, bool & out_enable)
			{
				return packet >> io::read<NetUInt32>(out_enable);
			}

			bool authSession(io::Reader &packet, UInt32 &out_clientBuild, String &out_Account, UInt32 &out_clientSeed, SHA1Hash &out_digest, game::AddonEntries &out_addons)
			{
				UInt32 unknown;

				if (!(packet
				        >> io::read<NetUInt32>(out_clientBuild)
				        >> io::read<NetUInt32>(unknown)))
				{
					return false;
				}

				// Read account name
				out_Account.clear();
				if (!(packet
					>> io::read_string(out_Account)))
				{
					return false;
				}

				UInt32 addonSize;
				if (!(packet
				        >> io::read<NetUInt32>(out_clientSeed)
				        >> io::read_range(out_digest.begin(), out_digest.end())
				        >> io::read<NetUInt32>(addonSize)))
				{
					return false;
				}

				// Validate addon size
				if (addonSize == 0 || addonSize > 0xFFFFF)
				{
					return false;
				}

				// Get remaining packet size
				const size_t remainingSize = packet.getSource()->size() - packet.getSource()->position();

				// Create buffer
				String buffer;
				buffer.resize(remainingSize);
				packet.getSource()->read(&buffer[0], remainingSize);
				std::istringstream inStrm(buffer);

				// Uncompress using ZLib
				boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
				in.push(boost::iostreams::zlib_decompressor());
				in.push(inStrm);

				std::stringstream outStrm;
				boost::iostreams::copy(in, outStrm);
				outStrm.seekg(0);

				io::StreamSource source(outStrm);
				io::Reader addonReader(source);

				while (!source.end())
				{
					game::AddonEntry addon;

					// Read other addon values
					if (!(addonReader
							>> io::read_string(addon.addonNames)
					        >> io::read<NetUInt32>(addon.crc)
					        >> io::read<NetUInt32>(addon.unk7)
					        >> io::read<NetUInt8>(addon.unk6)))
					{
						break;
					}

					out_addons.push_back(addon);
				}

				return packet;
			}

			bool charEnum(io::Reader &packet)
			{
				// This is an empty packet
				return packet;
			}

			bool charCreate(io::Reader &packet, game::CharEntry &out_entry)
			{
				// Read 0-terminated name
				out_entry.name.clear();
				if (!(packet
						>> io::read_string(out_entry.name)
				        >> io::read<NetUInt8>(out_entry.race)
				        >> io::read<NetUInt8>(out_entry.class_)
				        >> io::read<NetUInt8>(out_entry.gender)
				        >> io::read<NetUInt8>(out_entry.skin)
				        >> io::read<NetUInt8>(out_entry.face)
				        >> io::read<NetUInt8>(out_entry.hairStyle)
				        >> io::read<NetUInt8>(out_entry.hairColor)
				        >> io::read<NetUInt8>(out_entry.facialHair)
				        >> io::read<NetUInt8>(out_entry.outfitId)))
				{
					return false;
				}

				return packet;
			}

			bool charDelete(io::Reader &packet, DatabaseId &out_characterId)
			{
				return packet
				       >> io::read<NetUInt64>(out_characterId);
			}

			bool nameQuery(io::Reader &packet, NetUInt64 &out_objectId)
			{
				return packet
				       >> io::read<NetUInt64>(out_objectId);
			}

			bool playerLogin(io::Reader &packet, DatabaseId &out_characterId)
			{
				return packet
				       >> io::read<NetUInt64>(out_characterId);
			}

			bool creatureQuery(io::Reader &packet, UInt32 &out_entry, UInt64 &out_guid)
			{
				return packet
				       >> io::read<NetUInt32>(out_entry)
				       >> io::read<NetUInt64>(out_guid);
			}

			bool playerLogout(
			    io::Reader &packet
			)
			{
				return packet;
			}

			bool logoutRequest(
			    io::Reader &packet
			)
			{
				return packet;
			}

			bool logoutCancel(
			    io::Reader &packet
			)
			{
				return packet;
			}

			bool moveStop(io::Reader &packet, MovementInfo &out_info)
			{
				return packet
				       >> out_info;
			}

			bool moveStartForward(io::Reader &packet, MovementInfo &out_info)
			{
				return packet
				       >> out_info;
			}

			bool moveStartBackward(io::Reader &packet, MovementInfo &out_info)
			{
				return packet
				       >> out_info;
			}

			bool moveHeartBeat(io::Reader &packet, MovementInfo &out_info)
			{
				return packet
				       >> out_info;
			}

			bool setSelection(io::Reader &packet, UInt64 &out_targetGUID)
			{
				return packet
				       >> io::read<NetUInt64>(out_targetGUID);
			}

			bool minimapPing(io::Reader & packet, float & out_x, float & out_y)
			{
				return packet
					>> io::read<float>(out_x)
					>> io::read<float>(out_y);
			}

			bool standStateChange(io::Reader &packet, UnitStandState &out_standState)
			{
				return packet
				       >> io::read<NetUInt32>(out_standState);
			}

			bool messageChat(io::Reader &packet, game::ChatMsg &out_type, game::Language &out_lang, String &out_to, String &out_channel, String &out_message)
			{
				packet
				        >> io::read<NetUInt32>(out_type)
				        >> io::read<NetUInt32>(out_lang);

				// TODO: Prevent player to speak in unknown language
				switch (out_type)
				{
				case game::chat_msg::Say:
				case game::chat_msg::Emote:
				case game::chat_msg::Yell:
				case game::chat_msg::Party:
				case game::chat_msg::Guild:
				case game::chat_msg::Officer:
				case game::chat_msg::Raid:
				case game::chat_msg::RaidLeader:
				case game::chat_msg::RaidWarning:
				case game::chat_msg::Battleground:
				case game::chat_msg::BattlegroundLeader:
				case game::chat_msg::Afk:
				case game::chat_msg::Dnd:
					{
						if (!(packet >> io::read_string(out_message)))
						{
							return false;
						}
						break;
					}
				case game::chat_msg::Whisper:
					{
						if (!(packet 
							>> io::read_string(out_to)
							>> io::read_string(out_message)))
						{
							return false;
						}
						break;
					}
				case game::chat_msg::Channel:
					{
						if (!(packet
							>> io::read_string(out_channel)
							>> io::read_string(out_message)))
						{
							return false;
						}
						break;
					}
				default:
					// Unknown / Unhandled chat type
					return false;
				}

				return packet;
			}

			bool contactList(io::Reader &packet)
			{
				// Skip the next four bytes as they don't seem to be important...
				packet.skip(4);
				return packet;
			}

			bool addFriend(io::Reader &packet, String &out_name, String &out_note)
			{
				return packet
					>> io::read_string(out_name)
					>> io::read_string(out_note);
			}

			bool deleteFriend(io::Reader &packet, UInt64 &out_guid)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid);
			}

			bool setContactNotes(io::Reader &packet /*TODO */)
			{
				//TODO
				return packet;
			}

			bool addIgnore(io::Reader &packet, String &out_name)
			{
				return packet
					>> io::read_string(out_name);
			}

			bool deleteIgnore(io::Reader &packet, UInt64 &out_guid)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid);
			}

			bool castSpell(io::Reader &packet, UInt32 &out_spellID, UInt8 &out_castCount, SpellTargetMap &out_targetMap)
			{
				return packet
				       >> io::read<NetUInt32>(out_spellID)
				       >> io::read<NetUInt8>(out_castCount)
				       >> out_targetMap;
			}

			bool cancelCast(io::Reader &packet, UInt32 &out_spellID)
			{
				return packet
				       >> io::read<NetUInt32>(out_spellID);
			}

			bool itemQuerySingle(io::Reader &packet, UInt32 &out_itemID)
			{
				return packet
				       >> io::read<NetUInt32>(out_itemID);
			}

			bool attackSwing(io::Reader &packet, UInt64 &out_targetGUID)
			{
				return packet
				       >> io::read<NetUInt64>(out_targetGUID);
			}

			bool attackStop(io::Reader &packet)
			{
				return true;
			}

			bool setSheathed(io::Reader &packet, UInt32 &out_sheath)
			{
				return packet
				       >> io::read<NetUInt32>(out_sheath);
			}

			bool togglePvP(io::Reader &packet, UInt8 &out_enabled)
			{
				return packet
				       >> io::read<NetUInt8>(out_enabled);
			}

			bool autoStoreLootItem(io::Reader &packet, UInt8 &out_lootSlot)
			{
				return packet
				       >> io::read<NetUInt8>(out_lootSlot);
			}

			bool autoEquipItem(io::Reader &packet, UInt8 &out_srcBag, UInt8 &out_srcSlot)
			{
				return packet
				       >> io::read<NetUInt8>(out_srcBag)
				       >> io::read<NetUInt8>(out_srcSlot);
			}

			bool autoStoreBagItem(io::Reader &packet, UInt8 &out_srcBag, UInt8 &out_srcSlot, UInt8 &out_dstBag)
			{
				return packet
				       >> io::read<NetUInt8>(out_srcBag)
				       >> io::read<NetUInt8>(out_srcSlot)
				       >> io::read<NetUInt8>(out_dstBag);
			}

			bool swapItem(io::Reader &packet, UInt8 &out_dstBag, UInt8 &out_dstSlot, UInt8 &out_srcBag, UInt8 &out_srcSlot)
			{
				return packet
				       >> io::read<NetUInt8>(out_dstBag)
				       >> io::read<NetUInt8>(out_dstSlot)
				       >> io::read<NetUInt8>(out_srcBag)
				       >> io::read<NetUInt8>(out_srcSlot);
			}

			bool swapInvItem(io::Reader &packet, UInt8 &out_srcSlot, UInt8 &out_dstSlot)
			{
				return packet
				       >> io::read<NetUInt8>(out_srcSlot)
				       >> io::read<NetUInt8>(out_dstSlot);
			}

			bool splitItem(io::Reader &packet, UInt8 &out_srcBag, UInt8 &out_srcSlot, UInt8 &out_dstBag, UInt8 &out_dstSlot, UInt8 &out_count)
			{
				return packet
				       >> io::read<NetUInt8>(out_srcBag)
				       >> io::read<NetUInt8>(out_srcSlot)
				       >> io::read<NetUInt8>(out_dstBag)
				       >> io::read<NetUInt8>(out_dstSlot)
				       >> io::read<NetUInt8>(out_count);
			}

			bool autoEquipItemSlot(io::Reader &packet, UInt64 &out_itemGUID, UInt8 &out_dstSlot)
			{
				return packet
				       >> io::read<NetUInt64>(out_itemGUID)
				       >> io::read<NetUInt8>(out_dstSlot);
			}

			bool destroyItem(io::Reader &packet, UInt8 &out_bag, UInt8 &out_slot, UInt8 &out_count, UInt8 &out_data1, UInt8 &out_data2, UInt8 &out_data3)
			{
				return packet
				       >> io::read<NetUInt8>(out_bag)
				       >> io::read<NetUInt8>(out_slot)
				       >> io::read<NetUInt8>(out_count)
				       >> io::read<NetUInt8>(out_data1)
				       >> io::read<NetUInt8>(out_data2)
				       >> io::read<NetUInt8>(out_data3);
			}

			bool groupInvite(io::Reader &packet, String &out_memberName)
			{
				return packet
					>> io::read_string(out_memberName);
			}

			bool groupAccept(io::Reader &packet)
			{
				return packet;
			}

			bool groupDecline(io::Reader &packet)
			{
				return packet;
			}

			bool groupUninvite(io::Reader &packet, String &out_memberName)
			{
				return packet
					>> io::read_string(out_memberName);
			}

			bool groupUninviteGUID(io::Reader &packet, UInt64 &out_guid)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid);
			}

			bool groupSetLeader(io::Reader &packet, UInt64 &out_leaderGUID)
			{
				return packet
				       >> io::read<NetUInt64>(out_leaderGUID);
			}

			bool lootMethod(io::Reader &packet, UInt32 &out_method, UInt64 &out_masterGUID, UInt32 &out_treshold)
			{
				return packet
				       >> io::read<NetUInt32>(out_method)
				       >> io::read<NetUInt64>(out_masterGUID)
				       >> io::read<NetUInt32>(out_treshold);
			}

			bool groupDisband(io::Reader &packet)
			{
				return packet;
			}

			bool requestPartyMemberStats(io::Reader &packet, UInt64 &out_GUID)
			{
				return packet
				       >> io::read<NetUInt64>(out_GUID);
			}

			bool moveWorldPortAck(io::Reader &packet)
			{
				return packet;
			}

			bool areaTrigger(io::Reader &packet, UInt32 &out_triggerId)
			{
				return packet
				       >> io::read<NetUInt32>(out_triggerId);
			}

			bool setActionButton(io::Reader &packet, UInt8 &out_button, UInt8 &out_misc, UInt8 &out_type, UInt16 &out_action)
			{
				return packet
				       >> io::read<NetUInt8>(out_button)
				       >> io::read<NetUInt16>(out_action)
				       >> io::read<NetUInt8>(out_misc)
				       >> io::read<NetUInt8>(out_type);
			}

			bool gameObjectQuery(io::Reader &packet, UInt32 &out_entry, UInt64 &out_guid)
			{
				return packet
				       >> io::read<NetUInt32>(out_entry)
				       >> io::read<NetUInt64>(out_guid);
			}

			bool forceMoveRootAck(io::Reader &packet)
			{
				// Skip packet bytes
				packet.skip(packet.getSource()->size());
				return true;
			}

			bool forceMoveUnrootAck(io::Reader &packet)
			{
				// Skip packet bytes
				packet.skip(packet.getSource()->size());
				return true;
			}

			bool cancelAura(io::Reader &packet, UInt32 &out_spellId)
			{
				return packet
				       >> io::read<NetUInt32>(out_spellId);
			}

			bool tutorialFlag(io::Reader &packet, UInt32 &out_flag)
			{
				return packet
				       >> io::read<NetUInt32>(out_flag);
			}

			bool tutorialClear(io::Reader &packet)
			{
				return packet;
			}

			bool tutorialReset(io::Reader &packet)
			{
				return packet;
			}

			bool emote(io::Reader &packet, UInt32 &out_emote)
			{
				return packet
				       >> io::read<NetUInt32>(out_emote);
			}

			bool textEmote(io::Reader &packet, UInt32 &out_textEmote, UInt32 &out_emoteNum, UInt64 &out_guid)
			{
				return packet
				       >> io::read<NetUInt32>(out_textEmote)
				       >> io::read<NetUInt32>(out_emoteNum)
				       >> io::read<NetUInt64>(out_guid);
			}

			bool completeCinematic(io::Reader &packet)
			{
				return true;
			}

			bool repopRequest(io::Reader &packet)
			{
				packet.skip(1);
				return true;
			}

			bool loot(io::Reader &packet, UInt64 &out_targetGuid)
			{
				return packet
				       >> io::read<NetUInt64>(out_targetGuid);
			}

			bool zoneUpdate(io::Reader & packet, UInt32 & out_zoneId)
			{
				return packet
					>> io::read<NetUInt32>(out_zoneId);
			}

			bool lootMoney(io::Reader &packet /*TODO */)
			{
				return true;
			}

			bool lootRelease(io::Reader &packet, UInt64 &out_targetGuid)
			{
				return packet
				       >> io::read<NetUInt64>(out_targetGuid);
			}

			bool timeSyncResponse(io::Reader &packet, UInt32 &out_counter, UInt32 &out_ticks)
			{
				return packet
				       >> io::read<NetUInt32>(out_counter)
				       >> io::read<NetUInt32>(out_ticks);
			}

			bool raidTargetUpdate(io::Reader &packet, UInt8 &out_mode, UInt64 &out_guidOptional)
			{
				if (!(packet
				        >> io::read<NetUInt8>(out_mode)))
				{
					return false;
				}

				if (out_mode != 0xFF)
				{
					return packet
					       >> io::read<NetUInt64>(out_guidOptional);
				}

				return packet;
			}

			bool groupRaidConvert(io::Reader &packet)
			{
				return packet;
			}

			bool groupAssistentLeader(io::Reader &packet, UInt64 &out_guid, UInt8 &out_flag)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid)
				       >> io::read<NetUInt8>(out_flag);
			}

			bool raidReadyCheck(io::Reader &packet, bool &out_hasState, UInt8 &out_state)
			{
				if (packet.getSource()->end())
				{
					out_hasState = false;
					return true;
				}

				out_hasState = true;
				return packet
				       >> io::read<NetUInt8>(out_state);
			}

			bool learnTalent(io::Reader &packet, UInt32 &out_talentId, UInt32 &out_rank)
			{
				return packet
				       >> io::read<NetUInt32>(out_talentId)
				       >> io::read<NetUInt32>(out_rank);
			}

			bool useItem(io::Reader &packet, UInt8 &out_bag, UInt8 &out_slot, UInt8 &out_spellCount, UInt8 &out_castCount, UInt64 &out_itemGuid, SpellTargetMap &out_targetMap)
			{
				return packet
				       >> io::read<NetUInt8>(out_bag)
				       >> io::read<NetUInt8>(out_slot)
				       >> io::read<NetUInt8>(out_spellCount)
				       >> io::read<NetUInt8>(out_castCount)
				       >> io::read<NetUInt64>(out_itemGuid)
				       >> out_targetMap
				       ;
			}

			bool listInventory(io::Reader &packet, UInt64 &out_guid)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid);
			}

			bool sellItem(io::Reader &packet, UInt64 &out_vendorGuid, UInt64 &out_itemGuid, UInt8 &out_count)
			{
				return packet
				       >> io::read<NetUInt64>(out_vendorGuid)
				       >> io::read<NetUInt64>(out_itemGuid)
				       >> io::read<NetUInt8>(out_count);
			}

			bool buyItem(io::Reader &packet, UInt64 &out_vendorGuid, UInt32 &out_item, UInt8 &out_count)
			{
				UInt8 skipped = 0;
				return packet
				       >> io::read<NetUInt64>(out_vendorGuid)
				       >> io::read<NetUInt32>(out_item)
				       >> io::read<NetUInt8>(out_count)
				       >> io::read<NetUInt8>(skipped);
			}

			bool buyItemInSlot(io::Reader &packet, UInt64 &out_vendorGuid, UInt32 &out_item, UInt64 &out_bagGuid, UInt8 &out_slot, UInt8 &out_count)
			{
				return packet
				       >> io::read<NetUInt64>(out_vendorGuid)
				       >> io::read<NetUInt32>(out_item)
				       >> io::read<NetUInt64>(out_bagGuid)
				       >> io::read<NetUInt8>(out_slot)
				       >> io::read<NetUInt8>(out_count);
			}

			bool gossipHello(io::Reader &packet, UInt64 &out_npcGuid)
			{
				return packet
				       >> io::read<NetUInt64>(out_npcGuid);
			}

			bool trainerBuySpell(io::Reader &packet, UInt64 &out_guid, UInt32 &out_spell)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid)
				       >> io::read<NetUInt32>(out_spell);
			}

			bool realmSplit(io::Reader &packet, UInt32 &out_preferredRealm)
			{
				return packet
				       >> io::read<NetUInt32>(out_preferredRealm);
			}

			bool voiceSessionEnable(io::Reader &packet, UInt16 &out_unknown)
			{
				return packet
				       >> io::read<NetUInt16>(out_unknown);
			}

			bool charRename(io::Reader &packet, UInt64 &out_guid, String &out_name)
			{
				packet
					>> io::read<NetUInt64>(out_guid)
					>> io::read_string(out_name);
				
				return packet;
			}

			bool questgiverStatusQuery(io::Reader &packet, UInt64 &out_guid)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid);
			}

			bool questgiverHello(io::Reader &packet, UInt64 &out_guid)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid);
			}

			bool questgiverQueryQuest(io::Reader &packet, UInt64 &out_guid, UInt32 &out_questId)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid)
				       >> io::read<NetUInt32>(out_questId);
			}

			bool questgiverQuestAutolaunch(io::Reader &packet)
			{
				return packet;
			}

			bool questgiverAcceptQuest(io::Reader &packet, UInt64 &out_guid, UInt32 &out_questId)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid)
				       >> io::read<NetUInt32>(out_questId);
			}

			bool questgiverCompleteQuest(io::Reader &packet, UInt64 &out_guid, UInt32 &out_questId)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid)
				       >> io::read<NetUInt32>(out_questId);
			}

			bool questgiverRequestReward(io::Reader &packet, UInt64 &out_guid, UInt32 &out_questId)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid)
				       >> io::read<NetUInt32>(out_questId);
			}

			bool questgiverChooseReward(io::Reader &packet, UInt64 &out_guid, UInt32 &out_questId, UInt32 &out_reward)
			{
				return packet
				       >> io::read<NetUInt64>(out_guid)
				       >> io::read<NetUInt32>(out_questId)
				       >> io::read<NetUInt32>(out_reward);
			}

			bool questgiverCancel(io::Reader &packet)
			{
				return packet;
			}

			bool questQuery(io::Reader &packet, UInt32 &out_questId)
			{
				return packet
				       >> io::read<NetUInt32>(out_questId);
			}

			bool questgiverStatusMultiple(io::Reader &packet)
			{
				return packet;
			}

			bool questlogRemoveQuest(io::Reader &packet, UInt8 &out_questIndex)
			{
				return packet
				       >> io::read<NetUInt8>(out_questIndex);
			}

			bool gameobjectUse(io::Reader &packet, UInt64 &out_guid)
			{
				return packet
					>> io::read<NetUInt64>(out_guid);
			}
			bool openItem(io::Reader & packet, UInt8 & out_bag, UInt8 & out_slot)
			{
				return packet
					>> io::read<NetUInt8>(out_bag)
					>> io::read<NetUInt8>(out_slot);
			}
			bool moveTimeSkipped(io::Reader & packet, UInt64 & out_guid, UInt32 & out_timeSkipped)
			{
				return packet
					>> io::read<NetUInt64>(out_guid)
					>> io::read<NetUInt32>(out_timeSkipped);
			}

			bool who(io::Reader &packet, WhoListRequest &out_whoList)
			{
				return packet
					   >> out_whoList;
			}

			bool initateTrade(io::Reader &packet, UInt64 &guid)
			{
				return packet
					>> io::read<NetUInt64>(guid);
			}

			bool beginTrade(io::Reader &packet)
			{
				return packet;
			}

			bool acceptTrade(io::Reader &packet)
			{
				return packet;
			}

			bool setTradeGold(io::Reader &packet, UInt32 &gold)
			{
				return packet
					>> io::read<NetUInt32>(gold);
			}

			bool setTradeItem(io::Reader &packet, UInt8 &tradeSlot, UInt8 &bag, UInt8 &slot)
			{
				return packet
					>> io::read<NetUInt8>(tradeSlot)
					>> io::read<NetUInt8>(bag)
					>> io::read<NetUInt8>(slot);
			}

			bool clearTradeItem(io::Reader & packet, UInt8 & slot)
			{
				return packet
					>> io::read<NetUInt8>(slot);
			}
			
			bool petNameRequest(io::Reader & packet, UInt32 & out_petNumber, UInt64 & out_petGUID)
			{
				return packet
					>> io::read<NetUInt32>(out_petNumber)
					>> io::read<NetUInt64>(out_petGUID);
			}

			bool setActionBarToggles(io::Reader & packet, UInt8 & out_actionBars)
			{
				return packet
					>> io::read<NetUInt8>(out_actionBars);
			}

			bool mailSend(io::Reader &packet, ObjectGuid &out_mailboxGuid, MailData &out_mail)
			{
				return packet
					>> io::read<NetObjectGuid>(out_mailboxGuid)
					>> out_mail;
			}

			bool mailGetList(io::Reader & packet, ObjectGuid & out_mailboxGuid)
			{
				return packet
					>> io::read<NetUInt64>(out_mailboxGuid);
			}

			bool mailGetBody(io::Reader & packet, UInt32 & out_mailTextId, UInt32 & out_mailId)
			{
				return packet
					>> io::read<NetUInt32>(out_mailTextId)
					>> io::read<NetUInt32>(out_mailId)
					>> io::skip(sizeof(UInt32));
			}

			bool mailMarkAsRead(io::Reader & packet, ObjectGuid & out_mailboxGuid, UInt32 & out_mailId)
			{
				return packet
					>> io::read<NetUInt64>(out_mailboxGuid)
					>> io::read<NetUInt32>(out_mailId);
			}

			bool mailQueryNextTime(io::Reader & packet)
			{
				return packet;
			}

			bool mailTakeMoney(io::Reader & packet, ObjectGuid & out_mailboxGuid, UInt32 & out_mailId)
			{
				return packet
					>> io::read<NetObjectGuid>(out_mailboxGuid)
					>> io::read<NetUInt32>(out_mailId);
			}

			bool mailDelete(io::Reader & packet, ObjectGuid & out_mailboxGuid, UInt32 & out_mailId)
			{
				return packet
					>> io::read<NetObjectGuid>(out_mailboxGuid)
					>> io::read<NetUInt32>(out_mailId)
					>> io::skip(sizeof(UInt32));
			}

			bool resurrectResponse(io::Reader & packet, UInt64 &out_guid, UInt8 &out_status)
			{
				return packet
					>> io::read<NetUInt64>(out_guid)
					>> io::read<NetUInt8>(out_status);
			}

			bool cancelChanneling(io::Reader & packet)
			{
				packet.skip(packet.getSource()->size());

				return packet;
			}
			bool itemNameQuery(io::Reader & packet, UInt32 & out_entry, UInt64 & out_guid)
			{
				return packet
					>> io::read<NetUInt32>(out_entry)
					>> io::read<NetUInt64>(out_guid);
				return false;
			}
			bool repairItem(io::Reader & packet, UInt64 & out_npcGuid, UInt64 & out_itemGuid, UInt8 & out_guildBank)
			{
				return packet
					>> io::read<NetUInt64>(out_npcGuid)
					>> io::read<NetUInt64>(out_itemGuid)
					>> io::read<NetUInt8>(out_guildBank);
			}
			bool buyBackItem(io::Reader & packet, UInt64 & out_vendorGuid, UInt32 & out_slot)
			{
				return packet
					>> io::read<NetUInt64>(out_vendorGuid)
					>> io::read<NetUInt32>(out_slot);
			}
		}

		wowpp::game::WhoResponseEntry::WhoResponseEntry(const GameCharacter & character)
			: level(character.getLevel())
			, class_(character.getClass())
			, race(character.getRace())
			, name(character.getName())
			, zone(character.getZone())
			, gender(character.getGender())
		{
		}

		io::Reader &operator>>(io::Reader &r, WhoListRequest &out_whoList)
		{
			r
				>> io::read<NetUInt32>(out_whoList.level_min)
				>> io::read<NetUInt32>(out_whoList.level_max)
				>> io::read_string(out_whoList.player_name)
				>> io::read_string(out_whoList.guild_name)
				>> io::read<NetUInt32>(out_whoList.racemask)
				>> io::read<NetUInt32>(out_whoList.classmask);
			
			// Read zones
			UInt32 zoneCount = 0;
			r >> io::read<NetUInt32>(zoneCount);
			if (zoneCount > 0)
			{
				out_whoList.zoneids.resize(zoneCount);
				for (auto &zone : out_whoList.zoneids)
				{
					r >> io::read<NetUInt32>(zone);
				}
			}

			// Read strings
			UInt32 stringCount = 0;
			r >> io::read<NetUInt32>(stringCount);
			if (stringCount > 0)
			{
				out_whoList.strings.resize(stringCount);
				for (auto &string : out_whoList.strings)
				{
					r >> io::read_string(string);
				}
			}
			return r;
		}
	}
}
