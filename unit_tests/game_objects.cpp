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
#include "common/timer_queue.h"
#include "game/game_object.h"
#include "game/game_character.h"
#include "proto_data/project.h"
#include <boost/test/unit_test.hpp>

namespace wowpp
{
	BOOST_AUTO_TEST_CASE(GameObject_signed_fields)
	{
		// Empty project
		proto::Project project;

		// Construct new object
		GameObject object(project);
		object.initialize();

		object.setInt32Value(object_fields::Entry, -1);
		BOOST_CHECK(object.getInt32Value(object_fields::Entry) == -1);
		BOOST_CHECK(object.getUInt32Value(object_fields::Entry) == 0xFFFFFFFF);

		object.setUInt16Value(object_fields::Entry, 1, -1);
		BOOST_CHECK(object.getInt16Value(object_fields::Entry, 1) == -1);
	}

	BOOST_AUTO_TEST_CASE(GameCharacter_destructor)
	{
		boost::asio::io_service ioService;

		// Empty project
		TimerQueue timers(ioService);
		proto::Project project;

		// Construct new object
		auto testCharacter = std::make_shared<GameCharacter>(project, timers);
		testCharacter->initialize();
		testCharacter.reset();
	}
}
