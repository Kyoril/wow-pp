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

#include <QAbstractListModel>
#include <QObject>
#include "proto_data/project.h"
#include "qfilesystemmodel.h"

namespace wowpp
{
	namespace editor
	{
		class TriggerListModel : public QAbstractListModel
		{
		public:

			explicit TriggerListModel(proto::TriggerManager &entries, QObject *parent = nullptr)
				: QAbstractListModel(parent)
				, m_entries(entries)
			{
			}

			virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override
			{
				return m_entries.getTemplates().entry_size();
			}
			
			QVariant data(const QModelIndex &index, int role) const override
			{
				if (!index.isValid())
					return QVariant();

				if (index.row() >= m_entries.getTemplates().entry_size())
					return QVariant();

				static QImage fileImage(":/File.png");
				static QImage folderImage(":/Folder.png");

				if (role == Qt::DisplayRole)
				{
					const auto &templates = m_entries.getTemplates();
					const auto &tpl = templates.entry(index.row());

					return QString("%1 %2").arg(QString::number(tpl.id()), 6, QLatin1Char('0')).arg(tpl.name().c_str());
				}
				else if (role == Qt::DecorationRole)
				{
					return fileImage;
				}

				return QVariant();
			}

			QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
			{
				if (role != Qt::DisplayRole)
					return QVariant();

				if (orientation == Qt::Horizontal)
				{
					return QString("Column %1").arg(section);
				}
				else
				{
					return QString("Row %1").arg(section);
				}
			}

			Qt::ItemFlags flags(const QModelIndex &index) const override
			{
				if (!index.isValid())
					return Qt::ItemIsEnabled;

				return QAbstractItemModel::flags(index) & ~Qt::ItemIsEditable;
			}

			bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override
			{
				// Not supported
				return false;
			}

			bool insertRows(int position, int rows, const QModelIndex &index = QModelIndex()) override
			{
				// Not supported
				return false;
			}

			bool removeRows(int position, int rows, const QModelIndex &index = QModelIndex()) override
			{
				// Not supported
				return false;
			}

		private:

			proto::TriggerManager &m_entries;
		};
	}
}
