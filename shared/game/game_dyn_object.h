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

#include "game_object.h"
#include "common/timer_queue.h"
#include "common/countdown.h"
#include "tiled_unit_watcher.h"

namespace wowpp
{
	namespace proto
	{
		class SpellEntry;
		class SpellEffect;
	}

	class GameUnit;
	class AuraEffect;
	class AuraSpellSlot;

	namespace dyn_object_fields
	{
		enum Enum
		{
			/// 64 bit guid of the caster of this spell.
			Caster					= object_fields::ObjectFieldCount + 0x00,
			/// Unknown
			Bytes					= object_fields::ObjectFieldCount + 0x02,
			/// Id of the spell that created this object (also used to determine visual effect).
			SpellId					= object_fields::ObjectFieldCount + 0x03,
			/// Radius of the area effect in yards.
			Radius					= object_fields::ObjectFieldCount + 0x04,
			/// X position in the world.
			PosX					= object_fields::ObjectFieldCount + 0x05,
			/// Y position in the world (height).
			PosY					= object_fields::ObjectFieldCount + 0x06,
			/// Z position in the world.
			PosZ					= object_fields::ObjectFieldCount + 0x07,
			/// Orientation (radians) in the world.
			Facing					= object_fields::ObjectFieldCount + 0x08,
			/// Creation timestamp of this object (used for animation display).
			CastTime				= object_fields::ObjectFieldCount + 0x09,

			DynObjectFieldCount		= object_fields::ObjectFieldCount + 0x0A
		};
	}

	/// Represents a visual object which is created by spells (most likely by persistent area auras
	/// like Blizzard). This object is also used as a channel target.
	class DynObject : public GameObject
	{
		friend io::Writer &operator << (io::Writer &w, DynObject const &object);
		friend io::Reader &operator >> (io::Reader &r, DynObject &object);

	public:

		/// Constructor
		explicit DynObject(
		    proto::Project &project,
		    TimerQueue &timers,
			GameUnit &caster,
			const proto::SpellEntry &entry,
			const proto::SpellEffect &effect);
		/// Default destructor
		virtual ~DynObject();

		/// @copydoc GameObject::initialize()
		void initialize() override;
		/// @copydoc GameObject::relocate()
		void relocate(const math::Vector3 &position, float o, bool fire = true) override;
		/// @copydoc GameObject::writeCreateObjectBlocks()
		void writeCreateObjectBlocks(std::vector<std::vector<char>> &out_blocks, bool creation = true) const override;

		/// Gets the object type id value.
		ObjectType getTypeId() const override {
			return object_type::DynamicObject;
		}

	public:

		/// Despawns this object after a duration.
		/// @param duration The lifetime of this object in milliseconds.
		void triggerDespawnTimer(UInt64 duration);
		/// Gets the caster that this dynamic object belongs to. This object will automatically
		/// be destroyed and despawn once the caster despawns.
		GameUnit &getCaster() const { return m_caster; }
		/// Gets the spell entry that created this dynamic object.
		const proto::SpellEntry &getEntry() const { return m_entry; }
		/// Gets the spell effect that created this dynamic object.
		const proto::SpellEffect &getEffect() const { return m_effect; }
		/// Starts the unit watcher if needed.
		void startUnitWatcher();
		///
		void updatePeriodicTimer();

	protected:
		typedef std::unordered_map<GameUnit *, std::shared_ptr<AuraSpellSlot>> AuraMap;

		std::unique_ptr<UnitWatcher> m_unitWatcher;
		TimerQueue &m_timers;
		GameUnit &m_caster;
		const proto::SpellEntry &m_entry;
		const proto::SpellEffect &m_effect;
		Countdown m_despawnTimer;
		Countdown m_tickCountdown;
		simple::scoped_connection m_onTick, m_onDespawn;
		AuraMap m_auras;
	};

	io::Writer &operator << (io::Writer &w, DynObject const &object);
	io::Reader &operator >> (io::Reader &r, DynObject &object);
}
