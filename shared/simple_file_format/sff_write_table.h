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

#include "sff_write_object.h"
#include "sff_write_array.h"

namespace sff
{
	namespace write
	{
		template <class C>
		struct Table : Object<C>
		{
			typedef Object<C> Super;
			typedef typename Super::MyWriter MyWriter;
			typedef typename Super::Flags Flags;


			template <class Name>
			Table(Object<C> &parent, const Name &name, Flags flags)
				: Super(parent, flags)
			{
				this->writer.writeKey(name);

				if (this->usesMultiLine())
				{
					this->writer.newLine();
					this->writer.writeIndentation();
					this->writer.enterLevel();
				}

				this->writer.enterTable();
			}

			Table(Array<C> &parent, Flags flags)
				: Super(parent, flags)
			{
				if (this->usesMultiLine())
				{
					this->writer.enterLevel();
				}

				this->writer.enterTable();
			}

			//global table
			Table(MyWriter &writer, Flags flags)
				: Super(writer, flags)
			{
			}

			template <class Name, class Value>
			void addKey(const Name &name, const Value &value)
			{
				this->beforeElement();
				this->writer.writeKey(name);
				this->writer.writeValue(value);
				this->afterElement();
			}

			void finish()
			{
				Super::finish();
				if (this->hasParent())
				{
					this->writer.leaveTable();
				}
			}
		};
	}
}
