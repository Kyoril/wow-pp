
#include "pch.h"
#include "variable_type_delegate.h"

#include <QComboBox>
#include <QWidget>
#include <QModelIndex>
#include <QApplication>
#include <QString>

#include <iostream>

VariableTypeDelegate::VariableTypeDelegate(QObject *parent)
	: QItemDelegate(parent)
{
	Items.push_back("INTEGER");
	Items.push_back("LONG");
	Items.push_back("FLOAT");
	Items.push_back("STRING");
}


QWidget *VariableTypeDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/* option */, const QModelIndex &/* index */) const
{
	QComboBox* editor = new QComboBox(parent);
	for (unsigned int i = 0; i < Items.size(); ++i)
	{
		editor->addItem(Items[i].c_str());
	}
	return editor;
}

void VariableTypeDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	QComboBox *comboBox = static_cast<QComboBox*>(editor);
	int value = index.model()->data(index, Qt::EditRole).toUInt();
	comboBox->setCurrentIndex(value);
}

void VariableTypeDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
	QComboBox *comboBox = static_cast<QComboBox*>(editor);
	model->setData(index, comboBox->currentIndex(), Qt::EditRole);
}

void VariableTypeDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
{
	editor->setGeometry(option.rect);
}

void VariableTypeDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItemV4 myOption = option;
	QString text = Items[index.row()].c_str();

	myOption.text = text;

	QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &myOption, painter);
}
