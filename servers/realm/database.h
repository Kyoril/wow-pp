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

#include "common/typedefs.h"
#include "game_protocol/game_protocol.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "game/game_character.h"
#include "log/log_exception.h"

namespace wowpp
{
	// Fowards
	class PlayerSocial;
	namespace proto
	{
		class SpellEntry;
	}

	/// Basic interface for a database system used by the realm server.
	struct IDatabase
	{
	public:
		virtual ~IDatabase();

	public:
		/// Gets the number of characters a specific account has on this realm.
		/// 
		/// @param accountId Account identifier.
		/// @returns Number of characters of the account or uninitialized in case of an error.
		virtual boost::optional<UInt32> getCharacterCount(UInt32 accountId) = 0;
		/// Creates a new character.
		/// 
		/// @param accountId Account identifier.
		/// @param character Data of the character to create.
		/// @returns false if the creation process failed.
		virtual game::ResponseCode createCharacter(UInt32 accountId, const std::vector<const proto::SpellEntry*> &spells, const std::vector<ItemData> &items, game::CharEntry &character) = 0;
		/// 
		/// @param id
		/// @param newName
		/// @returns 
		virtual game::ResponseCode renameCharacter(DatabaseId id, const String &newName) = 0;
		/// Retrieves character information based on a character id.
		/// @param id Characters database id.
		/// @returns Filled CharEntry struct or nullptr on failure or non-existance.
		virtual boost::optional<game::CharEntry> getCharacterById(DatabaseId id) = 0;
		/// Retrieves character information based on a characters name.
		/// 
		/// @param id Characters name.
		/// @returns Filled CharEntry struct or nullptr on failure or non-existance.
		virtual boost::optional<game::CharEntry> getCharacterByName(const String &name) = 0;
		/// 
		/// 
		/// @param accountId
		/// @param out_characters
		/// @returns 
		virtual bool getCharacters(UInt32 accountId, game::CharEntries &out_characters) = 0;
		/// 
		/// 
		/// @param accountId
		/// @param characterGuid
		/// @returns 
		virtual game::ResponseCode deleteCharacter(UInt32 accountId, UInt64 characterGuid) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param out_character
		/// @returns 
		virtual bool getGameCharacter(DatabaseId characterId, GameCharacter &out_character) = 0;
		/// 
		/// 
		/// @param character 
		/// @param items
		/// @returns 
		virtual bool saveGameCharacter(const GameCharacter &character, const std::vector<ItemData> &items) = 0;
		/// 
		/// 
		/// @param characterId 
		/// @param out_social
		/// @returns 
		virtual bool getCharacterSocialList(DatabaseId characterId, PlayerSocial &out_social) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param socialGuid
		/// @param flags
		/// @param note
		/// @returns 
		virtual bool addCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param socialGuid
		/// @param flags
		/// @returns 
		virtual bool updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param socialGuid
		/// @param flags
		/// @param note 
		/// @returns 
		virtual bool updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param socialGuid
		/// @returns 
		virtual bool removeCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param out_buttons
		/// @returns 
		virtual bool getCharacterActionButtons(DatabaseId characterId, ActionButtons &out_buttons) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param buttons
		/// @returns 
		virtual bool setCharacterActionButtons(DatabaseId characterId, const ActionButtons &buttons) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param state
		/// @returns 
		virtual bool setCinematicState(DatabaseId characterId, bool state) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param questId
		/// @param data 
		/// @returns 
		virtual bool setQuestData(DatabaseId characterId, UInt32 questId, const QuestStatusData &data) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param mapId
		/// @param x 
		/// @param y 
		/// @param z 
		/// @param o 
		/// @param changeHome 
		/// @returns 
		virtual bool teleportCharacter(DatabaseId characterId, UInt32 mapId, float x, float y, float z, float o, bool changeHome = false) = 0;
		/// 
		/// 
		/// @param characterId
		/// @param spellId
		/// @returns 
		virtual bool learnSpell(DatabaseId characterId, UInt32 spellId) = 0;
		/// 
		/// 
		/// @param groupId
		/// @param leader
		/// @returns 
		virtual bool createGroup(UInt64 groupId, UInt64 leader) = 0;
		/// 
		/// 
		/// @param groupId
		/// @returns 
		virtual bool disbandGroup(UInt64 groupId) = 0;
		/// 
		/// 
		/// @param groupId
		/// @param member 
		/// @returns 
		virtual bool addGroupMember(UInt64 groupId, UInt64 member) = 0;
		/// 
		/// 
		/// @param groupId
		/// @param leaderGuid 
		/// @returns 
		virtual bool setGroupLeader(UInt64 groupId, UInt64 leaderGuid) = 0;
		/// 
		/// 
		/// @param groupId
		/// @param member 
		/// @returns 
		virtual bool removeGroupMember(UInt64 groupId, UInt64 member) = 0;
		/// 
		/// 
		/// @param out_groupIds  
		/// @returns 
		virtual bool listGroups(std::vector<UInt64> &out_groupIds) = 0;
		/// 
		/// 
		/// @param groupId
		/// @param out_leader 
		/// @param out_member
		/// @returns 
		virtual bool loadGroup(UInt64 groupId, UInt64 &out_leader, std::vector<UInt64> &out_member) = 0;
	};

