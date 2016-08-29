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
#include "game_object.h"
#include "game_character.h"
#include "game_creature.h"
#include "game_world_object.h"
#include "loot_instance.h"
#include "log/default_log_levels.h"
#include "binary_io/vector_sink.h"
#include "common/clock.h"
#include "visibility_tile.h"
#include "world_instance.h"
#include "proto_data/project.h"

namespace wowpp
{
	GameObject::GameObject(proto::Project &project)
		: m_project(project)
		, m_mapId(0)
		, m_o(0.0f)
		, m_objectType(0x01)
		, m_objectTypeId(0x01)
		, m_worldInstance(nullptr)
	{
		m_values.resize(object_fields::ObjectFieldCount, 0);
		m_valueBitset.resize((object_fields::ObjectFieldCount + 31) / 32, 0);
	}

	GameObject::~GameObject()
	{
	}

	void GameObject::initialize()
	{
		// TODO
	}

	wowpp::UInt8 GameObject::getByteValue(UInt16 index, UInt8 offset) const
	{
		assert(offset < 4);
		assert(index < m_values.size());
		return *((reinterpret_cast<const UInt8 *>(&m_values[index])) + offset);
	}

	wowpp::UInt16 GameObject::getUInt16Value(UInt16 index, UInt8 offset) const
	{
		assert(offset < 2);
		assert(index < m_values.size());
		return *((reinterpret_cast<const UInt16 *>(&m_values[index])) + offset);
	}

	wowpp::Int32 GameObject::getInt32Value(UInt16 index) const
	{
		assert(index < m_values.size());
		return *(reinterpret_cast<const Int32 *>(&m_values[index]));
	}

	wowpp::UInt32 GameObject::getUInt32Value(UInt16 index) const
	{
		assert(index < m_values.size());
		return m_values[index];
	}

	wowpp::UInt64 GameObject::getUInt64Value(UInt16 index) const
	{
		assert(index + 1 < m_values.size());
		return *(reinterpret_cast<const UInt64 *>(&m_values[index]));
	}

	float GameObject::getFloatValue(UInt16 index) const
	{
		assert(index < m_values.size());
		return *(reinterpret_cast<const float *>(&m_values[index]));
	}

	void GameObject::setByteValue(UInt16 index, UInt8 offset, UInt8 value)
	{
		assert(index < m_values.size());
		assert(offset < 4);

		if (UInt8(m_values[index] >> (offset * 8)) != value)
		{
			m_values[index] &= ~UInt32(UInt32(0xFF) << (offset * 8));
			m_values[index] |= UInt32(UInt32(value) << (offset * 8));

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8 *>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			markForUpdate();
		}
	}

	void GameObject::setUInt16Value(UInt16 index, UInt8 offset, UInt16 value)
	{
		assert(index < m_values.size());
		assert(offset < 2);

		if (UInt16(m_values[index] >> (offset * 16)) != value)
		{
			m_values[index] &= ~UInt32(UInt32(0xFFFF) << (offset * 16));
			m_values[index] |= UInt32(UInt32(value) << (offset * 16));

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8 *>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			markForUpdate();
		}
	}

	void GameObject::setUInt32Value(UInt16 index, UInt32 value)
	{
		assert(index < m_values.size());

		if (m_values[index] != value)
		{
			// Update value
			m_values[index] = value;

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8 *>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			markForUpdate();
		}
	}

	void GameObject::forceFieldUpdate(UInt16 index)
	{
		assert(index < m_values.size());
		UInt16 bitIndex = index >> 3;

		// Mark bit as changed
		UInt8 &changed = (reinterpret_cast<UInt8 *>(&m_valueBitset[0]))[bitIndex];
		changed |= 1 << (index & 0x7);

		markForUpdate();
	}

	void GameObject::addFlag(UInt16 index, UInt32 flag)
	{
		assert(index < m_values.size());

		UInt32 newValue = m_values[index] | flag;
		if (newValue != m_values[index])
		{
			m_values[index] = newValue;

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8 *>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			markForUpdate();
		}
	}

	void GameObject::removeFlag(UInt16 index, UInt32 flag)
	{
		assert(index < m_values.size());

		UInt32 newValue = m_values[index] & ~flag;
		if (newValue != m_values[index])
		{
			m_values[index] = newValue;
			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8 *>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			markForUpdate();
		}
	}

