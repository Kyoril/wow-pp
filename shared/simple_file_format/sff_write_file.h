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

#include "sff_write_writer.h"
#include "sff_write_table.h"

namespace sff
{
	namespace write
	{
		template <class C>
		struct File : Table<C>
		{
			typedef Table<C> Super;
			typedef typename Super::MyWriter MyWriter;
			typedef typename Super::Flags Flags;


			MyWriter writer;


			File(typename MyWriter::Stream &stream, Flags flags)
				: Super(writer, flags)
				, writer(stream)
			{
			}
		};
	}
}
