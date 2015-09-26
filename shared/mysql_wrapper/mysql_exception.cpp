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

#include "mysql_exception.h"

namespace wowpp
{
	namespace MySQL
	{
		Exception::Exception(const std::string &message)
			: std::runtime_error(message)
		{
		}


		OutOfRangeException::OutOfRangeException()
			: Exception("Index out of range")
		{
		}


		QueryFailException::QueryFailException(const std::string &message)
			: Exception(message)
		{
		}


		StatementException::StatementException(const std::string &message)
			: Exception(message)
		{
		}
	}
}
