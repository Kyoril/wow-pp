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
#include "player.h"
#include "player_manager.h"
#include "realm_manager.h"
#include "realm.h"
#include "common/sha1.h"
#include "common/constants.h"
#include "log/default_log_levels.h"
#include "database.h"

using namespace std;

namespace wowpp
{
	Player::Player(PlayerManager &manager, RealmManager &realmManager, IDatabase &database, SessionFactory createSession, std::shared_ptr<Client> connection,
	               const String &address, TimerQueue &timerQueue)
		: m_manager(manager)
		, m_realmManager(realmManager)
		, m_database(database)
		, m_connection(std::move(connection))
		, m_address(address)
        , m_createSession(createSession)
		, m_accountId(0)
		, m_loginChallenge(false)
		, m_reconnectChallenge(false)
		, m_timeout(timerQueue)
		, m_nextRealmRequest(0)
		, m_realmRequestCount(0)
	{
		ASSERT(m_connection);

		m_connection->setListener(*this);
		m_onTimeout = m_timeout.ended.connect([this]() 
		{
			WLOG("Client connection timed out!");
			m_connection->close();

			destroy();
			return;
		});

		m_timeout.setEnd(getCurrentTime() + constants::OneMinute);
	}

	void Player::connectionLost()
	{
		ILOG("Client " << m_address << " disconnected");
		destroy();
	}

	void Player::destroy()
	{
		m_connection->resetListener();
		m_connection.reset();

		m_manager.playerDisconnected(*this);
	}

	void Player::setVSFields(const String &passwordHash)
	{
		m_s.setRand(ByteCountS * 8);

		BigNumber I;
		I.setHexStr(passwordHash);

		// In case of leading zeros in the rI hash, restore them
		std::array<UInt8, ShaDigestLength> mDigest;
		mDigest.fill(0);

		if (I.getNumBytes() <= ShaDigestLength)
		{
			auto arr = I.asByteArray();
			std::copy(arr.begin(), arr.end(), mDigest.begin());
		}

		std::reverse(mDigest.begin(), mDigest.end());

		// Generate sha1 hash
		Boost_SHA1HashSink sha;
		std::vector<UInt8> sArr = m_s.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		sha.write(reinterpret_cast<const char*>(mDigest.data()), ShaDigestLength);
		SHA1Hash hash = sha.finalizeHash();

		BigNumber x;
		x.setBinary(hash.data(), hash.size());

		m_v = constants::srp::g.modExp(x, constants::srp::N);

		// Update database values
		m_database.setSVFields(m_accountId, m_s, m_v);
	}

	void Player::connectionMalformedPacket()
	{
		ILOG("Client " << m_address << " sent malformed packet");
		destroy();
	}

	PacketParseResult Player::connectionPacketReceived(auth::IncomingPacket &packet)
	{
		const auto packetId = packet.getId();
		bool isValid = true;

		switch (packetId)
		{
			case auth::client_packet::LogonChallenge:
			{
				handleLogonChallenge(packet);
				break;
			}
			case auth::client_packet::LogonProof:
			{
				handleLogonProof(packet);
				break;
			}
			case auth::client_packet::ReconnectChallenge:
			{
				handleReconnectChallenge(packet);
				break;
			}
			case auth::client_packet::ReconnectProof:
			{
				handleReconnectProof(packet);
				break;
			}
			case auth::client_packet::RealmList:
			{
				handleRealmList(packet);
				break;
			}
			default:
			{
				isValid = false;
				WLOG("Unknown packet received from " << m_address 
					<< " - ID: " << static_cast<UInt32>(packetId) 
					<< "; Size: " << packet.getSource()->size() << " bytes");
				return PacketParseResult::Disconnect;
			}
		}

		if (isValid)
		{
			// Update timeout event
			m_timeout.setEnd(getCurrentTime() + constants::OneMinute);
		}

		return PacketParseResult::Pass;
	}

