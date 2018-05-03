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
#include "export_dialog.h"
#include "ui_export_dialog.h"
#include "editor_application.h"
#include "configuration.h"
#include <qtconcurrentrun.h>
#include <QMessageBox>

namespace wowpp
{
	namespace editor
	{
		ExportDialog::ExportDialog(EditorApplication &app)
			: QDialog()
			, m_ui(new Ui::ExportDialog)
			, m_app(app)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			connect(this, SIGNAL(progressRangeChanged(int,int)), this, SLOT(on_progressRangeChanged(int, int)));
			connect(this, SIGNAL(progressTextChanged(QString)), this, SLOT(on_progressTextChanged(QString)));
			connect(this, SIGNAL(progressValueChanged(int)), this, SLOT(on_progressValueChanged(int)));
			connect(this, SIGNAL(calculationFinished()), this, SLOT(on_calculationFinished()));
		}

		int ExportDialog::exec()
		{
			// Run connection thread
			const QFuture<void> future = QtConcurrent::run(this, &ExportDialog::exportData);
			m_watcher.setFuture(future);

			return QDialog::exec();
		}

		void ExportDialog::on_progressRangeChanged(int minimum, int maximum)
		{
			m_ui->progressBar->setMinimum(minimum);
			m_ui->progressBar->setMaximum(maximum);
		}

		void ExportDialog::on_progressValueChanged(int value)
		{
			m_ui->progressBar->setValue(value);
		}

		void ExportDialog::on_progressTextChanged(QString text)
		{
			DLOG(text.toStdString());
			m_ui->label->setText(text);
		}

		void ExportDialog::on_calculationFinished()
		{
		}

		void ExportDialog::reportError(const String & error)
		{
			emit progressTextChanged(QString("Error: %1").arg(error.c_str()));
		}

		void ExportDialog::reportProgressChange(float progress)
		{
			emit progressValueChanged(progress * 100.0);
		}

		void ExportDialog::addTask(TransferTask task)
		{
			m_tasks.emplace(std::move(task));
		}
		
		void ExportDialog::exportData()
		{
			// Change progress range
			emit progressRangeChanged(0, 0);
			emit progressValueChanged(0);
			emit progressTextChanged("Connecting...");

			auto &config = m_app.getConfiguration();

			const auto& dataProject = m_app.getActiveProject();

			// Database connection
			MySQL::DatabaseInfo connectionInfo(
				dataProject.databaseInfo.mysqlHost, 
				dataProject.databaseInfo.mysqlPort, 
				dataProject.databaseInfo.mysqlUser, 
				dataProject.databaseInfo.mysqlPassword, 
				dataProject.databaseInfo.mysqlDatabase);
			MySQL::Connection connection;
			if (!connection.connect(connectionInfo))
			{
				emit progressRangeChanged(0, 100);
				emit progressTextChanged(QString("Could not connect: %1").arg(connection.getErrorMessage()));
				return;
			}

			if (!connection.execute("SET NAMES 'UTF8';"))
			{
				emit progressTextChanged(QString("Error: %1").arg(connection.getErrorMessage()));
				return;
			}

			// Write lock all tables
			connection.execute("FLUSH TABLES WITH WRITE LOCK;");

			bool bShouldCommit = true;
			{
				// Start a new transaction
				MySQL::Transaction transaction(connection);

				// Not collect data
				emit progressRangeChanged(0, 100);

				// Execute procedure before export (mostly cleanup work is done here)
				while (!m_tasks.empty())
				{
					const auto& task = m_tasks.front();

					if (task.beforeTransfer) task.beforeTransfer(connection);

					// Export items...
					emit progressTextChanged(task.taskName.c_str());
					{
						if (!task.doWork(connection, *this))
						{
							bShouldCommit = false;
							break;
						}
					}

					// Execute procedure after transfer
					if (task.afterTransfer) task.afterTransfer();

					m_tasks.pop();
				}

				// Commit transaction
				if (bShouldCommit)
					transaction.commit();
			}

			// Release lock
			connection.execute("UNLOCK TABLES;");
			
			emit progressValueChanged(100);
			emit progressTextChanged("Finished");

			emit calculationFinished();
		}
	}
}
