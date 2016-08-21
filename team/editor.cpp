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
#include "editor.h"
#include "editor_manager.h"
#include "login_connector.h"
#include "common/constants.h"
#include "wowpp_protocol/wowpp_editor_team.h"
#include "log/default_log_levels.h"

using namespace std;

namespace wowpp
{
	Editor::Editor(EditorManager &manager, LoginConnector &loginConnector, proto::Project &project, std::shared_ptr<Client> connection, const String &address)
		: m_manager(manager)
		, m_loginConnector(loginConnector)
		, m_project(project)
		, m_connection(std::move(connection))
		, m_address(address)
		, m_state(editor_state::Connected)
	{
		assert(m_connection);
		m_connection->setListener(*this);
	}

	void Editor::onAuthResult(pp::editor_team::LoginResult result)
	{
		// Already authentificated?
		if (isAuthentificated())
		{
			return;
		}

		// No username set?
		if (m_name.empty())
		{
			return;
		}

		// Check if result was a success
		if (result == pp::editor_team::login_result::Success)
		{
			m_state = editor_state::Authentificated;
			ILOG("User " << m_name << " successfully signed in");
		}

		// Notify the editor about the results
		sendPacket(
			std::bind(pp::editor_team::team_write::loginResult, std::placeholders::_1, result));
	}

	void Editor::connectionLost()
	{
		WLOG("Editor " << (isAuthentificated() ? m_name : m_address) << " disconnected");
		destroy();
	}

	void Editor::destroy()
	{
		m_connection->resetListener();
		m_connection.reset();

		m_manager.editorDisconnected(*this);
	}

	bool Editor::isInValidState(const String &name, EditorState state, bool verbose /*= false*/) const
	{
		switch (state)
		{
			case editor_state::Connected:
			{
				const bool auth = isAuthentificated();
				if (auth && verbose)
				{
					WLOG("Packet " << name << " is only handled if the editor not yet authentificated!");
				}
				return !auth;
			}

			case editor_state::Authentificated:
			{
				const bool auth = isAuthentificated();
				if (!auth && verbose)
				{
					WLOG("Packet " << name << " is only handled if the editor is authenticated!");
				}
				return auth;
			}

			case editor_state::UpToDate:
			{
				if ((m_state < editor_state::UpToDate || m_state == editor_state::Unknown) && verbose)
				{
					WLOG("Packet " << name << " is only handled if the editor is up to date with the local repository!");
				}
				return (m_state >= editor_state::UpToDate && m_state != editor_state::Unknown);
			}

			default:
			{
				// Unknown or unhandled state
				return false;
			}
		}
	}

	void Editor::connectionMalformedPacket()
	{
		WLOG("Editor " << (isAuthentificated() ? m_name : m_address) << " sent malformed packet");
		destroy();
	}

