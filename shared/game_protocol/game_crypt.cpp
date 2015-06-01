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

#include "game_crypt.h"
#include "common/big_number.h"
#include "common/hmac.h"

namespace wowpp
{
	namespace game
	{
		const size_t Crypt::CryptedSendLength = 4;
		const size_t Crypt::CryptedReceiveLength = 6;


		Crypt::Crypt()
			: m_initialized(false)
		{
		}

		Crypt::~Crypt()
		{
		}

		void Crypt::init()
		{
			m_send_i = m_send_j = m_recv_i = m_recv_j = 0;
			m_initialized = true;
		}

		void Crypt::decryptReceive(UInt8 *data, size_t length)
		{
			if (!m_initialized) return;
			if (length < CryptedReceiveLength) return;

			for (size_t t = 0; t < CryptedReceiveLength; ++t)
			{
				m_recv_i %= m_key.size();
				UInt8 x = (data[t] - m_recv_j) ^ m_key[m_recv_i];
				++m_recv_i;
				m_recv_j = data[t];
				data[t] = x;
			}
		}

		void Crypt::encryptSend(UInt8 *data, size_t length)
		{
			if (!m_initialized) return;
			if (length < CryptedSendLength) return;

			for (size_t t = 0; t < CryptedSendLength; ++t)
			{
				m_send_i %= m_key.size();
				UInt8 x = (data[t] ^ m_key[m_send_i]) + m_send_j;
				++m_send_i;
				data[t] = m_send_j = x;
			}
		}

		void Crypt::setKey(UInt8 *key, size_t length)
		{
			m_key.resize(length);
			std::copy(key, key + length, m_key.begin());
		}

		void Crypt::generateKey(HMACHash &out_key, const BigNumber &prime)
		{
			OpenSSL_HMACHashSink sink;

			std::vector<UInt8> arr = prime.asByteArray();
			sink.write(reinterpret_cast<const char*>(arr.data()), arr.size());
			out_key = sink.finalizeHash();
		}
	}
}