	bool GameObject::hasFlag(UInt16 index, UInt32 flag)
	{
		assert(index < m_values.size()); //TODO
		return (m_values[index] & flag) != 0;
	}

	void GameObject::markForUpdate()
	{
		if (!m_updated)
		{
			if (m_worldInstance)
			{
				m_worldInstance->addUpdateObject(*this);
				m_updated = true;
			}
		}
	}

	void GameObject::setInt32Value(UInt16 index, Int32 value)
	{
		assert(index < m_values.size());

		Int32 *tmp = reinterpret_cast<Int32 *>(&m_values[index]);
		if (*tmp != value)
		{
			// Update value
			*tmp = value;

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8 *>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			markForUpdate();
		}
	}

	void GameObject::setUInt64Value(UInt16 index, UInt64 value)
	{
		assert(index + 1 < m_values.size());

		UInt64 *tmp = reinterpret_cast<UInt64 *>(&m_values[index]);
		if (*tmp != value)
		{
			setUInt32Value(index, *(reinterpret_cast<UInt32 *>(&value)));
			setUInt32Value(index + 1, *(reinterpret_cast<UInt32 *>(&value) + 1));
		}
	}

	void GameObject::setFloatValue(UInt16 index, float value)
	{
		assert(index < m_values.size());

		float *tmp = reinterpret_cast<float *>(&m_values[index]);
		if (*tmp != value)
		{
			// Update value
			*tmp = value;

			UInt16 bitIndex = index >> 3;

			// Mark bit as changed
			UInt8 &changed = (reinterpret_cast<UInt8 *>(&m_valueBitset[0]))[bitIndex];
			changed |= 1 << (index & 0x7);

			markForUpdate();
		}
	}

	void GameObject::relocate(const math::Vector3 &position, float o, bool fire/* = true*/)
	{
		float oldO = m_o;

		m_position = position;
		m_o = o;

		if (fire)
		{
            auto lastFiredPosition = m_lastFiredPosition;
            m_lastFiredPosition = m_position;
    
			moved(*this, lastFiredPosition, oldO);
        }
	}

	void GameObject::setOrientation(float o)
	{
		float oldO = m_o;

		m_o = o;

		moved(*this, m_position, oldO);
	}

	void GameObject::setMapId(UInt32 mapId)
	{
		m_mapId = mapId;
	}

	void GameObject::writeCreateObjectBlocks(std::vector<std::vector<char>> &out_blocks, bool creation /*= true*/) const
	{
		// TODO
	}