	void Editor::connectionPacketReceived(pp::IncomingPacket &packet)
	{
		const auto packetId = packet.getId();

		switch (packetId)
		{
#define QUOTE(str) #str
#define WOWPP_HANDLE_PACKET(name, state) \
			case pp::editor_team::editor_packet::name: \
			{ \
				if (!isInValidState(#name, state, true)) \
					break; \
				handle##name(packet); \
				break; \
			}

			WOWPP_HANDLE_PACKET(Login, editor_state::Connected)
			WOWPP_HANDLE_PACKET(ProjectHashMap, editor_state::Authentificated)
			WOWPP_HANDLE_PACKET(EntryUpdate, editor_state::UpToDate)

#undef WOWPP_HANDLE_PACKET
#undef QUOTE

			default:
			{
				// Log unknown or unhandled packet
				WLOG("Received unknown packet " << static_cast<UInt32>(packetId)
					<< " from editor at " << m_address);
				break;
			}
		}
	}

	void Editor::handleLogin(pp::IncomingPacket &packet)
	{
		using namespace pp::editor_team;

		// Read packet
		String username;
		SHA1Hash password;
		if (!pp::editor_team::editor_read::login(packet, username, 16, password))
		{
			WLOG("Could not read login packet from editor.");
			return;
		}

		DLOG("AUTH REQUEST: " << username);
		m_name = username;

		// Send authentication request to the login server
		m_loginConnector.editorLoginRequest(shared_from_this(), password);
	}

	void Editor::handleProjectHashMap(pp::IncomingPacket & packet)
	{
		std::map<String, String> hashs;
		if (!pp::editor_team::editor_read::projectHashMap(packet, hashs))
		{
			WLOG("Could not read packet from editor.");
			return;
		}

		// Contains a list of file names to update
		std::vector<String> filesToUpdate;

		// Build local hash table
		std::map<String, String> localHashs;
		localHashs["spells"] = m_project.spells.hashString;
		localHashs["units"] = m_project.units.hashString;
		localHashs["objects"] = m_project.objects.hashString;
		localHashs["maps"] = m_project.maps.hashString;
		localHashs["emotes"] = m_project.emotes.hashString;
		localHashs["unit_loot"] = m_project.unitLoot.hashString;
		localHashs["object_loot"] = m_project.objectLoot.hashString;
		localHashs["item_loot"] = m_project.itemLoot.hashString;
		localHashs["skinning_loot"] = m_project.skinningLoot.hashString;
		localHashs["skills"] = m_project.skills.hashString;
		localHashs["trainers"] = m_project.trainers.hashString;
		localHashs["vendors"] = m_project.vendors.hashString;
		localHashs["talents"] = m_project.talents.hashString;
		localHashs["items"] = m_project.items.hashString;
		localHashs["item_sets"] = m_project.itemSets.hashString;
		localHashs["classes"] = m_project.classes.hashString;
		localHashs["races"] = m_project.races.hashString;
		localHashs["levels"] = m_project.levels.hashString;
		localHashs["triggers"] = m_project.triggers.hashString;
		localHashs["zones"] = m_project.zones.hashString;
		localHashs["quests"] = m_project.quests.hashString;
		localHashs["factions"] = m_project.factions.hashString;
		localHashs["faction_templates"] = m_project.factionTemplates.hashString;
		localHashs["area_triggers"] = m_project.areaTriggers.hashString;
		localHashs["spell_categories"] = m_project.spellCategories.hashString;

		// Compare both
		for (const auto &local : localHashs)
		{
			auto it = hashs.find(local.first);
			if (it == hashs.end())
			{
				// Could not find the file info at all, so add an update
				filesToUpdate.push_back(local.first);
			}
			else
			{
				// Compare the actual hashes
				if ((*it).second != local.second)
				{
					filesToUpdate.push_back(local.first);
				}
			}
		}

		// Result
		ILOG("Detected " << filesToUpdate.size() << " file changes");

		// Build full path
		boost::filesystem::path p = m_project.getLastPath();
		p /= "wowpp";

		// Send updated or new files
		for (auto &file : filesToUpdate)
		{
			// Load the respective file
			std::ifstream inFile((p / (file + ".wppdat")).c_str(), std::ios::in | std::ios::binary);
			if (!inFile)
			{
				ELOG("Could not read file at path: " << p);
				continue;
			}

			// Send packet
			sendPacket(
				std::bind(pp::editor_team::team_write::compressedFile, std::placeholders::_1, std::cref(file), std::ref(inFile)));
		}

		// Send packet
		sendPacket(
			std::bind(pp::editor_team::team_write::editorUpToDate, std::placeholders::_1));

		// We are up-to-date now
		m_state = editor_state::UpToDate;
	}

	void Editor::handleEntryUpdate(pp::IncomingPacket & packet)
	{
		std::map<pp::editor_team::DataEntryType, std::map<UInt32, pp::editor_team::DataEntryChangeType>> changes;
		if (!pp::editor_team::editor_read::entryUpdate(packet, changes, m_project))
		{
			WLOG("Could not read packet from editor.");
			return;
		}

		// Save project
		if (!m_project.save(m_project.getLastPath()))
		{
			ELOG("Error saving updated project!");
			return;
		}

		// TODO: Resend packet
	}
}