	void Player::handleLogonChallenge(auth::IncomingPacket &packet)
	{
		// Read packet data and save it
		if (!auth::client_read::logonChallenge(packet, m_version1, m_version2, m_version3, m_build, m_platform, m_system, m_locale, m_userName))
		{
			ELOG("Could not read packet CMD_AUTH_LOGON_CHALLENGE");
			m_connection->close();
			destroy();
			return;
		}

		if (m_accountId != 0)
		{
			WLOG("Player tried to log in again!");
			m_connection->close();
			destroy();
			return;
		}
		
		if (m_reconnectChallenge)
		{
			WLOG("Already sent reconnect challenge, so can't send login challenge");
			m_connection->close();
			destroy();
			return;
		}
		
		// The temporary result
		auth::AuthResult result = auth::auth_result::FailUnknownAccount;

		// Try to get user settings
		String dbPassword;
		if (m_database.getPlayerPassword(m_userName, m_accountId, dbPassword))
		{
			if (m_accountId != 0)
			{
				// TODO: Check if the account is banned / suspended etc.
				BigNumber tmpS, tmpV;
				m_database.getSVFields(m_accountId, tmpS, tmpV);

				if (tmpS.getNumBytes() != ByteCountS || tmpV.getNumBytes() != ByteCountS)
				{
					setVSFields(dbPassword);
				}
				else
				{
					m_s = tmpS;
					m_v = tmpV;
				}

				// We are NOT banned so continue
				result = auth::auth_result::Success;

				m_b.setRand(19 * 8);
				BigNumber gmod = constants::srp::g.modExp(m_b, constants::srp::N);
				m_B = ((m_v * 3) + gmod) % constants::srp::N;

				ASSERT(gmod.getNumBytes() <= 32);

				m_unk3.setRand(16 * 8);
				m_loginChallenge = true;
			}
		}

		// Send packet
		m_connection->sendSinglePacket(
			std::bind(
				auth::server_write::logonChallenge,
				std::placeholders::_1,
				result,
				(auth::SecurityFlags)(auth::security_flags::None /*| auth::security_flags::MatrixInput*/),
				std::cref(m_B),
				std::cref(constants::srp::g),
				std::cref(constants::srp::N),
				std::cref(m_s),
				std::cref(m_unk3)));
	}

