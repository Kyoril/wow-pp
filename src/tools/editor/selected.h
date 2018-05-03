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

#include "math/vector3.h"
#include "common/vector.h"

namespace wowpp
{
	/// Base class for a selected object instance.
	class Selected
	{
	public:

		typedef simple::signal<void(const Selected &)> PositionChangedSignal;
		typedef simple::signal<void(const Selected &)> RotationChangedSignal;
		typedef simple::signal<void(const Selected &)> ScaleChangedSignal;

		PositionChangedSignal positionChanged;
		RotationChangedSignal rotationChanged;
		ScaleChangedSignal scaleChanged;

		virtual ~Selected();

		/// Translates the selected object.
		/// @param delta The position offset.
		virtual void translate(const math::Vector3 &delta) = 0;
		/// Rotates the selected object.
		/// @param delta The orientation offset.
		virtual void rotate(const Vector<float, 4> &delta) = 0;
		/// Scales the selected object.
		/// @param delta The scale amount on all three axis.
		virtual void scale(const math::Vector3 &delta) = 0;
		/// Removes the selected object permanently.
		virtual void remove() = 0;
		/// Deselects the selected object.
		virtual void deselect() = 0;
		/// Returns the position of the selected object.
		/// @returns Position of the selected object in world coordinates.
		virtual math::Vector3 getPosition() const = 0;
		/// Returns the orientation of the selected object.
		/// @returns Orientation of the selected object as a quaternion.
		virtual Vector<float, 4> getOrientation() const = 0;
		/// Returns the scale of the selected object.
		/// @returns Scale of the selected object.
		virtual math::Vector3 getScale() const = 0;
	};
}
