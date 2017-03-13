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
#include "team_server_manager.h"
#include "team_server.h"
#include "binary_io/string_sink.h"

namespace wowpp
{
	TeamServerManager::TeamServerManager(
	    size_t capacity)
		: m_capacity(capacity)
	{
	}

	TeamServerManager::~TeamServerManager()
	{
	}

	void TeamServerManager::teamServerDisconnected(TeamServer &teamServer)
	{
		const auto p = std::find_if(
		                   m_teamServers.begin(),
							m_teamServers.end(),
		                   [&teamServer](const std::unique_ptr<TeamServer> &p)
		{
			return (&teamServer == p.get());
		});
		ASSERT(p != m_teamServers.end());
		m_teamServers.erase(p);
	}

	const TeamServerManager::TeamServers &TeamServerManager::getTeamServers() const
	{
		return m_teamServers;
	}
	
	bool TeamServerManager::hasCapacityBeenReached() const
	{
		return m_teamServers.size() >= m_capacity;
	}

	void TeamServerManager::addTeamServer(std::unique_ptr<TeamServer> added)
	{
		ASSERT(added);
		m_teamServers.push_back(std::move(added));
	}

	TeamServer * TeamServerManager::getTeamServerByInternalName(const String &name)
	{
		const auto r = std::find_if(
			m_teamServers.begin(),
			m_teamServers.end(),
			[&name](const std::unique_ptr<TeamServer> &r)
		{
			return (
				r->isAuthentificated() &&
				name == r->getInternalName()
				);
		});

		if (r == m_teamServers.end())
		{
			return nullptr;
		}

		return r->get();
	}

}