	void Player::handleLogonProof(auth::IncomingPacket &packet)
	{
		if (m_accountId == 0)
		{
			WLOG("Tried to send logon proof before challenge");
			m_connection->close();
			destroy();
			return;
		}

		if (m_session)
		{
			WLOG("Tried to send proof when already proofed!");
			m_connection->close();
			destroy();
			return;
		}
		
		if (!m_loginChallenge || m_reconnectChallenge)
		{
			WLOG("Received logon proof without proper challenge packet!");
			m_connection->close();
			destroy();
			return;
		}

		// Read packet data and save it
		std::array<UInt8, 32> rec_A;
		std::array<UInt8, 20> rec_M1;
		std::array<UInt8, 20> rec_crc_hash;
		UInt8 number_of_keys;
		UInt8 securityFlags;

		// Parse packet content
		if (!auth::client_read::logonProof(packet, rec_A, rec_M1, rec_crc_hash, number_of_keys, securityFlags))
		{
			ELOG("Could not read packet CMD_AUTH_LOGON_PROOF");
			destroy();
			return;
		}

		// TODO: Read pin input

		// Read Matrix hash
		if (securityFlags & auth::security_flags::MatrixInput)
		{
			SHA1Hash matrixHash;
			if (!(packet >> io::read_range(matrixHash)))
			{
				ELOG("Could not read matrix card hash");
				destroy();
				return;
			}

			// TODO: Make sure that the hash matches...
			//sha1PrintHex(std::cout, matrixHash);
			//std::cout << std::endl;
		}

		// TODO: Token

		SHA1Hash hash;

		// Check if the client version is valid (SUPPORTED_CLIENT_BUILD is set in CMake)
		if (m_build != SUPPORTED_CLIENT_BUILD)
		{
			// Send failure and stop here
			WLOG("Player " << m_address << " tried to login with unsupported client build (" << m_build << ")");
			m_connection->sendSinglePacket(
				std::bind(
					auth::server_write::logonChallenge,
					std::placeholders::_1,
					auth::auth_result::FailVersionInvalid,
					auth::security_flags::None,
					std::cref(m_B),
					std::cref(constants::srp::g),
					std::cref(constants::srp::N),
					std::cref(m_s),
					std::cref(m_unk3)));
			return;
		}

		// Continue the SRP6 calculation based on data received from the client
		BigNumber A;
		A.setBinary(rec_A.data(), rec_A.size());

		// SRP safeguard: abort if A % N == 0
		if ((A % constants::srp::N).isZero())
		{
			ELOG("Detected invalid A value from client");
			m_connection->close();
			destroy();
			return;
		}

		// Build hash
		Boost_SHA1HashSink sha;
		std::vector<UInt8> sArr = A.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		sArr = m_B.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		hash = sha.finalizeHash();

		BigNumber u;
		u.setBinary(hash.data(), hash.size());
		BigNumber S = (A * (m_v.modExp(u, constants::srp::N))).modExp(m_b, constants::srp::N);

		std::vector<UInt8> t = S.asByteArray(32);
		std::array<UInt8, 16> t1;
		for (size_t i = 0; i < t1.size(); ++i)
		{
			t1[i] = t[i * 2];
		}

		sha.write(reinterpret_cast<const char*>(t1.data()), t1.size());
		hash = sha.finalizeHash();

		std::array<UInt8, 40> vK;
		for (size_t i = 0; i < 20; ++i)
		{
			vK[i * 2] = hash[i];
		}
		for (size_t i = 0; i < 16; ++i)
		{
			t1[i] = t[i * 2 + 1];
		}

		BigNumber K;
		sha.write(reinterpret_cast<const char*>(t1.data()), t1.size());
		hash = sha.finalizeHash();
		for (size_t i = 0; i < 20; ++i)
		{
			vK[i * 2 + 1] = hash[i];
		}
		K.setBinary(vK.data(), vK.size());

		sArr = constants::srp::N.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		SHA1Hash h = sha.finalizeHash();

		sArr = constants::srp::g.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		hash = sha.finalizeHash();

		for (size_t i = 0; i < h.size(); ++i)
		{
			h[i] ^= hash[i];
		}

		BigNumber t3;
		t3.setBinary(h.data(), h.size());

		sha.write(reinterpret_cast<const char*>(m_userName.data()), m_userName.size());
		SHA1Hash t4 = sha.finalizeHash();

		sArr = t3.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		sha.write(reinterpret_cast<const char*>(t4.data()), t4.size());
		sArr = m_s.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		sArr = A.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		sArr = m_B.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		sArr = K.asByteArray();
		sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
		hash = sha.finalizeHash();

		BigNumber M;
		M.setBinary(hash.data(), hash.size());

		// Result
		auth::AuthResult proofResult = auth::auth_result::FailUnknownAccount;

		/// Check if SRP6 results match (password is correct), else send an error
		sArr = M.asByteArray(20);
		if (std::equal(sArr.begin(), sArr.end(), rec_M1.begin()))
		{
			ILOG("User " << m_userName << " successfully authenticated");

			/// Finish SRP6 and send the final result to the client
			sArr = A.asByteArray();
			sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
			sArr = M.asByteArray();
			sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
			sArr = K.asByteArray();
			sha.write(reinterpret_cast<const char*>(sArr.data()), sArr.size());
			hash = sha.finalizeHash();

			// Create session
			m_session = m_createSession(K, m_accountId, m_userName, m_v, m_s);
			m_database.setKey(m_accountId, K);

			// Send proof
			proofResult = auth::auth_result::Success;
		}
		else
		{
			WLOG("Account " << m_userName << " tried to login with wrong password!");
		}

		// Send proof result
		m_connection->sendSinglePacket(
			std::bind(
				auth::server_write::logonProof,
				std::placeholders::_1,
				proofResult,
				std::cref(hash)));
	}

	void Player::handleReconnectChallenge(auth::IncomingPacket & packet)
	{
		// Read packet data and save it
		if (!auth::client_read::reconnectChallenge(packet, m_version1, m_version2, m_version3, m_build, m_platform, m_system, m_locale, m_userName))
		{
			ELOG("Could not read packet CMD_AUTH_RECONNECT_CHALLENGE");
			m_connection->close();
			destroy();
			return;
		}

		if (m_accountId != 0)
		{
			WLOG("Player tried to reconnect again!");
			m_connection->close();
			destroy();
			return;
		}
		
		if (m_loginChallenge)
		{
			WLOG("Already sent login challenge, so can't send reconnect challenge");
			m_connection->close();
			destroy();
			return;
		}

		// Get account informations (TODO: We need a session key)
		if (!m_database.getKey(m_userName, m_accountId, m_reconnectKey) ||
			m_accountId == 0)
		{
			WLOG("Unknown account!");
			m_connection->close();
			destroy();
			return;
		}

		// Build some random reconnect proof
		m_reconnectProof.setRand(16 * 8);
		m_reconnectChallenge = true;
		
		// Send proof result
		m_connection->sendSinglePacket(
			std::bind(
				auth::server_write::reconnectChallenge,
				std::placeholders::_1,
				std::cref(m_reconnectProof)));
	}

