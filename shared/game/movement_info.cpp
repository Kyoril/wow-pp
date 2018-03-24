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
#include "movement_info.h"
#include "game/defines.h"

namespace wowpp
{
	MovementInfo::MovementInfo()
		: moveFlags(game::movement_flags::None)
		, moveFlags2(0)
		, time(0)
		, x(0.0f), y(0.0f), z(0.0f), o(0.0f)
		, transportGuid(0)
		, tX(0.0f), tY(0.0f), tZ(0.0f), tO(0.0f)
		, transportTime(0)
		, pitch(0.0f)
		, fallTime(0)
		, jumpVelocity(0.0f)
		, jumpSinAngle(0.0f)
		, jumpCosAngle(0.0f)
		, jumpXYSpeed(0.0f)
		, unknown1(0.0f)
	{
	}

	io::Writer &operator<<(io::Writer &w, MovementInfo const &info)
	{
		w
			<< io::write<NetUInt32>(info.moveFlags)
			<< io::write<NetUInt8>(info.moveFlags2)
			<< io::write<NetUInt32>(info.time)
			<< io::write<float>(info.x)
			<< io::write<float>(info.y)
			<< io::write<float>(info.z)
			<< io::write<float>(info.o);

		if (info.moveFlags & game::movement_flags::OnTransport)
		{
			w
				<< io::write<NetUInt64>(info.transportGuid)
				<< io::write<float>(info.tX)
				<< io::write<float>(info.tY)
				<< io::write<float>(info.tZ)
				<< io::write<float>(info.tO)
				<< io::write<NetUInt32>(info.transportTime);
		}

		if (info.moveFlags & (game::movement_flags::Swimming | game::movement_flags::Flying))
		{
			w << io::write<float>(info.pitch);
		}

		w << io::write<NetUInt32>(info.fallTime);

		if (info.moveFlags & game::movement_flags::Falling)
		{
			w
				<< io::write<float>(info.jumpVelocity)
				<< io::write<float>(info.jumpSinAngle)
				<< io::write<float>(info.jumpCosAngle)
				<< io::write<float>(info.jumpXYSpeed);
		}

		if (info.moveFlags & game::movement_flags::SplineElevation)
		{
			w << io::write<float>(info.unknown1);
		}

		return w;
	}

	io::Reader &operator>>(io::Reader &r, MovementInfo &info)
	{
		r
			>> io::read<NetUInt32>(info.moveFlags)
			>> io::read<NetUInt8>(info.moveFlags2)
			>> io::read<NetUInt32>(info.time)
			>> io::read<float>(info.x)
			>> io::read<float>(info.y)
			>> io::read<float>(info.z)
			>> io::read<float>(info.o);

		if (info.moveFlags & game::movement_flags::OnTransport)
		{
			r
				>> io::read<NetUInt64>(info.transportGuid)
				>> io::read<float>(info.tX)
				>> io::read<float>(info.tY)
				>> io::read<float>(info.tZ)
				>> io::read<float>(info.tO)
				>> io::read<NetUInt32>(info.transportTime);
		}

		if (info.moveFlags & (game::movement_flags::Swimming | game::movement_flags::Flying))
		{
			r >> io::read<float>(info.pitch);
		}

		r >> io::read<NetUInt32>(info.fallTime);

		if (info.moveFlags & game::movement_flags::Falling)
		{
			r
				>> io::read<float>(info.jumpVelocity)
				>> io::read<float>(info.jumpSinAngle)
				>> io::read<float>(info.jumpCosAngle)
				>> io::read<float>(info.jumpXYSpeed);
		}

		if (info.moveFlags & game::movement_flags::SplineElevation)
		{
			r >> io::read<float>(info.unknown1);
		}

		return r;
	}
}
