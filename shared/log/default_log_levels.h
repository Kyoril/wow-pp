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

#include "log_level.h"
#include "default_log.h"


namespace wowpp
{
	//für Einträge, die für Entwickler hilfreich sind
	extern const LogLevel g_DebugLevel;

	//für alle gewöhnlichen Ereignisse, die von Interesse sind
	// - Verbindung hergestellt
	// - Spieler angemeldet
	extern const LogLevel g_InfoLevel;

	//für unerwartete, aber unkritische Ereignisse
	// - optionale, aber sinnvolle Einstellung nicht vorhanden
	// - unsinnige Spielereingabe
	extern const LogLevel g_WarningLevel;

	//für Probleme, die zur Laufzeit entstehen
	// - Datei nicht gefunden
	// - Verbindung verloren
	extern const LogLevel g_ErrorLevel;


#define DLOG(message) WOWPP_LOG(::wowpp::g_DebugLevel, message)
#define ILOG(message) WOWPP_LOG(::wowpp::g_InfoLevel, message)
#define WLOG(message) WOWPP_LOG(::wowpp::g_WarningLevel, message)
#define ELOG(message) WOWPP_LOG(::wowpp::g_ErrorLevel, message)
}
