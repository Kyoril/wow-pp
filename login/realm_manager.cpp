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

#include "realm_manager.h"
#include "realm.h"
#include "binary_io/string_sink.h"
#include <algorithm>
#include <cassert>

namespace wowpp
{
	RealmManager::RealmManager(
	    size_t realmCapacity)
		: m_realmCapacity(realmCapacity)
	{
	}

	RealmManager::~RealmManager()
	{
	}

	void RealmManager::realmDisconnected(Realm &realm)
	{
		const auto p = std::find_if(
		                   m_realms.begin(),
						   m_realms.end(),
		                   [&realm](const std::unique_ptr<Realm> &p)
		{
			return (&realm == p.get());
		});
		assert(p != m_realms.end());
		m_realms.erase(p);
	}

	const RealmManager::Realms &RealmManager::getRealms() const
	{
		return m_realms;
	}
	
	bool RealmManager::hasRealmCapacityBeenReached() const
	{
		return m_realms.size() >= m_realmCapacity;
	}

	void RealmManager::addRealm(std::unique_ptr<Realm> added)
	{
		assert(added);
		m_realms.push_back(std::move(added));
	}

	Realm * RealmManager::getRealmByInternalName(const String &name)
	{
		const auto r = std::find_if(
			m_realms.begin(),
			m_realms.end(),
			[&name](const std::unique_ptr<Realm> &r)
		{
			return (
				r->isAuthentificated() &&
				name == r->getInternalName()
				);
		});

		if (r == m_realms.end())
		{
			return nullptr;
		}

		return r->get();
	}

}
