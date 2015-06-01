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
#include "common/constants.h"
#include "common/big_number.h"

namespace wowpp
{
	/// Manages a player session on the login server.
	class Session final
	{
	public:

		typedef BigNumber Key;

	public:

		explicit Session(
			const Key &key,
			UInt32 userId,
		    const String &userName,
			const BigNumber &v,
			const BigNumber &s
		);

		/// Gets the duration of this session in milliseconds.
		GameTime getDuration() const;
		/// Tries to enter a realm.
		bool tryEnterRealm(UInt32 realm);
		/// Leaves the current realm.
		void realmLeft();
		/// Gets the account identifier.
		UInt32 getUserId() const { return m_userId; }
		/// Gets the user name in uppercase letters.
		const String &getUserName() const { return m_userName; }
		/// Determines whether the user has entered a realm.
		bool hasEnteredRealm() const { return (m_realm != constants::InvalidId); }
		/// Gets the realm identifier of the entered realm.
		UInt32 getRealm() const { return m_realm; }
		/// Gets the SRP-6 calculated session key.
		const Key &getKey() const { return m_key; }
		/// Gets the sessions start time.
		GameTime getStartTime() const { return m_startTime; }
		const BigNumber &getV() const { return m_v; }
		const BigNumber &getS() const { return m_s; }

	private:

		Key m_key;
		GameTime m_startTime;
		UInt32 m_realm;
		UInt32 m_userId;
		String m_userName;
		const BigNumber &m_v;
		const BigNumber &m_s;

		void realmEntered(UInt32 realm);
	};
}

