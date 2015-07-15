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
#include "game/defines.h"
#include "binary_io/writer.h"
#include "binary_io/reader.h"

namespace wowpp
{
	class GameObject;
	class GameUnit;
	class GameItem;
	class WorldInstance;

	/// TODO: Cleanup this class
	class SpellTargetMap final
	{
		friend io::Writer &operator << (io::Writer &w, SpellTargetMap const& targetMap);
		friend io::Reader &operator >> (io::Reader &r, SpellTargetMap& targetMap);

	public:

		explicit SpellTargetMap();
		SpellTargetMap(const SpellTargetMap &other);

		bool resolvePointers(WorldInstance &instance, GameUnit **out_unitTarget, GameItem **out_itemTarget, GameObject **out_objectTarget, GameObject **out_corpseTarget);

		/// 
		UInt32 getTargetMap() const { return m_targetMap; }
		/// 
		UInt64 getUnitTarget() const { return m_unitTarget; }
		/// 
		UInt64 getGOTarget() const { return m_goTarget; }
		/// 
		UInt64 getItemTarget() const { return m_itemTarget; }
		/// 
		UInt64 getCorpseTarget() const { return m_corpseTarget; }
		/// 
		void getSourceLocation(float &out_X, float &out_Y, float &out_Z) const;
		/// 
		void getDestLocation(float &out_X, float &out_Y, float &out_Z) const;
		/// 
		const String &getStringTarget() const { return m_stringTarget; }
		/// Determines whether a unit target guid is provided.
		const bool hasUnitTarget() const { return m_unitTarget != 0; }
		/// Determines whether a game object target guid is provided.
		const bool hasGOTarget() const { return m_goTarget != 0; }
		/// Determines whether an item target guid is provided.
		const bool hasItemTarget() const { return m_itemTarget != 0; }
		/// Determines whether a corpse target guid is provided.
		const bool hasCorpseTarget() const { return m_corpseTarget != 0; }
		/// Determines whether a source location is provided.
		const bool hasSourceTarget() const { return (m_targetMap & game::spell_cast_target_flags::SourceLocation) != 0; }
		/// Determines whether a dest location is provided.
		const bool hasDestTarget() const { return (m_targetMap & game::spell_cast_target_flags::DestLocation) != 0; }
		/// Determines whether a string target is provided.
		const bool hasStringTarget() const { return (m_targetMap & game::spell_cast_target_flags::String) != 0; }

		virtual SpellTargetMap &operator =(const SpellTargetMap &other);

	public:

		UInt32 m_targetMap;
		UInt64 m_unitTarget;
		UInt64 m_goTarget;
		UInt64 m_itemTarget;
		UInt64 m_corpseTarget;
		float m_srcX, m_srcY, m_srcZ;
		float m_dstX, m_dstY, m_dstZ;
		String m_stringTarget;
	};

	io::Writer &operator << (io::Writer &w, SpellTargetMap const& targetMap);
	io::Reader &operator >> (io::Reader &r, SpellTargetMap& targetMap);
}
