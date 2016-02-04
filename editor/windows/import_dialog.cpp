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

#include "import_dialog.h"
#include "ui_import_dialog.h"
#include "editor_application.h"
#include <qtconcurrentrun.h>
#include <QMessageBox>

namespace wowpp
{
	namespace editor
	{
		ImportDialog::ImportDialog(EditorApplication &app, ImportTask task)
			: QDialog()
			, m_ui(new Ui::ImportDialog)
			, m_app(app)
			, m_task(task)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			connect(this, SIGNAL(progressRangeChanged(int,int)), this, SLOT(on_progressRangeChanged(int, int)));
			connect(this, SIGNAL(progressTextChanged(QString)), this, SLOT(on_progressTextChanged(QString)));
			connect(this, SIGNAL(progressValueChanged(int)), this, SLOT(on_progressValueChanged(int)));
			connect(this, SIGNAL(calculationFinished()), this, SLOT(on_calculationFinished()));

			// Run connection thread
			const QFuture<void> future = QtConcurrent::run(this, &ImportDialog::importData);
			m_watcher.setFuture(future);
		}

		void ImportDialog::on_progressRangeChanged(int minimum, int maximum)
		{
			m_ui->progressBar->setMinimum(minimum);
			m_ui->progressBar->setMaximum(maximum);
		}

		void ImportDialog::on_progressValueChanged(int value)
		{
			m_ui->progressBar->setValue(value);
		}

		void ImportDialog::on_progressTextChanged(QString text)
		{
			m_ui->label->setText(text);
		}

		void ImportDialog::on_calculationFinished()
		{
			m_app.markAsChanged();
		}

		void ImportDialog::importData()
		{
			// Change progress range
			emit progressRangeChanged(0, 0);
			emit progressValueChanged(0);
			emit progressTextChanged("Connecting...");

			auto &config = m_app.getConfiguration();

			// Database connection
			MySQL::DatabaseInfo connectionInfo(config.mysqlHost, config.mysqlPort, config.mysqlUser, config.mysqlPassword, config.mysqlDatabase);
			MySQL::Connection connection;
			if (!connection.connect(connectionInfo))
			{
				emit progressRangeChanged(0, 100);
				emit progressTextChanged(QString("Could not connect: %1").arg(connection.getErrorMessage()));
				return;
			}

			UInt32 currentEntry = 0;
			UInt32 entryCount = 0;

			// Not collect data
			emit progressRangeChanged(0, 100);
			emit progressTextChanged("Collecting data...");
			{
				wowpp::MySQL::Select select(connection, m_task.countQuery.toStdString());
				if (select.success())
				{
					wowpp::MySQL::Row row(select);
					while (row)
					{
						UInt32 count = 0;
						row.getField(0, count);
						entryCount += count;

						row = row.next(select);
					}
				}
				else
				{
					emit progressTextChanged(QString("Error: %1").arg(connection.getErrorMessage()));
					return;
				}
			}

			// Execute procedure before import (mostly cleanup work is done here)
			if (m_task.beforeImport) m_task.beforeImport();

			// Import items...
			emit progressTextChanged(QString("Importing %1 entries...").arg(entryCount));
			{
				wowpp::MySQL::Select select(connection, m_task.selectQuery.toStdString());
				if (select.success())
				{
					wowpp::MySQL::Row row(select);
					while (row)
					{
						// Increase counter
						emit progressValueChanged(static_cast<int>(static_cast<double>(currentEntry) / static_cast<double>(entryCount) * 100.0));
						currentEntry++;

						if (m_task.onImport)
						{
							bool result = m_task.onImport(row);
							if (!result)
							{
								// TODO: Handle import error
							}
						}

						// Next row
						row = row.next(select);
					}
				}
				else
				{
					emit progressTextChanged(QString("Error: %1").arg(connection.getErrorMessage()));
					return;
				}
			}

			// Execute procedure after import
			if (m_task.afterImport) m_task.afterImport();

			emit progressValueChanged(100);
			emit progressTextChanged("Finished");

			emit calculationFinished();
		}
	}
}
