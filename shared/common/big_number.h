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
#include <vector>

struct bignum_st;

namespace wowpp
{
	/// Handles very large numbers needed for SRP-6 calculation. This is a modified
	/// version of the BigNumber class of the MaNGOS project (http://www.getmangos.eu)
	/// with some bug fixes and minor improvements.
	class BigNumber final
	{
	public:

		/// Initializes an empty number (zero).
		BigNumber();
		/// Copies the value of another big number.
		BigNumber(const BigNumber &n);
		/// Initializes a number based on a 32 bit unsigned integer value.
		BigNumber(UInt32 number);
		/// Initializes a number based on a hex string.
		BigNumber(const String &Hex);
		~BigNumber();

		/// Sets the value of this number to the value of a 32 bit unsigned integer.
		void setUInt32(UInt32 value);
		/// Sets the value of this number to the value of a 64 bit unsigned integer.
		void setUInt64(UInt64 value);
		/// Sets the value of this number based on a binary data block.
		void setBinary(const UInt8* data, size_t length);
		/// Sets the value of this number based on a binary data block.
		void setBinary(const std::vector<UInt8> &data);
		/// Sets the value of this number based on a hex string.
		void setHexStr(const String &string);
		/// Randomizes the value.
		void setRand(int numBits);
		/// Determines whether the value of this number is equal to zero.
		bool isZero() const;
		/// Modular exponentiation.
		BigNumber modExp(const BigNumber &bn1, const BigNumber &bn2) const;
		/// Exponential function.
		BigNumber exp(const BigNumber &Other) const;
		/// Gets the number of bytes this number needs.
		UInt32 getNumBytes() const;
		/// Converts the value of this number to a 32 bit unsigned integer.
		UInt32 asUInt32() const;
		/// Returns an array of bytes which represent the value of this number.
		std::vector<UInt8> asByteArray(int minSize = 0) const;
		/// Returns a string of this number in it's hexadecimal form.
		String asHexStr() const;
		/// Returns a string of this number in it's decimal form.
		String asDecStr() const;

	public:

		BigNumber operator=(const BigNumber &Other);
		BigNumber operator+=(const BigNumber &Other);
		BigNumber operator+(const BigNumber &Other)
		{
			BigNumber t(*this);
			return t += Other;
		}
		BigNumber operator-=(const BigNumber &Other);
		BigNumber operator-(const BigNumber &Other)
		{
			BigNumber t(*this);
			return t -= Other;
		}
		BigNumber operator*=(const BigNumber &Other);
		BigNumber operator*(const BigNumber &Other)
		{
			BigNumber t(*this);
			return t *= Other;
		}
		BigNumber operator/=(const BigNumber &Other);
		BigNumber operator/(const BigNumber &Other)
		{
			BigNumber t(*this);
			return t /= Other;
		}
		BigNumber operator%=(const BigNumber &Other);
		BigNumber operator%(const BigNumber &Other)
		{
			BigNumber t(*this);
			return t %= Other;
		}

	private:

		bignum_st *m_bn;
	};
}
