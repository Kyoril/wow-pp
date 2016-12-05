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
#include "wowpp_editor_team.h"
#include "log/default_log_levels.h"
#include "proto_data/project.h"

namespace wowpp
{
	namespace pp
	{
		namespace editor_team
		{
			static const google::protobuf::Message *getEntry(const proto::Project &project, UInt32 entry, DataEntryType type)
			{
				switch (type)
				{
					case data_entry_type::Spells:
						return project.spells.getById(entry);
					case data_entry_type::Units:
						return project.units.getById(entry);
					case data_entry_type::Objects:
						return project.objects.getById(entry);
					case data_entry_type::Maps:
						return project.maps.getById(entry);
					case data_entry_type::Emotes:
						return project.emotes.getById(entry);
					case data_entry_type::UnitLoot:
						return project.unitLoot.getById(entry);
					case data_entry_type::ObjectLoot:
						return project.objectLoot.getById(entry);
					case data_entry_type::ItemLoot:
						return project.itemLoot.getById(entry);
					case data_entry_type::SkinningLoot:
						return project.skinningLoot.getById(entry);
					case data_entry_type::Skills:
						return project.skills.getById(entry);
					case data_entry_type::Trainers:
						return project.trainers.getById(entry);
					case data_entry_type::Vendors:
						return project.vendors.getById(entry);
					case data_entry_type::Talents:
						return project.talents.getById(entry);
					case data_entry_type::Items:
						return project.items.getById(entry);
					case data_entry_type::ItemSets:
						return project.itemSets.getById(entry);
					case data_entry_type::Classes:
						return project.classes.getById(entry);
					case data_entry_type::Races:
						return project.races.getById(entry);
					case data_entry_type::Levels:
						return project.levels.getById(entry);
					case data_entry_type::Triggers:
						return project.triggers.getById(entry);
					case data_entry_type::Zones:
						return project.zones.getById(entry);
					case data_entry_type::Quests:
						return project.quests.getById(entry);
					case data_entry_type::Factions:
						return project.factions.getById(entry);
					case data_entry_type::FactionTemplates:
						return project.factionTemplates.getById(entry);
					case data_entry_type::AreaTriggers:
						return project.areaTriggers.getById(entry);
					case data_entry_type::SpellCategories:
						return project.spellCategories.getById(entry);
					case data_entry_type::CombatRatings:
						return project.combatRatings.getById(entry);
					case data_entry_type::MeleeCritChance:
						return project.meleeCritChance.getById(entry);
					case data_entry_type::ResistancePercentage:
						return project.resistancePcts.getById(entry);
					case data_entry_type::Variables:
						return project.variables.getById(entry);
				}

				return nullptr;
			}

			static google::protobuf::Message *getOrCreateEntry(proto::Project &project, UInt32 entry, DataEntryType type)
			{
				switch (type)
				{
#define WOWPP_ENTRY_WRAPPER(name, type) \
					case data_entry_type::name: \
					{ \
						auto *e = project.type.getById(entry); \
						if (!e) \
						{ \
							e = project.type.add(entry); \
						} \
						return e; \
					}

					WOWPP_ENTRY_WRAPPER(Spells, spells)
					WOWPP_ENTRY_WRAPPER(Units, units)
					WOWPP_ENTRY_WRAPPER(Objects, objects)
					WOWPP_ENTRY_WRAPPER(Maps, maps)
					WOWPP_ENTRY_WRAPPER(Emotes, emotes)
					WOWPP_ENTRY_WRAPPER(UnitLoot, unitLoot)
					WOWPP_ENTRY_WRAPPER(ObjectLoot, objectLoot)
					WOWPP_ENTRY_WRAPPER(ItemLoot, itemLoot)
					WOWPP_ENTRY_WRAPPER(SkinningLoot, skinningLoot)
					WOWPP_ENTRY_WRAPPER(Skills, skills)
					WOWPP_ENTRY_WRAPPER(Trainers, trainers)
					WOWPP_ENTRY_WRAPPER(Vendors, vendors)
					WOWPP_ENTRY_WRAPPER(Talents, talents)
					WOWPP_ENTRY_WRAPPER(Items, items)
					WOWPP_ENTRY_WRAPPER(ItemSets, itemSets)
					WOWPP_ENTRY_WRAPPER(Classes, classes)
					WOWPP_ENTRY_WRAPPER(Races, races)
					WOWPP_ENTRY_WRAPPER(Levels, levels)
					WOWPP_ENTRY_WRAPPER(Triggers, triggers)
					WOWPP_ENTRY_WRAPPER(Zones, zones)
					WOWPP_ENTRY_WRAPPER(Quests, quests)
					WOWPP_ENTRY_WRAPPER(Factions, factions)
					WOWPP_ENTRY_WRAPPER(FactionTemplates, factionTemplates)
					WOWPP_ENTRY_WRAPPER(AreaTriggers, areaTriggers)
					WOWPP_ENTRY_WRAPPER(SpellCategories, spellCategories)
#undef WOWPP_ENTRY_WRAPPER
				}

				return nullptr;
			}