	void GameObject::writeUpdateValue(io::Writer &writer, GameCharacter &receiver, UInt16 index) const
	{
		if (isCreature())
		{
			switch (index)
			{
			case unit_fields::NpcFlags:
				{
					UInt32 value = m_values[index];
					if ((value & game::unit_npc_flags::Trainer) != 0)
					{
						const GameCreature *creature = static_cast<const GameCreature *>(this);
						const auto *trainerEntry = getProject().trainers.getById(creature->getEntry().trainerentry());
						if (!trainerEntry ||
						        ((trainerEntry->type() == proto::TrainerEntry_TrainerType_CLASS_TRAINER) && trainerEntry->classid() != receiver.getClass()))
						{
							value &= ~game::unit_npc_flags::Trainer;
							value &= ~game::unit_npc_flags::TrainerClass;
							value &= ~game::unit_npc_flags::TrainerProfession;
						}
					}
					writer << io::write<NetUInt32>(value);
					break;
				}
			case unit_fields::DynamicFlags:
				{
					const GameCreature *creature = static_cast<const GameCreature *>(this);
					if (!creature->isLootRecipient(receiver))
					{
						// Creature not lootable because we are not a loot recipient
						writer << io::write<NetUInt32>(m_values[index] & ~game::unit_dynamic_flags::Lootable);
					}
					else
					{
						UInt32 value = m_values[index] & ~game::unit_dynamic_flags::OtherTagger;

						// Creature is lootable, but only as long as the creature has loot for us
						auto *loot = creature->getUnitLoot();
						if (!loot ||
							!loot->containsLootFor(receiver.getGuid()))
						{
							// No longer lootable
							value &= ~game::unit_dynamic_flags::Lootable;
						}

						// Write the value
						writer << io::write<NetUInt32>(value);
					}
					break;
				}
			case unit_fields::MaxHealth:
				{
					if (getUInt64Value(unit_fields::SummonedBy) == receiver.getGuid())
					{
						writer << io::write<NetUInt32>(m_values[index]);
					}
					else
					{
						writer << io::write<NetUInt32>(100);
					}
					break;
				}
			case unit_fields::Health:
				{
					if (getUInt64Value(unit_fields::SummonedBy) == receiver.getGuid())
					{
						writer << io::write<NetUInt32>(m_values[index]);
					}
					else
					{
						float maxHealth = static_cast<float>(m_values[unit_fields::MaxHealth]);
						UInt32 healthPct = static_cast<UInt32>(::ceil(static_cast<float>(m_values[index]) / maxHealth * 100.0f));

						// Prevent display of 0
						if (healthPct == 0 && m_values[index] > 0) {
							healthPct = 1;
						}
						writer << io::write<NetUInt32>(healthPct);
					}
					break;
				}
			default:
				writer << io::write<NetUInt32>(m_values[index]);
				break;
			}
		}
		else if (isGameCharacter())
		{
			const auto *character = reinterpret_cast<const GameCharacter *>(this);
			const bool isHealthVisible = (character == &receiver || (character->getGroupId() != 0 && character->getGroupId() == receiver.getGroupId()));

			switch (index)
			{
			case unit_fields::MaxHealth:
				{
					if (!isHealthVisible)
					{
						writer << io::write<NetUInt32>(100);
					}
					else
					{
						writer << io::write<NetUInt32>(m_values[index]);
					}
					break;
				}
			case unit_fields::Health:
				{
					if (!isHealthVisible)
					{
						float maxHealth = static_cast<float>(m_values[unit_fields::MaxHealth]);
						UInt32 healthPct = static_cast<UInt32>(::ceil(static_cast<float>(m_values[index]) / maxHealth * 100.0f));

						// Prevent display of 0
						if (healthPct == 0 && m_values[index] > 0) {
							healthPct = 1;
						}
						writer << io::write<NetUInt32>(healthPct);
					}
					else
					{
						writer << io::write<NetUInt32>(m_values[index]);
					}
					break;
				}
			default:
				writer << io::write<NetUInt32>(m_values[index]);
				break;
			}
		}
		else if (isWorldObject())
		{
			switch (index)
			{
			case world_object_fields::DynFlags:
				{
					// Determine whether this object is active as a quest object
					const WorldObject *worldObject = reinterpret_cast<const WorldObject *>(this);
					if (worldObject->isQuestObject(receiver))
					{
						writer
						        << io::write<NetUInt16>(1 | 8)
						        << io::write<NetUInt16>(-1);
						//						ILOG("\tWORLD OBJECT FIELD " << index << ": 0x" << std::hex << std::uppercase << UInt32(UInt32(1 | 8) | UInt32(UInt16(-1) << 16)));
					}
					else
					{
						//						ILOG("\tWORLD OBJECT FIELD " << index << ": 0x" << std::hex << std::uppercase << 0);
						writer << io::write<NetUInt32>(0);
					}
					break;
				}
			default:
				{
					//					ILOG("\tWORLD OBJECT FIELD " << index << ": 0x" << std::hex << std::uppercase << m_values[index]);
					writer << io::write<NetUInt32>(m_values[index]);
					break;
				}
			}
		}
		else
		{
			writer << io::write<NetUInt32>(m_values[index]);
		}
	}