	/// Enumerates possible request status results for async database requests with void return types.
	enum RequestStatus
	{
		/// Successfully executed task.
		RequestSuccess,
		/// Task execution failed (most likely due to an exception)
		RequestFail
	};

	namespace detail
	{
		template <class Result>
		struct RequestProcessor
		{
			template <class ResultDispatcher, class Request, class ResultHandler>
			void operator ()(const ResultDispatcher &dispatcher,
				const Request &request,
				const ResultHandler &handler) const
			{
				boost::optional<Result> result;
				try
				{
					result = request();
				}
				catch (const std::exception &ex)
				{
					defaultLogException(ex);
				}
				dispatcher(std::bind<void>(handler, std::move(result)));
			}
		};

		template <>
		struct RequestProcessor<void>
		{
			template <class ResultDispatcher, class Request, class ResultHandler>
			void operator ()(const ResultDispatcher &dispatcher,
				const Request &request,
				const ResultHandler &handler) const
			{
				RequestStatus status = RequestFail;
				try
				{
					request();
					status = RequestSuccess;
				}
				catch (const std::exception &ex)
				{
					defaultLogException(ex);
				}
				dispatcher(std::bind<void>(handler, status));
			}
		};
	}

	/// Helper class for async database operations
	class AsyncDatabase final
	{
	private:
		// Non-copyable
		AsyncDatabase(const AsyncDatabase &Other) = delete;
		AsyncDatabase &operator=(AsyncDatabase &Other) = delete;

	public:
		typedef std::function<void(const std::function<void()> &)> ActionDispatcher;

		/// Initializes this class by assigning a database and worker callbacks.
		/// 
		/// @param database The linked database which will be passed in to database operations.
		/// @param asyncWorker Callback which should queue a request to the async worker queue.
		/// @param resultDispatcher Callback which should queue a result callback to the main worker queue.
		explicit AsyncDatabase(IDatabase &database,
			ActionDispatcher asyncWorker,
			ActionDispatcher resultDispatcher);
		
	public:
		/// Performs an async database request.
		/// 
		/// @param request A request callback which will be executed on the database thread without blocking the caller.
		/// @param handler A handler callback which will be executed after the request was successful.
		template <class Result, class ResultHandler, class RequestFunction>
		void asyncRequest(RequestFunction &&request, ResultHandler &&handler)
		{
			auto processor = [this, request, handler]() -> void
			{
				detail::RequestProcessor<Result> proc;
				auto boundRequest = std::bind(request, &m_database);
				return proc(m_resultDispatcher, boundRequest, handler);
			};
			m_asyncWorker(std::move(processor));
		}

	private:
		/// The database instance to perform 
		IDatabase &m_database;
		/// Callback which will queue a request to the async worker queue.
		const ActionDispatcher m_asyncWorker;
		/// Callback which will queue a result callback to the main worker queue.
		const ActionDispatcher m_resultDispatcher;
	};

	typedef std::function<void()> Action;
}