			namespace editor_write
			{
				void login(pp::OutgoingPacket &out_packet, const String &internalName, const SHA1Hash &password)
				{
					out_packet.start(editor_packet::Login);
					out_packet
					        << io::write<NetUInt32>(ProtocolVersion)
					        << io::write_dynamic_range<NetUInt8>(internalName)
					        << io::write_range<SHA1Hash>(password);
					out_packet.finish();
				}

				void keepAlive(pp::OutgoingPacket &out_packet)
				{
					out_packet.start(editor_packet::KeepAlive);
					// This packet is empty
					out_packet.finish();
				}

				void projectHashMap(pp::OutgoingPacket &out_packet, const std::map<String, String> &hashMap)
				{
					out_packet.start(editor_packet::ProjectHashMap);
					out_packet << io::write<NetUInt32>(hashMap.size());
					for (const auto &pair : hashMap)
					{
						out_packet
							<< io::write_dynamic_range<NetUInt8>(pair.first)
							<< io::write_dynamic_range<NetUInt8>(pair.second);
					}
					out_packet.finish();
				}
				void entryUpdate(pp::OutgoingPacket & out_packet, const std::map<DataEntryType, std::map<UInt32, DataEntryChangeType>>& changes, const proto::Project &project)
				{
					out_packet.start(editor_packet::EntryUpdate);
					out_packet << io::write<NetUInt32>(changes.size());
					for (const auto &pair : changes)
					{
						out_packet
							<< io::write<NetUInt32>(pair.first)
							<< io::write<NetUInt32>(pair.second.size());
						for (const auto &pair2 : pair.second)
						{
							out_packet
								<< io::write<NetUInt32>(pair2.first)
								<< io::write<NetUInt32>(pair2.second);

							// Write data entry if not deleted
							if (pair2.second != data_entry_change_type::Removed)
							{
								// Look for the entry
								const auto *entry = getEntry(project, pair2.first, pair.first);
								assert(entry);

								// Serialize data
								auto data = entry->SerializeAsString();
								out_packet
									<< io::write_dynamic_range<NetUInt32>(data);
							}
						}
					}
					out_packet.finish();
				}
			}

			namespace team_write
			{
				void loginResult(pp::OutgoingPacket &out_packet, LoginResult result)
				{
					out_packet.start(team_packet::LoginResult);
					out_packet
					        << io::write<NetUInt32>(ProtocolVersion)
					        << io::write<NetUInt8>(result);
					out_packet.finish();
				}

				void compressedFile(pp::OutgoingPacket & out_packet, const String &filename, std::istream & fileStream)
				{
					// Determine total file size
					fileStream.seekg(0, std::ios::end);
					size_t originalSize = fileStream.tellg();
					fileStream.seekg(0);

					// Compress using ZLib
					boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
					in.push(boost::iostreams::zlib_compressor(boost::iostreams::zlib::best_speed));
					in.push(fileStream);

					// Copy to output stream
					std::stringstream outStrm;
					boost::iostreams::copy(in, outStrm);
					outStrm.seekg(0, std::ios::end);
					size_t bufferSize = outStrm.tellg();
					outStrm.seekg(0, std::ios::beg);

					String buffer = outStrm.str();

					// Write packet
					out_packet.start(team_packet::CompressedFile);
					out_packet
						<< io::write_dynamic_range<NetUInt8>(filename)
						<< io::write<NetUInt32>(originalSize)
						<< io::write_range(buffer.begin(), buffer.begin() + bufferSize);
					out_packet.finish();
				}

				void editorUpToDate(pp::OutgoingPacket & out_packet)
				{
					// Write packet
					out_packet.start(team_packet::EditorUpToDate);
					out_packet.finish();
				}

				void entryUpdate(pp::OutgoingPacket & out_packet, const std::map<DataEntryType, std::map<UInt32, DataEntryChangeType>>& changes, const proto::Project &project)
				{
					out_packet.start(team_packet::EntryUpdate);
					out_packet << io::write<NetUInt32>(changes.size());
					for (const auto &pair : changes)
					{
						out_packet
							<< io::write<NetUInt32>(pair.first)
							<< io::write<NetUInt32>(pair.second.size());
						for (const auto &pair2 : pair.second)
						{
							out_packet
								<< io::write<NetUInt32>(pair2.first)
								<< io::write<NetUInt32>(pair2.second);

							// Write data entry if not deleted
							if (pair2.second != data_entry_change_type::Removed)
							{
								// Look for the entry
								const auto *entry = getEntry(project, pair2.first, pair.first);
								if (!entry)
								{
									// TODO!!!
									assert(false);
								}

								// Serialize data
								auto data = entry->SerializeAsString();
								out_packet
									<< io::write_dynamic_range<NetUInt32>(data);
							}
						}
					}
					out_packet.finish();
				}
			}