	void GameObject::writeValueUpdateBlock(io::Writer &writer, GameCharacter &receiver, bool creation /*= true*/) const
	{
		// Number of UInt32 blocks used to represent all values as one bit
		UInt8 blockCount = m_valueBitset.size();
		writer
		        << io::write<NetUInt8>(blockCount);

		if (creation)
		{
			const bool isWorldGO = isWorldObject();
			//			if (isWorldGO)
			//			{
			//				ILOG("CREATE_OBJECT_VALUES");
			//			}

			// Create a new bitset which has set all bits to one where the field value
			// isn't equal to 0, since this will be for a CREATE_OBJECT block (spawn) and not
			// for an UPDATE_OBJECT block
			std::vector<UInt32> creationBits(m_valueBitset.size(), 0);
			for (size_t i = 0; i < m_values.size(); ++i)
			{
				if (m_values[i] ||
				        (isWorldGO && i == world_object_fields::DynFlags))
				{
					// Calculate bit index
					UInt16 bitIndex = i >> 3;

					// Mark bit as changed
					UInt8 &changed = (reinterpret_cast<UInt8 *>(&creationBits[0]))[bitIndex];
					changed |= 1 << (i & 0x7);
				}
			}

			// Write only the changed bits
			writer
			        << io::write_range(creationBits);

			// Write all values != zero
			for (size_t i = 0; i < m_values.size(); ++i)
			{
				if (m_values[i] != 0 ||
				        (isWorldGO && i == world_object_fields::DynFlags))
				{
					writeUpdateValue(writer, receiver, i);
				}
			}
		}
		else
		{
			// Write only the changed bits
			writer
			        << io::write_range(m_valueBitset);

			// Write all values marked as changed
			for (size_t i = 0; i < m_values.size(); ++i)
			{
				const UInt8 &changed = (reinterpret_cast<const UInt8 *>(&m_valueBitset[0]))[i >> 3];
				if ((changed & (1 << (i & 0x7))) != 0)
				{
					writeUpdateValue(writer, receiver, i);
				}
			}
		}
	}

	void GameObject::clearUpdateMask()
	{
		std::fill(m_valueBitset.begin(), m_valueBitset.end(), 0);
		m_updated = false;
	}

	float GameObject::getDistanceTo(const GameObject &other, bool use3D/* = true*/) const
	{
		if (&other == this) {
			return 0.0f;
		}

		math::Vector3 position = other.getLocation();
		return getDistanceTo(position, use3D);
	}

	float GameObject::getDistanceTo(const math::Vector3 &position, bool use3D /*= true*/) const
	{
		if (use3D) {
			return (sqrtf(((position.x - m_position.x) * (position.x - m_position.x)) + ((position.y - m_position.y) * (position.y - m_position.y)) + ((position.z - m_position.z) * (position.z - m_position.z))));
		}
		else {
			return (sqrtf(((position.x - m_position.x) * (position.x - m_position.x)) + ((position.y - m_position.y) * (position.y - m_position.y))));
		}
	}

	bool GameObject::canSpawnForCharacter(GameCharacter & target) const
	{
		return true;
	}
	
	void GameObject::onWorldInstanceDestroyed()
	{
		// We are no longer part of the old world instance
		setWorldInstance(nullptr);
	}

	void GameObject::setWorldInstance(WorldInstance *instance)
	{
		m_worldInstanceDestroyed.disconnect();

		// Use new instance
		m_worldInstance = instance;

		// Watch for destruction of the new world instance
		if (m_worldInstance)
		{
			m_worldInstanceDestroyed = m_worldInstance->willBeDestroyed.connect(
			                               std::bind(&GameObject::onWorldInstanceDestroyed, this));
		}
	}

	bool GameObject::getTileIndex(TileIndex2D &out_index) const
	{
		// This object has to be in a world instance
		if (!m_worldInstance)
		{
			return false;
		}

		// Try to resolve the objects position
		return m_worldInstance->getGrid().getTilePosition(m_position, out_index[0], out_index[1]);
	}

	bool GameObject::isInArc(float arcRadian, float x, float y) const
	{
		if (x == m_position.x && y == m_position.y) {
			return true;
		}

		const float PI = 3.1415927f;
		float arc = arcRadian;

		// move arc to range 0.. 2*pi
		while (arc >= 2.0f * PI) {
			arc -= 2.0f * PI;
		}
		while (arc < 0) {
			arc += 2.0f * PI;
		}

		float angle = getAngle(x, y);
		angle -= m_o;

		// move angle to range -pi ... +pi
		while (angle > PI) {
			angle -= 2.0f * PI;
		}
		while (angle < -PI) {
			angle += 2.0f * PI;
		}

		float lborder = -1 * (arc / 2.0f);                     // in range -pi..0
		float rborder = (arc / 2.0f);                           // in range 0..pi
		return ((angle >= lborder) && (angle <= rborder));
	}

