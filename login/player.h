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

#include <boost/noncopyable.hpp>
#include "auth_protocol/auth_protocol.h"
#include "auth_protocol/auth_connection.h"
#include "common/big_number.h"
#include "session.h"
#include <cassert>

namespace wowpp
{
	class PlayerManager;
	class RealmManager;
	struct IDatabase;

	/// Player connection class.
	class Player final
			: public auth::IConnectionListener
			, public boost::noncopyable
	{
	public:

		typedef AbstractConnection<auth::Protocol> Client;
		typedef std::function<std::unique_ptr<Session>(Session::Key key, UInt32 userId, String userName, const BigNumber &v, const BigNumber &s)> SessionFactory;

	public:

		explicit Player(PlayerManager &manager,
						RealmManager &realmManager,
						IDatabase &database,
						SessionFactory createSession,
		                std::shared_ptr<Client> connection,
						const String &address);

		/// Gets the player connection class used to send packets to the client.
		Client &getConnection() { assert(m_connection); return *m_connection; }
		/// Gets the player manager which manages all connected players.
		PlayerManager &getManager() const { return m_manager; }
		/// Determines whether the player is authentificated.
		/// @returns true if the player is authentificated.
		bool isAuthentificated() const { return (getSession() != nullptr); }
		/// Gets the account name the player is logged in with.
		const String &getAccountName() const { return m_userName; }
		/// Gets a pointer to the current session if available or nullptr if
		/// not authentificated.
		Session *getSession() { return m_session.get(); }
		/// Gets a pointer to the current session if available or nullptr if
		/// not authentificated.
		const Session *getSession() const { return m_session.get(); }
		/// 
		UInt32 getAccountId() const { return m_accountId; }

	private:

		PlayerManager &m_manager;
		RealmManager &m_realmManager;
		IDatabase &m_database;
		std::shared_ptr<Client> m_connection;
		String m_address;						// IP address in string format
		String m_userName;						// Account name in uppercase letters
		auth::AuthPlatform m_platform;			// Client platform (32 Bit / 64 Bit)
		auth::AuthSystem m_system;				// User system (Windows, Mac)
		auth::AuthLocale m_locale;				// Client language
		UInt8 m_version1;						// Major version: X.0.0.00000
		UInt8 m_version2;						// Minor version: 0.X.0.00000
		UInt8 m_version3;						// Patch version: 0.0.X.00000
		UInt16 m_build;							// Build version: 0.0.0.XXXXX
		UInt32 m_accountId;						// Account ID
		SessionFactory m_createSession;
		std::unique_ptr<Session> m_session;		// Session
		
	private:

		BigNumber m_s, m_v;
		BigNumber m_b, m_B;
		BigNumber m_unk3;
		BigNumber m_reconnectProof;

		/// Number of bytes used to store m_s.
		const static int ByteCountS = 32;
		/// Number of bytes used by a sha1 hash. Taken from OpenSSL.
		const static int ShaDigestLength = 20;

	private:

		/// Closes the connection if still connected.
		void destroy();
		/// Calculates initial values of S and V based on the users password hash.
		/// Note: These values could be cached to the database.
		/// @param passwordHash A string containing the hexadecimal version of the SHA1 password hash.
		void setVSFields(const String &passwordHash);

		/// @copydoc wow::auth::IConnectionListener::connectionLost()
		void connectionLost() override;
		/// @copydoc wow::auth::IConnectionListener::connectionMalformedPacket()
		void connectionMalformedPacket() override;
		/// @copydoc wow::auth::IConnectionListener::connectionPacketReceived()
		void connectionPacketReceived(auth::IncomingPacket &packet) override;

		/// Handles an incoming packet with packet id LogonChallenge.
		/// @param packet The packet data.
		void handleLogonChallenge(auth::IncomingPacket &packet);
		/// Handles an incoming packet with packet id LogonProof.
		/// @param packet The packet data.
		void handleLogonProof(auth::IncomingPacket &packet);
		/// Handles an incoming packet with packet id RealmList.
		/// @param packet The packet data.
		void handleRealmList(auth::IncomingPacket &packet);
	};
}