			namespace editor_read
			{
				bool login(io::Reader &packet, String &out_username, size_t maxUsernameLength, SHA1Hash &out_password)
				{
					UInt32 teamProtocolVersion = 0xffffffff;
					packet
					        >> io::read<NetUInt32>(teamProtocolVersion)
					        >> io::read_container<NetUInt8>(out_username, static_cast<NetUInt8>(maxUsernameLength))
					        >> io::read_range(out_password);

					// Handle team protocol version based packet changes if needed

					return packet;
				}

				bool keepAlive(io::Reader &packet)
				{
					//TODO
					return packet;
				}

				bool projectHashMap(io::Reader &packet, std::map<String, String> &out_hashMap)
				{
					UInt32 entries = 0;
					packet
						>> io::read<NetUInt32>(entries);
					
					out_hashMap.clear();
					for (UInt32 i = 0; i < entries; ++i)
					{
						String key, value;
						packet
							>> io::read_container<NetUInt8>(key)
							>> io::read_container<NetUInt8>(value);
						out_hashMap[key] = value;
					}

					return packet;
				}
				bool entryUpdate(io::Reader & packet, std::map<DataEntryType, std::map<UInt32, DataEntryChangeType>>& out_changes, proto::Project &out_project)
				{
					UInt32 entries = 0;
					packet
						>> io::read<NetUInt32>(entries);

					out_changes.clear();
					for (UInt32 i = 0; i < entries; ++i)
					{
						UInt32 entries2 = 0;
						DataEntryType key1;
						packet
							>> io::read<NetUInt32>(key1)
							>> io::read<NetUInt32>(entries2);

						auto &data = out_changes[key1];
						for (UInt32 j = 0; j < entries2; ++j)
						{
							UInt32 key = 0;
							DataEntryChangeType value;
							packet
								>> io::read<NetUInt32>(key)
								>> io::read<NetUInt32>(value);
							data[key] = value;

							// Deserialize content
							if (value != data_entry_change_type::Removed)
							{
								String data;
								packet
									>> io::read_container<NetUInt32>(data);

								// Create new object from this content
								if (value == data_entry_change_type::Modified)
								{
									auto *entry = getOrCreateEntry(out_project, key, key1);
									if (!entry)
									{
										// ERROR!
										return false;
									}

									if (!entry->ParseFromString(data))
									{
										return false;
									}
								}
							}
							else
							{
								// TODO: Remove selected entry
								
							}
						}
					}

					return packet;
				}
			}

			namespace team_read
			{
				bool loginResult(io::Reader &packet, LoginResult &out_result, UInt32 &out_serverVersion)
				{
					if (!(packet
					        >> io::read<NetUInt32>(out_serverVersion)
					        >> io::read<NetUInt8>(out_result)))
					{
						return false;
					}

					return true;
				}

				bool compressedFile(io::Reader & packet, String &out_filename, std::ostream & out_stream)
				{
					// Read original packet size
					UInt32 origSize = 0;
					if (!(packet
						>> io::read_container<NetUInt8>(out_filename)
						>> io::read<NetUInt32>(origSize)))
					{
						return false;
					}

					try
					{
						// Determine remaining size
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

						boost::iostreams::copy(in, out_stream);
						return true;
					}
					catch (const std::exception &e)
					{
						ELOG("Protocol read error: " << e.what());
						return false;
					}
				}

				bool editorUpToDate(io::Reader & packet)
				{
					return true;
				}
				bool entryUpdate(io::Reader & packet, std::map<DataEntryType, std::map<UInt32, DataEntryChangeType>>& out_changes, proto::Project &out_project)
				{
					UInt32 entries = 0;
					packet
						>> io::read<NetUInt32>(entries);

					out_changes.clear();
					for (UInt32 i = 0; i < entries; ++i)
					{
						UInt32 entries2 = 0;
						DataEntryType key1;
						packet
							>> io::read<NetUInt32>(key1)
							>> io::read<NetUInt32>(entries2);

						auto &data = out_changes[key1];
						for (UInt32 j = 0; j < entries2; ++j)
						{
							UInt32 key = 0;
							DataEntryChangeType value;
							packet
								>> io::read<NetUInt32>(key)
								>> io::read<NetUInt32>(value);
							data[key] = value;

							// Deserialize content
							if (value != data_entry_change_type::Removed)
							{
								String data;
								packet
									>> io::read_container<NetUInt32>(data);

								// Create new object from this content
								if (value == data_entry_change_type::Modified)
								{
									auto *entry = getOrCreateEntry(out_project, key, key1);
									if (!entry)
									{
										// ERROR!
										return false;
									}

									if (!entry->ParseFromString(data))
									{
										return false;
									}
								}
							}
							else
							{
								// TODO: Remove selected entry

							}
						}
					}

					return packet;
				}
			}
		}
	}
}
