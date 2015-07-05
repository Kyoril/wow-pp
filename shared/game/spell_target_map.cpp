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

#include "spell_target_map.h"

namespace wowpp
{
	SpellTargetMap::SpellTargetMap()
		: m_targetMap(0)
		, m_unitTarget(0)
		, m_goTarget(0)
		, m_itemTarget(0)
		, m_corpseTarget(0)
		, m_srcX(0.0f), m_srcY(0.0f), m_srcZ(0.0f)
		, m_dstX(0.0f), m_dstY(0.0f), m_dstZ(0.0f)
	{
	}

	SpellTargetMap::SpellTargetMap(const SpellTargetMap &other)
		: m_targetMap(other.m_targetMap)
		, m_unitTarget(other.m_unitTarget)
		, m_goTarget(other.m_goTarget)
		, m_itemTarget(other.m_itemTarget)
		, m_corpseTarget(other.m_corpseTarget)
		, m_srcX(other.m_srcX), m_srcY(other.m_srcY), m_srcZ(other.m_srcZ)
		, m_dstX(other.m_dstX), m_dstY(other.m_dstY), m_dstZ(other.m_dstZ)
		, m_stringTarget(other.m_stringTarget)
	{
	}

	void SpellTargetMap::getSourceLocation(float &out_X, float &out_Y, float &out_Z) const
	{
		out_X = m_srcX;
		out_Y = m_srcY;
		out_Z = m_srcZ;
	}

	void SpellTargetMap::getDestLocation(float &out_X, float &out_Y, float &out_Z) const
	{
		out_X = m_dstX;
		out_Y = m_dstY;
		out_Z = m_dstZ;
	}

	SpellTargetMap & SpellTargetMap::operator=(const SpellTargetMap &other)
	{
		m_targetMap = other.m_targetMap;
		m_unitTarget = other.m_unitTarget;
		m_goTarget = other.m_goTarget;
		m_itemTarget = other.m_itemTarget;
		m_corpseTarget = other.m_corpseTarget;
		m_srcX = other.m_srcX;
		m_srcY = other.m_srcY;
		m_srcZ = other.m_srcZ;
		m_dstX = other.m_dstX;
		m_dstY = other.m_dstY;
		m_dstZ = other.m_dstZ;
		m_stringTarget = other.m_stringTarget;
		return *this;
	}


	io::Reader & operator>>(io::Reader &r, SpellTargetMap& targetMap)
	{
		if (!(r >> io::read<NetUInt32>(targetMap.m_targetMap)))
		{
			return r;
		}

		// No targets
		if (targetMap.m_targetMap == game::spell_cast_target_flags::Self)
			return r;

		// Unit target
		if (targetMap.m_targetMap & (game::spell_cast_target_flags::Unit | game::spell_cast_target_flags::Unk2))
		{
			targetMap.m_unitTarget = 0;
			UInt8 guidMark = 0;
			if (!(r >> io::read<NetUInt8>(guidMark)))
			{
				return r;
			}

			for (int i = 0; i < 8; ++i)
			{
				if (guidMark & (static_cast<UInt8>(1) << i))
				{
					UInt8 bit;
					if (!(r >> io::read<NetUInt8>(bit)))
					{
						return r;
					}

					targetMap.m_unitTarget |= (static_cast<UInt64>(bit) << (i * 8));
				}
			}
		}

		// Object target
		if (targetMap.m_targetMap & (game::spell_cast_target_flags::Object | game::spell_cast_target_flags::ObjectUnknown))
		{
			targetMap.m_goTarget = 0;
			UInt8 guidMark = 0;
			if (!(r >> io::read<NetUInt8>(guidMark)))
			{
				return r;
			}

			for (int i = 0; i < 8; ++i)
			{
				if (guidMark & (static_cast<UInt8>(1) << i))
				{
					UInt8 bit;
					if (!(r >> io::read<NetUInt8>(bit)))
					{
						return r;
					}
					targetMap.m_goTarget |= (static_cast<UInt64>(bit) << (i * 8));
				}
			}
		}

		// Item target
		if (targetMap.m_targetMap & (game::spell_cast_target_flags::Item | game::spell_cast_target_flags::TradeItem))	// TODO: AND isPlayer(caster)
		{
			targetMap.m_itemTarget = 0;
			UInt8 guidMark = 0;
			if (!(r >> io::read<NetUInt8>(guidMark)))
			{
				return r;
			}

			for (int i = 0; i < 8; ++i)
			{
				if (guidMark & (static_cast<UInt8>(1) << i))
				{
					UInt8 bit;
					if (!(r >> io::read<NetUInt8>(bit)))
					{
						return r;
					}
					targetMap.m_itemTarget |= (static_cast<UInt64>(bit) << (i * 8));
				}
			}
		}

		// Source location target
		if (targetMap.m_targetMap & game::spell_cast_target_flags::SourceLocation)
		{
			if (!(r
				>> io::read<float>(targetMap.m_srcX)
				>> io::read<float>(targetMap.m_srcY)
				>> io::read<float>(targetMap.m_srcZ)))
			{
				return r;
			}
		}

		// Dest location target
		if (targetMap.m_targetMap & game::spell_cast_target_flags::DestLocation)
		{
			if (!(r
				>> io::read<float>(targetMap.m_srcX)
				>> io::read<float>(targetMap.m_srcY)
				>> io::read<float>(targetMap.m_srcZ)))
			{
				return r;
			}
		}

		// String target
		if (targetMap.m_targetMap & game::spell_cast_target_flags::String)
		{
			char c = 0x00;
			do
			{
				if (!(r >> c))
				{
					return r;
				}
				if (c != 0)
				{
					targetMap.m_stringTarget.push_back(c);
				}
			} while (c != 0);
		}

		// Corpse target
		if (targetMap.m_targetMap & (game::spell_cast_target_flags::Corpse | game::spell_cast_target_flags::PvPCorpse))
		{
			targetMap.m_corpseTarget = 0;
			UInt8 guidMark = 0;
			if (!(r >> io::read<NetUInt8>(guidMark)))
			{
				return r;
			}

			for (int i = 0; i < 8; ++i)
			{
				if (guidMark & (static_cast<UInt8>(1) << i))
				{
					UInt8 bit;
					if (!(r >> io::read<NetUInt8>(bit)))
					{
						return r;
					}

					targetMap.m_corpseTarget |= (static_cast<UInt64>(bit) << (i * 8));
				}
			}
		}

		return r;
	}

