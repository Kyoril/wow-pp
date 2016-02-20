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
	class PlayerSocial;
	namespace proto
	{
		class SpellEntry;
	}

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
		virtual game::ResponseCode createCharacter(UInt32 accountId, const std::vector<const proto::SpellEntry*> &spells, const std::vector<ItemData> &items, game::CharEntry &character) = 0;
		virtual game::ResponseCode renameCharacter(DatabaseId id, const String &newName) = 0;
		virtual bool getCharacterById(DatabaseId id, game::CharEntry &out_character) = 0;
		virtual bool getCharacterByName(const String &name, game::CharEntry &out_character) = 0;
		virtual bool getCharacters(UInt32 accountId, game::CharEntries &out_characters) = 0;
		virtual game::ResponseCode deleteCharacter(UInt32 accountId, UInt64 characterGuid) = 0;
		virtual bool getGameCharacter(DatabaseId characterId, GameCharacter &out_character) = 0;
		virtual bool saveGameCharacter(const GameCharacter &character, const std::vector<ItemData> &items, const std::vector<UInt32> &spells) = 0;
		virtual bool getCharacterSocialList(DatabaseId characterId, PlayerSocial &out_social) = 0;
		virtual bool addCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) = 0;
		virtual bool updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags) = 0;
		virtual bool updateCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid, game::SocialFlag flags, const String &note) = 0;
		virtual bool removeCharacterSocialContact(DatabaseId characterId, UInt64 socialGuid) = 0;
		virtual bool getCharacterActionButtons(DatabaseId characterId, ActionButtons &out_buttons) = 0;
		virtual bool setCharacterActionButtons(DatabaseId characterId, const ActionButtons &buttons) = 0;
		virtual bool setCinematicState(DatabaseId characterId, bool state) = 0;
		virtual bool setQuestData(DatabaseId characterId, UInt32 questId, const QuestStatusData &data) = 0;
		virtual bool teleportCharacter(DatabaseId characterId, UInt32 mapId, float x, float y, float z, float o, bool changeHome = false) = 0;
	};

	enum RequestStatus
	{
		RequestSuccess,
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

	struct AsyncDatabase : boost::noncopyable
	{
		typedef std::function<void(const std::function<void()> &)> ActionDispatcher;

		explicit AsyncDatabase(IDatabase &database,
			ActionDispatcher asyncWorker,
			ActionDispatcher resultDispatcher);

		/**
		* @brief asyncRequest begins an async request to the database. The
		* handler will eventually be called by the result dispatcher at most
		* once. It will be called if there is no unexpected internal error.
		* The handler is expected to accept exactly one argument whose type
		* depends on the result of the database method.
		* @param handler is expected to be a functor taking exactly one
		* argument. The argument type is {@link faudra::RequestStatus} if the
		* result type of the method is void. The argument type is
		* {@link boost::optional} of the result type for every result type
		* other than void. The handler is called at most once and its result
		* is ignored. The behavior is undefined when the handler throws.
		* @param b0 is the first argument to the database method.
		*/
		template <class ResultHandler, class Result, class A0, class B0_>
		void asyncRequest(ResultHandler &&handler,
			Result(IDatabase::*method)(A0),
			B0_ &&b0)
		{
			auto request = std::bind(method, &m_database, std::forward<B0_>(b0));
			auto processor = [this, request, handler]() -> void
			{
				detail::RequestProcessor<Result> proc;
				return proc(m_resultDispatcher, request, handler);
			};
			m_asyncWorker(processor);
		}

		template <class Result, class ResultHandler, class RequestFunction>
		void asyncRequest(ResultHandler &&handler,
			RequestFunction &&request)
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

		IDatabase &m_database;
		const ActionDispatcher m_asyncWorker;
		const ActionDispatcher m_resultDispatcher;
	};

	typedef std::function<void()> Action;
	typedef std::function<void(const Action &)> ActionDispatcher;
}