	float GameObject::getAngle(GameObject &other) const
	{
		return getAngle(other.m_position.x, other.m_position.y);
	}

	float GameObject::getAngle(float x, float y) const
	{
		float dx = x - m_position.x;
		float dy = y - m_position.y;

		float ang = ::atan2(dy, dx);
		ang = (ang >= 0) ? ang : 2 * 3.1415927f + ang;
		return ang;
	}

	io::Writer &operator<<(io::Writer &w, GameObject const &object)
	{
		// Write the bitset and values
		return w
		       << io::write_dynamic_range<NetUInt8>(object.m_valueBitset)
		       << io::write_dynamic_range<NetUInt16>(object.m_values)
		       << io::write<NetUInt32>(object.m_mapId)
		       << io::write<float>(object.m_position.x)
		       << io::write<float>(object.m_position.y)
		       << io::write<float>(object.m_position.z)
		       << io::write<float>(object.m_o);
	}

	io::Reader &operator>>(io::Reader &r, GameObject &object)
	{
		// Read the bitset and values
		r
		        >> io::read_container<NetUInt8>(object.m_valueBitset)
		        >> io::read_container<NetUInt16>(object.m_values)
		        >> io::read<NetUInt32>(object.m_mapId)
		        >> io::read<float>(object.m_position.x)
		        >> io::read<float>(object.m_position.y)
		        >> io::read<float>(object.m_position.z)
		        >> io::read<float>(object.m_o);

		object.m_lastFiredPosition = object.m_position;
		return r;
	}

