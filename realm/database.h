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

#include "common/typedefs.h"
#include "game_protocol/game_protocol.h"
#include "game/game_character.h"

namespace wowpp
{
	class PlayerSocial;

	/// Basic interface for a database system used by the realm server.
	struct IDatabase
	{
		virtual ~IDatabase();

		/// Gets the number of characters a specific account has on this realm.
		/// @param accountId Account identifier.
		/// @returns Number of characters of the account.
		virtual UInt32 getCharacterCount(UInt32 accountId) = 0;
		/// Creates a new character.
		/// @param accountId Account identifier.
		/// @param character Data of the character to create.
		/// @returns false if the creation process failed.
		virtual game::ResponseCode createCharacter(UInt32 accountId, game::CharEntry &character) = 0;

		virtual bool getCharacterById(DatabaseId id, game::CharEntry &out_character) = 0;
		virtual bool getCharacterByName(const String &name, game::CharEntry &out_character) = 0;

		virtual bool getCharacters(UInt32 accountId, game::CharEntries &out_characters) = 0;

		virtual game::ResponseCode deleteCharacter(UInt32 accountId, UInt32 characterGuid) = 0;

		virtual bool getGameCharacter(DatabaseId characterId, GameCharacter &out_character) = 0;

		virtual bool getCharacterSocialList(DatabaseId characterId, PlayerSocial &out_social) = 0;
	};
}
