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

#include <cmath>
#include <cstddef>
#include <ostream>
#include <cassert>
#include "matrix4.h"
#include "binary_io/reader.h"
#include "binary_io/writer.h"

namespace wowpp
{
	namespace math
	{
		const Matrix4 Matrix4::ZERO(
		    0.0f, 0.0f, 0.0f, 0.0f,
		    0.0f, 0.0f, 0.0f, 0.0f,
		    0.0f, 0.0f, 0.0f, 0.0f,
		    0.0f, 0.0f, 0.0f, 0.0f);
		const Matrix4 Matrix4::IDENTITY(
		    1.0f, 0.0f, 0.0f, 0.0f,
		    0.0f, 1.0f, 0.0f, 0.0f,
		    0.0f, 0.0f, 1.0f, 0.0f,
		    0.0f, 0.0f, 0.0f, 1.0f);

		io::Writer &operator << (io::Writer &w, Matrix4 const &mat)
		{
			w.sink().write(reinterpret_cast<const char*>(&mat.m_[0]), sizeof(float) * 16);
			return w;
		}

		io::Reader &operator >> (io::Reader &r, Matrix4 &mat)
		{
			assert(r.getSource());
			r.getSource()->read(reinterpret_cast<char*>(&mat.m_[0]), sizeof(float) * 16);
			return r;
		}
	}
}