	void Player::handleReconnectProof(auth::IncomingPacket & packet)
	{
		if (m_accountId == 0 || !m_reconnectProof.getNumBytes() || m_userName.empty())
		{
			WLOG("Tried to send logon proof before challenge");
			m_connection->close();
			destroy();
			return;
		}

		if (m_session)
		{
			WLOG("Tried to send proof when already proofed!");
			m_connection->close();
			destroy();
			return;
		}
		
		if (!m_reconnectChallenge || m_loginChallenge)
		{
			WLOG("Tried to send reconnect proof without proper challenge request");
			m_connection->close();
			destroy();
			return;
		}

		// Read packet data and save it
		std::array<unsigned char, 16> R1;
		SHA1Hash R2, R3;
		UInt8 keyNum = 0;
		if (!auth::client_read::reconnectProof(packet, R1, R2, R3, keyNum))
		{
			ELOG("Could not read packet CMD_AUTH_RECONNECT_PROOF");
			m_connection->close();
			destroy();
			return;
		}

		BigNumber t1;
		t1.setBinary(R1.data(), R1.size());

		Boost_SHA1HashSink sha;
		sha.write(m_userName.c_str(), m_userName.size());
		std::vector<unsigned char> tArr = t1.asByteArray();
		sha.write(reinterpret_cast<const char*>(tArr.data()), tArr.size());
		tArr = m_reconnectProof.asByteArray();
		sha.write(reinterpret_cast<const char*>(tArr.data()), tArr.size());
		tArr = m_reconnectKey.asByteArray();
		sha.write(reinterpret_cast<const char*>(tArr.data()), tArr.size());
		auto hash = sha.finalizeHash();

		if (hash != R2)
		{
			WLOG("User " << m_userName << " tried to log in, but session is invalid!");
			m_connection->close();
			destroy();
			return;
		}

		// Create session
		m_session = m_createSession(m_reconnectKey, m_accountId, m_userName, m_v, m_s);
		m_reconnectKey.setUInt32(0);
		
		// Send proof
		m_connection->sendSinglePacket(
			std::bind(
				auth::server_write::reconnectProof,
				std::placeholders::_1));
	}

	void Player::handleRealmList(auth::IncomingPacket &packet)
	{
		// Read realm list packet
		if (!auth::client_read::realmList(packet))
		{
			m_connection->close();
			destroy();
			return;
		}

		// Are we authentificated?
		if (!isAuthentificated())
		{
			m_connection->close();
			destroy();
			return;
		}

		// Realm list request spam protection
		GameTime now = getCurrentTime();
		if (now < m_nextRealmRequest)
		{
			// Increase realm request counter
			m_realmRequestCount++;

			// Limit to three requests every 10 seconds
			if (m_realmRequestCount > 3)
			{
				m_connection->close();
				destroy();
				return;
			}
		}
		else
		{
			// Only one realm list request every 10 seconds
			m_nextRealmRequest = now + constants::OneSecond * 10;
			m_realmRequestCount = 0;
		}

		// Collect list of available realms
		std::vector<auth::RealmEntry> entries;
		const RealmManager::Realms &realms = m_realmManager.getRealms();
		for (auto &realm : realms)
		{
			if (realm->isAuthentificated())
			{
				entries.push_back(realm->getRealmEntry());
			}
		}

		// It seems that vanilla wow does not support more than 255 realms
		ASSERT(realms.size() < std::numeric_limits<UInt8>::max());

		// Send realm list
		m_connection->sendSinglePacket(
			std::bind(
				auth::server_write::realmList,
				std::placeholders::_1,
				std::cref(entries)));
	}
}
