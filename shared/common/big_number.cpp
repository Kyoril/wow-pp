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

#include "big_number.h"
#include <openssl/ssl.h>
#include <openssl/bn.h>
#include <algorithm>

namespace wowpp
{
	BigNumber::BigNumber()
	{
		m_bn = BN_new();
	}

	BigNumber::BigNumber(const BigNumber &Other)
	{
		m_bn = BN_dup(Other.m_bn);
	}

	BigNumber::BigNumber(UInt32 Value)
	{
		m_bn = BN_new();
		BN_set_word(m_bn, Value);
	}

	BigNumber::BigNumber(const String &Hex)
	{
		m_bn = BN_new();
		setHexStr(Hex);
	}

	BigNumber::~BigNumber()
	{
		BN_free(m_bn);
	}

	void BigNumber::setUInt32(UInt32 value)
	{
		BN_set_word(m_bn, value);
	}

	void BigNumber::setUInt64(UInt64 value)
	{
		BN_add_word(m_bn, static_cast<UInt32>(value >> 32));
		BN_lshift(m_bn, m_bn, 32);
		BN_add_word(m_bn, static_cast<UInt32>(value & 0xFFFFFFFF));
	}

	void BigNumber::setBinary(const UInt8 *data, size_t length)
	{
		std::vector<UInt8> vector(data, data + length);
		setBinary(vector);
	}

	void BigNumber::setBinary(const std::vector<UInt8> &data)
	{
		// Copy and reverse
		std::vector<UInt8> buffer(data);
		std::reverse(buffer.begin(), buffer.end());
		BN_bin2bn(buffer.data(), buffer.size(), m_bn);
	}

	void BigNumber::setHexStr(const String &Hex)
	{
		BN_hex2bn(&m_bn, Hex.c_str());
	}

	void BigNumber::setRand(int numBits)
	{
		BN_rand(m_bn, numBits, 0, 1);
	}

	bool BigNumber::isZero() const
	{
		return BN_is_zero(m_bn) != 0;
	}

	BigNumber BigNumber::modExp(const BigNumber &bn1, const BigNumber &bn2) const
	{
		BigNumber ret;

		BN_CTX *bnctx = BN_CTX_new();
		BN_mod_exp(ret.m_bn, m_bn, bn1.m_bn, bn2.m_bn, bnctx);
		BN_CTX_free(bnctx);

		return ret;
	}

	BigNumber BigNumber::exp(const BigNumber &Other) const
	{
		BigNumber ret;

		BN_CTX *bnctx = BN_CTX_new();
		BN_exp(ret.m_bn, m_bn, Other.m_bn, bnctx);
		BN_CTX_free(bnctx);

		return ret;
	}

	UInt32 BigNumber::getNumBytes() const
	{
		return BN_num_bytes(m_bn);
	}

	UInt32 BigNumber::asUInt32() const
	{
		return static_cast<UInt32>(BN_get_word(m_bn));
	}

	std::vector<UInt8> BigNumber::asByteArray(int minSize /*= 0*/) const
	{
		const int numBytes = getNumBytes();
		const int len = (minSize >= numBytes) ? minSize : numBytes;

		// Reserver enough memory
		std::vector<UInt8> ret;
		ret.resize(len);

		// Clear array if needed
		if (len > numBytes)
		{
			std::fill(ret.begin(), ret.end(), 0);
		}

		BN_bn2bin(m_bn, ret.data());
		std::reverse(ret.begin(), ret.end());

		return ret;
	}

	String BigNumber::asHexStr() const
	{
		char *buffer = BN_bn2hex(m_bn);
		String ret(buffer);

		// Free this since we copied it to the string already. If we don't do this,
		// it will result in a memory leak (which is bad!)
		OPENSSL_free(buffer);

		return ret;
	}

	String BigNumber::asDecStr() const
	{
		char *buffer = BN_bn2dec(m_bn);
		String ret(buffer);

		// Free this since we copied it to the string already. If we don't do this,
		// it will result in a memory leak (which is bad!)
		OPENSSL_free(buffer);

		return ret;
	}

	BigNumber BigNumber::operator=(const BigNumber &Other)
	{
		BN_copy(m_bn, Other.m_bn);
		return *this;
	}

	BigNumber BigNumber::operator+=(const BigNumber &Other)
	{
		BN_add(m_bn, m_bn, Other.m_bn);
		return *this;
	}

	BigNumber BigNumber::operator-=(const BigNumber &Other)
	{
		BN_sub(m_bn, m_bn, Other.m_bn);
		return *this;
	}

	BigNumber BigNumber::operator*=(const BigNumber &Other)
	{
		BN_CTX *bnctx = BN_CTX_new();
		BN_mul(m_bn, m_bn, Other.m_bn, bnctx);
		BN_CTX_free(bnctx);

		return *this;
	}

	BigNumber BigNumber::operator/=(const BigNumber &Other)
	{
		BN_CTX *bnctx = BN_CTX_new();
		BN_div(m_bn, nullptr, m_bn, Other.m_bn, bnctx);
		BN_CTX_free(bnctx);

		return *this;
	}

	BigNumber BigNumber::operator%=(const BigNumber &Other)
	{
		BN_CTX *bnctx = BN_CTX_new();
		BN_mod(m_bn, m_bn, Other.m_bn, bnctx);
		BN_CTX_free(bnctx);

		return *this;
	}
}
