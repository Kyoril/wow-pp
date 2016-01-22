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
#include "proto_data/project.h"
#include "mysql_wrapper/mysql_connection.h"
#include "mysql_wrapper/mysql_row.h"
#include "mysql_wrapper/mysql_select.h"
#include "mysql_wrapper/mysql_statement.h"
#include <QDialog>
#include <QItemSelection>
#include <QFutureWatcher>
#include <QFuture>
#include <memory>
#include <functional>

// Forwards
namespace Ui
{
	class ImportDialog;
}

namespace wowpp
{
	namespace editor
	{
		class EditorApplication;

		/// Represents an import task.
		struct ImportTask
		{
			/// The MySQL query used to count the objects to import. Should start with "SELECT COUNT(*) FROM ...".
			QString countQuery;
			/// The MySQL query used to select the objects to import. Should start with "SELECT ... FROM ...".
			QString selectQuery;
			/// Callback which is executed before the import starts.
			std::function<void()> beforeImport;
			/// Callback which is executed for every data row.
			std::function<bool(wowpp::MySQL::Row &row)> onImport;
			/// Callback which is executed after the import.
			std::function<void()> afterImport;
		};

		/// 
		class ImportDialog : public QDialog
		{
			Q_OBJECT

		public:

			/// 
			explicit ImportDialog(EditorApplication &app, ImportTask task);

		signals:

			void progressRangeChanged(int minimum, int maximum);
			void progressValueChanged(int value);
			void progressTextChanged(QString text);
			void calculationFinished();

		private Q_SLOTS:

			void on_progressRangeChanged(int minimum, int maximum);
			void on_progressValueChanged(int value);
			void on_progressTextChanged(QString text);
			void on_calculationFinished();

		private:

			void importData();

		private:

			Ui::ImportDialog *m_ui;
			EditorApplication &m_app;
			QFutureWatcher<void> m_watcher;
			ImportTask m_task;
		};
	}
}
