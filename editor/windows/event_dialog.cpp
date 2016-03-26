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
#include "event_dialog.h"
#include "ui_event_dialog.h"
#include "editor_application.h"
#include "trigger_helper.h"
#include "data_dialog.h"

namespace wowpp
{
	namespace editor
	{
		EventDialog::EventDialog(EditorApplication &app, proto::TriggerEvent e/* = proto::TriggerEvent()*/)
			: QDialog()
			, m_ui(new Ui::EventDialog)
			, m_app(app)
			, m_event(std::move(e))
		{
			// Setup auto generated ui
			m_ui->setupUi(this);
			m_ui->eventBox->setCurrentIndex(m_event.type());

			connect(m_ui->eventBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_eventBox_currentIndexChanged(int)));
			on_eventBox_currentIndexChanged(m_event.type());

			//return m_ui->eventBox->currentIndex();
		}

		void EventDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		void EventDialog::on_eventBox_currentIndexChanged(int index)
		{
			m_event.set_type(index);
			m_event.clear_data();

			m_ui->eventTextLabel->setText(getTriggerEventText(m_event, true));
		}

		void EventDialog::on_eventTextLabel_linkActivated(const QString & link)
		{
			if (link.startsWith("event-data-"))
			{
				QString numString = link.right(link.size() - 5);
				int index = numString.toInt();

				UInt32 data = (index < m_event.data_size() ? m_event.data(index) : 0);

				DataDialog dialog(m_app.getProject(), 0, index, data);
				auto result = dialog.exec();
				if (result == QDialog::Accepted)
				{
					if (index < m_event.data_size())
						m_event.mutable_data()->Set(index, dialog.getData());
					else
					{
						// Add as many new data elements as needed
						for (int i = m_event.data_size(); i < index + 1; ++i)
						{
							m_event.add_data((i == index ? dialog.getData() : 0));
						}
					}
					m_ui->eventTextLabel->setText(getTriggerEventText(m_event, true));
				}

			}
		}
	}
}
