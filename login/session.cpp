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

#include "session.h"
#include "common/clock.h"
#include "log/default_log_levels.h"
#include <cassert>

namespace wowpp
{
	Session::Session(const Key &key, UInt32 userId, const String &userName, const BigNumber &v, const BigNumber &s)
		: m_key(key)
		, m_startTime(getCurrentTime())
		, m_realm(constants::InvalidId)
		, m_userId(userId)
		, m_userName(userName)
		, m_v(v)
		, m_s(s)
	{
	}
	GameTime Session::getDuration() const
	{
		const GameTime current = getCurrentTime();

		assert(current >= getStartTime());
		return (current - getStartTime());
	}

	bool Session::tryEnterRealm(UInt32 realm)
	{
		assert(realm != constants::InvalidId);

		realmEntered(realm);
		return true;
	}

	void Session::realmLeft()
	{
		assert(getRealm() != constants::InvalidId);

		m_realm = constants::InvalidId;

		DLOG("Player " << m_userName << " left realm");
	}

	void Session::realmEntered(UInt32 realm)
	{
		m_realm = realm;
	}
}
