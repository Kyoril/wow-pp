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

	/// Argument structure for deleting a character.
	struct DeleteCharacterArgs
	{
		/// The respective account id as 
		UInt32 accountId;
		/// The character id.
		DatabaseId characterId;
	};

	/// Social entry to be used by the PlayerSocial class.
	struct PlayerSocialEntry
	{
		/// Guid of the other character.
		UInt64 guid;
		/// Social flags (Friend, Ignored, Muted)
		game::SocialFlag flags;
		/// Custom note.
		std::string note;
	};

	/// Vector of PlayerSocialEntry objects for syntactic sugar
	typedef std::vector<PlayerSocialEntry> PlayerSocialEntries;

	/// Represents data of a player group.
	struct GroupData
	{
		/// Guid of the group leader.
		UInt64 leaderGuid;
		/// Vector of guids of all group members, including the leader.
		std::vector<UInt64> memberGuids;
	};


	/// Basic interface for a database system used by the realm server.
	struct IDatabase
	{
	public:
		virtual ~IDatabase();

	public:
		/// Gets the number of characters a specific account has on this realm.
		/// 
		/// @param accountId Account identifier.
		/// @return Number of characters of the account or uninitialized in case of an error.
		virtual boost::optional<UInt32> getCharacterCount(UInt32 accountId) = 0;
		/// Creates a new character.
		/// 
		/// @param accountId Account identifier.
		/// @param character Data of the character to create.
		/// @return false if the creation process failed.
		virtual game::ResponseCode createCharacter(UInt32 accountId, const std::vector<const proto::SpellEntry*> &spells, const std::vector<ItemData> &items, game::CharEntry &character) = 0;
		/// Changes the name of the given player character. The database won't perform
		/// name checks, so check for valid character names before you call this function!
		/// 
		/// @param id The database id of the character which should be renamed.
		/// @param newName The new character name.
		/// @return 
		virtual game::ResponseCode renameCharacter(DatabaseId id, const String &newName) = 0;
		/// Retrieves character information based on a character id.
		/// @param id Characters database id.
		/// @return Filled CharEntry struct or nullptr on failure or non-existance.
		virtual boost::optional<game::CharEntry> getCharacterById(DatabaseId id) = 0;
		/// Retrieves character information based on a characters name.
		/// 
		/// @param id Characters name.
		/// @return Filled CharEntry struct or nullptr on failure or non-existance.
		virtual boost::optional<game::CharEntry> getCharacterByName(const String &name) = 0;
		/// Retrieves informations about all characters of a specific account.
		/// 
		/// @param accountId The account id.
		/// @returns An optional std::vector of CharEntry structs.
		virtual game::CharEntries getCharacters(UInt32 accountId) = 0;
		/// Deletes a specific character from the database. Deleting a character won't really
		/// delete it but just deactivate it so that it won't appear in the character list anymore.
		/// This will also set the current timestamp so that you can manually cleanup the db later,
		/// for example delete every character which was "deleted" 30+ days before.
		/// 
		/// @param arguments Function arguments.
		/// @throw std::exception If a database error occurred.
		virtual void deleteCharacter(DeleteCharacterArgs arguments) = 0;
		/// Loads all game-relevant data of a specific character from the database. This query
		/// is quite expensive!
		/// 
		/// @param characterId The database id of the character.
		/// @param out_character Reference to a valid GameCharacter whose properties will be set.
		/// @return true on success, false if an error occurred.
		virtual bool getGameCharacter(DatabaseId characterId, GameCharacter &out_character) = 0;
		/// Saves all character-relevant data of a specific character from the database. This query
		/// is quite expensive!
		/// 
		/// @param character The character object which will be saved.
		/// @param items Items of the specific character.
		/// @return true on success, false if an error occurred.
		virtual bool saveGameCharacter(const GameCharacter &character, const std::vector<ItemData> &items) = 0;
		/// Loads a characters social list entries from the database.
		/// 
		/// @param characterId The database id of the character.
		/// @return An optional std::vector of PlayerSocialEntry structs.
		virtual boost::optional<PlayerSocialEntries> getCharacterSocialList(DatabaseId characterId) = 0;
		/// Adds a new social contact to a characters social list.
		/// 
		/// @param characterId Guid of the character whose social list will be updated.
		/// @param socialGuid Guid of the other character.
		/// @param flags Social flags.
		/// @param note Custom note.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void addCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) = 0;
		/// Updates a social contact of a specific characters social list.
		/// 
		/// @param characterId Guid of the character whose social list will be updated.
		/// @param socialGuid Guid of the other character.
		/// @param flags Social flags.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags) = 0;
		/// Updates a social contact of a specific characters social list.
		/// 
		/// @param characterId Guid of the character whose social list will be updated.
		/// @param socialGuid Guid of the other character.
		/// @param flags Social flags.
		/// @param note Custom note.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) = 0;
		/// Removes a contact from a specific characters social list.
		/// 
		/// @param characterId Guid of the character whose social list will be updated.
		/// @param socialGuid Guid of the other character.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void removeCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid) = 0;
		/// Loads the action button bindings of a specific character.
		/// 
		/// @param characterId Guid of the character whose action button bindings will be loaded.
		/// @param out_buttons Reference to an ActionButtons struct which will be updated.
		/// @return true if successful, false if an error occurred.
		virtual bool getCharacterActionButtons(DatabaseId characterId, ActionButtons &out_buttons) = 0;
		/// Updates a characters action button bindings.
		/// 
		/// @param characterId The database id of the character.
		/// @param buttons Action binding map.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void setCharacterActionButtons(DatabaseId characterId, const ActionButtons &buttons) = 0;
		/// Updates the cinematic state of a specific character. The cinematic state
		/// is true, if the intro cinematic has been watched by the player, so that it
		/// will no longer be triggered on login.
		/// 
		/// @param characterId The database id of the specific character.
		/// @param state The new cinematic state (true indicates that the intro cinematic has been watched).
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void setCinematicState(DatabaseId characterId, bool state) = 0;
		/// Updates the state of a quest for a specific character.
		/// 
		/// @param characterId The character id.
		/// @param questId The quest id.
		/// @param data The quest data.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void setQuestData(DatabaseId characterId, UInt32 questId, const QuestStatusData &data) = 0;
		/// Changes the current location of the given character in the database and can also change the
		/// characters home location.
		/// 
		/// @param characterId Database id of the character whose location will be changed.
		/// @param mapId The destination map id.
		/// @param x The new x coordinate.
		/// @param y The new y coordinate.
		/// @param z The new z coordinate.
		/// @param o The new rotation in radians.
		/// @param changeHome If you pass true here, the home location of the character will be changed as well.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void teleportCharacter(DatabaseId characterId, UInt32 mapId, float x, float y, float z, float o, bool changeHome = false) = 0;
		/// Adds a spell to the list of known spells for a specific character. You have
		/// to make sure that this is a valid spell id before calling this method!
		/// 
		/// @param characterId The database id of the character.
		/// @param spellId The spell id.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void learnSpell(DatabaseId characterId, UInt32 spellId) = 0;
		/// Creates a new group. You have to make sure that the group has a unique id.
		/// 
		/// @param groupId The group id.
		/// @param leader Guid of the group leader.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void createGroup(UInt64 groupId, UInt64 leader) = 0;
		/// Disbands a group (removes it from the database as well).
		/// 
		/// @param groupId Id of the group to disband.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void disbandGroup(UInt64 groupId) = 0;
		/// Adds a group member to an existing group.
		/// 
		/// @param groupId Id of the group the member will be added.
		/// @param member Guid of the character to add.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void addGroupMember(UInt64 groupId, UInt64 member) = 0;
		/// Changes the leader of a specific existing group.
		/// 
		/// @param groupId Id of the group whose leader will be changed.
		/// @param leaderGuid Guid of the new group leader.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void setGroupLeader(UInt64 groupId, UInt64 leaderGuid) = 0;
		/// Removes a member from an existing group.
		/// 
		/// @param groupId Id of the group.
		/// @param member Guid of the group member to remove.
		/// @throw std::exception In case of a database error (i.e. syntax error, access error etc.)
		virtual void removeGroupMember(UInt64 groupId, UInt64 member) = 0;
		/// Loads a list of all group guids that are stored in the database.
		/// 
		/// @return optional std::vector of group guids.
		virtual boost::optional<std::vector<UInt64>> listGroups() = 0;
		/// Loads group data of a specific group from.
		/// 
		/// @param groupId Id of the group to load.
		/// @return optional GroupData struct.
		virtual boost::optional<GroupData> loadGroup(UInt64 groupId) = 0;
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
		/// Performs an async database request and allows passing exactly one argument to the database request.
		/// 
		/// @param method A request callback which will be executed on the database thread without blocking the caller.
		/// @param b0 Argument which will be forwarded to the handler.
		template <class A0, class B0_>
		void asyncRequest(void(IDatabase::*method)(A0), B0_ &&b0)
		{
			auto request = std::bind(method, &m_database, std::forward<B0_>(b0));
			auto processor = [request]() -> void {
				try
				{
					request();
				}
				catch (const std::exception& ex)
				{
					defaultLogException(ex);
				}
			}
			m_asyncWorker(processor);
		}

		/// Performs an async database request and allows passing exactly one argument to the database request.
		/// 
		/// @param handler A handler callback which will be executed after the request was successful.
		/// @param method A request callback which will be executed on the database thread without blocking the caller.
		/// @param b0 Argument which will be forwarded to the handler.
		template <class ResultHandler, class Result, class A0, class B0_>
		void asyncRequest(ResultHandler &&handler, Result(IDatabase::*method)(A0), B0_ &&b0)
		{
			auto request = std::bind(method, &m_database, std::forward<B0_>(b0));
			auto processor = [this, request, handler]() -> void
			{
				detail::RequestProcessor<Result> proc;
				proc(m_resultDispatcher, request, handler);
			};
			m_asyncWorker(processor);
		}

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
				proc(m_resultDispatcher, boundRequest, handler);
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