	void createUpdateBlocks(GameObject &object, GameCharacter &receiver, std::vector<std::vector<char>> &out_blocks)
	{
		float o = object.getOrientation();
		math::Vector3 location(object.getLocation());

		// Write create object packet
		std::vector<char> createBlock;
		io::VectorSink sink(createBlock);
		io::Writer writer(sink);
		{
			UInt8 updateType = 0x02;						// Update type (0x02 = CREATE_OBJECT)
			if (object.isGameCharacter() ||
			        object.getTypeId() == object_type::Corpse ||
			        object.getTypeId() == object_type::DynamicObject)
			{
				updateType = 0x03;		// CREATE_OBJECT_2
			}
			if (object.isCreature() && 
				object.getUInt64Value(unit_fields::CreatedBy) == receiver.getGuid())
			{
				updateType = 0x03;		// CREATE_OBJECT_2
				ILOG("PET CREATE: CREATE_OBJECT_2");
			}
			if (object.getTypeId() == object_type::GameObject)
			{
				UInt32 objType = object.getUInt32Value(world_object_fields::TypeID);
				switch (objType)
				{
				case 6:
				case 16:
				case 24:
				case 26:
					updateType = 0x03;		// CREATE_OBJECT_2
					break;
				default:
					break;
				}
			}

			UInt8 updateFlags = 0x10 | 0x20 | 0x40;			// UPDATEFLAG_ALL | UPDATEFLAG_LIVING | UPDATEFLAG_HAS_POSITION
			UInt8 objectTypeId = object.getTypeId();		//
			if (objectTypeId == object_type::GameObject ||
				objectTypeId == object_type::DynamicObject)
			{
				updateFlags = 0x08 | 0x10 | 0x40;	// Low-GUID, High-GUID & HasPosition
			}
			else if (
			    objectTypeId == object_type::Item ||
			    objectTypeId == object_type::Container)
			{
				updateFlags = 0x08 | 0x10;			// Low-GUID & High-GUID
			}

			// Header with object guid and type
			UInt64 guid = object.getGuid();
			writer
				<< io::write<NetUInt8>(updateType)
				<< io::write_packed_guid(guid)
				<< io::write<NetUInt8>(objectTypeId)
				<< io::write<NetUInt8>(updateFlags);

			// Write movement update
			auto &movement = object.getMovementInfo();
			UInt32 moveFlags = movement.moveFlags;
			if (updateFlags & 0x20)
			{
				writer
				        << io::write<NetUInt32>(moveFlags)
				        << io::write<NetUInt8>(0x00)
				        << io::write<NetUInt32>(mTimeStamp());
			}

			if (updateFlags & 0x40)
			{
				// Position & Rotation
				// TODO: Calculate position and rotation from the movement info using interpolation
				writer
				        << io::write<float>(location.x)
				        << io::write<float>(location.y)
				        << io::write<float>(location.z)
				        << io::write<float>(o);
			}

			if (updateFlags & 0x20)
			{
				// Transport
				if (moveFlags & (game::movement_flags::OnTransport))
				{
					//TODO
					WLOG("TODO");
				}

				// Pitch info
				if (moveFlags & (game::movement_flags::Swimming | game::movement_flags::Flying2))
				{
					if (object.isGameCharacter())
					{
						writer
						        << io::write<float>(movement.pitch);
					}
					else
					{
						writer
						        << io::write<float>(0.0f);
					}
				}

				// Fall time
				if (object.isGameCharacter())
				{
					writer
					        << io::write<NetUInt32>(movement.fallTime);
				}
				else
				{
					writer
					        << io::write<NetUInt32>(0);
				}

				// Fall information
				if (moveFlags & game::movement_flags::Falling)
				{
					if (object.isGameCharacter())
					{
						writer
						        << io::write<float>(movement.jumpVelocity)
						        << io::write<float>(movement.jumpSinAngle)
						        << io::write<float>(movement.jumpCosAngle)
						        << io::write<float>(movement.jumpXYSpeed);
					}
					else
					{
						writer
						        << io::write<float>(0.0f)
						        << io::write<float>(0.0f)
						        << io::write<float>(0.0f)
						        << io::write<float>(0.0f);
					}
				}

				// Elevation information
				if (moveFlags & game::movement_flags::SplineElevation)
				{
					if (object.isGameCharacter())
					{
						writer
						        << io::write<float>(movement.unknown1);
					}
					else
					{
						writer
						        << io::write<float>(0.0f);
					}
				}

				GameUnit *unitObject = nullptr;
				if (object.getTypeId() == object_type::Unit ||
				        object.isGameCharacter())
				{
					unitObject = reinterpret_cast<GameUnit *>(&object);
				}

				assert(unitObject);

				// TODO: Speed values
				writer
				        << io::write<float>(unitObject->getSpeed(movement_type::Walk))					// Walk
				        << io::write<float>(unitObject->getSpeed(movement_type::Run))					// Run
				        << io::write<float>(unitObject->getSpeed(movement_type::Backwards))				// Backwards
				        << io::write<float>(unitObject->getSpeed(movement_type::Swim))				// Swim
				        << io::write<float>(unitObject->getSpeed(movement_type::SwimBackwards))		// Swim Backwards
				        << io::write<float>(unitObject->getSpeed(movement_type::Flight))				// Fly
				        << io::write<float>(unitObject->getSpeed(movement_type::FlightBackwards))		// Fly Backwards
				        << io::write<float>(unitObject->getSpeed(movement_type::Turn));					// Turn (radians / sec: PI)
			}

			// Lower-GUID update?
			if (updateFlags & 0x08)
			{
				switch (objectTypeId)
				{
				case object_type::Unit:
					writer << io::write<NetUInt32>(0x0B);
					break;
				case object_type::Character:
					if (updateFlags & 0x01)
					{
						writer << io::write<NetUInt32>(0x15);
					}
					else
					{
						writer << io::write<NetUInt32>(0x08);
					}
					break;
				default:
					writer << io::write<NetUInt32>(guidLowerPart(guid));
					break;
				}
			}

			// High-GUID update?
			if (updateFlags & 0x10)
			{
				switch (objectTypeId)
				{
				case object_type::Object:
				case object_type::Item:
				case object_type::Container:
				case object_type::GameObject:
				case object_type::DynamicObject:
				case object_type::Corpse:
					writer
					        << io::write<NetUInt32>((guid >> 48) & 0x0000FFFF);
					break;
				default:
					writer
					        << io::write<NetUInt32>(0);
				}
			}

			// Write values update
			object.writeValueUpdateBlock(writer, receiver, true);
		}

		// Add block
		out_blocks.push_back(createBlock);
	}
}
