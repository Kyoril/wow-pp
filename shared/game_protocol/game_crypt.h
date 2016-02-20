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

#include "common/typedefs.h"
#include "common/hmac.h"
#include <vector>

namespace wowpp
{
	class BigNumber;

	namespace game
	{
		/// Used for packet encryption and decryption. Inspired by AuthCrypt class
		/// from MaNGOS project (http://www.getmangos.eu)
		class Crypt final
		{
		public:

			const static size_t CryptedSendLength;
			const static size_t CryptedReceiveLength;

		public:

			explicit Crypt();
			~Crypt();

			void init();
			void setKey(UInt8 *key, size_t length);
			void decryptReceive(UInt8 *data, size_t length);
			void encryptSend(UInt8 *data, size_t length);
			bool isInitialized() const {
				return m_initialized;
			}

			///
			/// @param out_key The generated key.
			/// @param prime Big number to be used for key calculation.
			static void generateKey(HMACHash &out_key, const BigNumber &prime);

		private:

			std::vector<UInt8> m_key;
			UInt8 m_send_i, m_send_j, m_recv_i, m_recv_j;
			bool m_initialized;
		};
	}
}
