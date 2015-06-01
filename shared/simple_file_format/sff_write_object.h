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

#include "sff_write_writer.h"

namespace sff
{
	namespace write
	{
		enum
		{
		    MultiLine = 1,
		    Comma = 2,
		    Quoted = 4
		};


		template <class C>
		class Object
		{
		public:

			typedef sff::write::Writer<C> MyWriter;
			typedef unsigned Flags;

		public:

			MyWriter &writer;
			Flags flags;

		public:

			Object(MyWriter &writer, Flags flags)
				: writer(writer)
				, flags(flags)
				, m_parent(nullptr)
				, m_hasElements(false)
			{
			}

			Object(Object &parent, Flags flags)
				: writer(parent.writer)
				, flags(flags)
				, m_parent(&parent)
				, m_hasElements(false)
			{
				m_parent->beforeElement();
			}

			bool usesMultiLine() const
			{
				return (flags & MultiLine) != 0;
			}

			bool usesComma() const
			{
				return (flags & Comma) != 0;
			}

			void finish()
			{
				if (m_parent)
				{
					m_parent->afterElement();
				}

				if (usesMultiLine() &&
				        hasParent())
				{
					writer.leaveLevel();
					writer.newLine();
					writer.writeIndentation();
				}
			}

			void beforeElement()
			{
				if (hasElements())
				{
					if (usesComma())
					{
						writer.writeComma();
					}
				}

				if (usesMultiLine())
				{
					if (hasParent() ||
					        hasElements())
					{
						writer.newLine();
						writer.writeIndentation();
					}
				}
				else if (hasElements())
				{
					writer.space();
				}
			}

			void afterElement()
			{
				m_hasElements = true;
			}

			bool hasElements() const
			{
				return m_hasElements;
			}

			bool hasParent() const
			{
				return (m_parent != 0);
			}

		private:

			Object *m_parent;
			bool m_hasElements;
		};
	}
}