	io::Writer & operator<<(io::Writer &w, SpellTargetMap const& targetMap)
	{
		namespace scf = game::spell_cast_target_flags;

		// Write mask
		w << io::write<NetUInt32>(targetMap.m_targetMap);

		// Write GUID target values
		if (targetMap.m_targetMap & (scf::Unit | scf::PvPCorpse | scf::Object | scf::Corpse | scf::Unk2))
		{
			if (targetMap.m_targetMap & scf::Unit)
			{
				// Write packed Unit GUID
				UInt64 guidCopy = targetMap.m_unitTarget;
				UInt8 packGUID[8 + 1];
				packGUID[0] = 0;
				size_t size = 1;
				for (UInt8 i = 0; guidCopy != 0; ++i)
				{
					if (guidCopy & 0xFF)
					{
						packGUID[0] |= UInt8(1 << i);
						packGUID[size] = UInt8(guidCopy & 0xFF);
						++size;
					}

					guidCopy >>= 8;
				}
				w.sink().write((const char*)&packGUID[0], size);
			}
			else if (targetMap.m_targetMap & (scf::Object | scf::ObjectUnknown))
			{
				// Write packed GO GUID
				UInt64 guidCopy = targetMap.m_goTarget;
				UInt8 packGUID[8 + 1];
				packGUID[0] = 0;
				size_t size = 1;
				for (UInt8 i = 0; guidCopy != 0; ++i)
				{
					if (guidCopy & 0xFF)
					{
						packGUID[0] |= UInt8(1 << i);
						packGUID[size] = UInt8(guidCopy & 0xFF);
						++size;
					}

					guidCopy >>= 8;
				}
				w.sink().write((const char*)&packGUID[0], size);
			}
			else if (targetMap.m_targetMap & (scf::Corpse | scf::PvPCorpse))
			{
				// Write packed corpse GUID
				UInt64 guidCopy = targetMap.m_corpseTarget;
				UInt8 packGUID[8 + 1];
				packGUID[0] = 0;
				size_t size = 1;
				for (UInt8 i = 0; guidCopy != 0; ++i)
				{
					if (guidCopy & 0xFF)
					{
						packGUID[0] |= UInt8(1 << i);
						packGUID[size] = UInt8(guidCopy & 0xFF);
						++size;
					}

					guidCopy >>= 8;
				}
				w.sink().write((const char*)&packGUID[0], size);
			}
			else
			{
				// No GUID
				w << io::write<NetUInt8>(0);
			}
		}

		// Item GUID
		if (targetMap.m_targetMap & (scf::Item | scf::TradeItem))
		{
			// Write packed item GUID
			UInt64 guidCopy = targetMap.m_itemTarget;
			UInt8 packGUID[8 + 1];
			packGUID[0] = 0;
			size_t size = 1;
			for (UInt8 i = 0; guidCopy != 0; ++i)
			{
				if (guidCopy & 0xFF)
				{
					packGUID[0] |= UInt8(1 << i);
					packGUID[size] = UInt8(guidCopy & 0xFF);
					++size;
				}

				guidCopy >>= 8;
			}
			w.sink().write((const char*)&packGUID[0], size);
		}
		
		// Source location
		if (targetMap.m_targetMap & scf::SourceLocation)
		{
			w
				<< io::write<float>(targetMap.m_srcX)
				<< io::write<float>(targetMap.m_srcY)
				<< io::write<float>(targetMap.m_srcZ);
		}

		// Dest location
		if (targetMap.m_targetMap & scf::DestLocation)
		{
			w
				<< io::write<float>(targetMap.m_dstX)
				<< io::write<float>(targetMap.m_dstY)
				<< io::write<float>(targetMap.m_dstZ);
		}

		// String target
		if (targetMap.m_targetMap & scf::String)
		{
			w
				<< io::write_range(targetMap.m_stringTarget) << io::write<NetUInt8>(0);
		}

		return w;
	}
}
