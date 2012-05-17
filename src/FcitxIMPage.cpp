/***************************************************************************
 *   Copyright (C) 2011~2011 by CSSlayer                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

// Qt
#include <QPainter>

// KDE
#include <KCategorizedSortFilterProxyModel>
#include <KCategoryDrawer>

// Fcitx
#include <fcitx/module/dbus/dbusstuff.h>
#include <fcitx/module/ipc/ipc.h>

// self
#include "FcitxIMPage.h"
#include "FcitxIM.h"
#include "ui_FcitxIMPage.h"
#include "FcitxIMPage_p.h"

#define MARGIN 0

namespace Fcitx
{

static QString languageName(const QString& langCode)
{
    if (langCode.isEmpty()) {
        return i18n("Unknown");
    }
    else if (langCode == "*")
        return i18n("Multilingual");
    else
        return KGlobal::locale()->languageCodeToName(langCode);
}

FcitxIMPage::Private::IMModel::IMModel(FcitxIMPage::Private *d, QObject* parent)
    : QAbstractListModel(parent),
      impage_d(d),
      showOnlyEnabled(false)
{
    connect(d, SIGNAL(updateIMList(QString)), this, SLOT(filterIMEntryList(QString)));
}

FcitxIMPage::Private::IMModel::~IMModel()
{
}

QModelIndex FcitxIMPage::Private::IMModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);

    return createIndex(row, column, (row < filteredIMEntryList.count()) ? (void*) &filteredIMEntryList.at(row) : 0);
}

QVariant FcitxIMPage::Private::IMModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= filteredIMEntryList.size()) {
        return QVariant();
    }

    const FcitxIM& imEntry = filteredIMEntryList.at(index.row());

    switch (role) {

    case Qt::DisplayRole:
        return imEntry.name();

    case KCategorizedSortFilterProxyModel::CategoryDisplayRole: // fall through

    case KCategorizedSortFilterProxyModel::CategorySortRole:

        return languageName(imEntry.langCode());

    default:
        return QVariant();
    }
}

bool FcitxIMPage::Private::IMModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    return false;
}

int FcitxIMPage::Private::IMModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return filteredIMEntryList.count();
}

void FcitxIMPage::Private::IMModel::setShowOnlyEnabled(bool show)
{
    if (show != showOnlyEnabled) {
        showOnlyEnabled = show;
        filterIMEntryList();
    }
}

void FcitxIMPage::Private::IMModel::filterIMEntryList(const QString& selection)
{
    impage_d->availIMProxyModel->setCategorizedModel(false);
    FcitxIMList imEntryList = impage_d->getIMList();
    beginRemoveRows(QModelIndex(), 0, filteredIMEntryList.size());
    filteredIMEntryList.clear();
    endRemoveRows();
    int row = 0, selectionRow = -1, count = 0;
    Q_FOREACH(const FcitxIM & im, imEntryList) {
        if ((showOnlyEnabled && im.enabled()) || (!showOnlyEnabled && !im.enabled())) {
            count ++;
        }
    }
    beginInsertRows(QModelIndex(), 0, count - 1);
    Q_FOREACH(const FcitxIM & im, imEntryList) {
        if ((showOnlyEnabled && im.enabled()) || (!showOnlyEnabled && !im.enabled())) {
            filteredIMEntryList.append(im);
            if (im.uniqueName() == selection)
                selectionRow = row;
            row ++;
        }
    }
    endInsertRows();

    impage_d->availIMProxyModel->sort(0);

    if (selectionRow >= 0) {
        emit select(index(selectionRow, 0));
    }
    else if (showOnlyEnabled && count > 0) {
        emit select(index(count - 1, 0));
    }

    impage_d->availIMProxyModel->setCategorizedModel(true);
}

FcitxIMPage::Private::IMProxyModel::IMProxyModel(FcitxIMPage::Private *d, QObject* parent)
    : KCategorizedSortFilterProxyModel(parent),
      impage_d(d)
{

}
FcitxIMPage::Private::IMProxyModel::~IMProxyModel()
{
}

bool FcitxIMPage::Private::IMProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    Q_UNUSED(source_parent)

    const QModelIndex index = sourceModel()->index(source_row, 0);
    const FcitxIM* imEntry = static_cast<FcitxIM*>(index.internalPointer());
    bool flag = true;

    flag = flag && (impage_d->onlyCurrentLanguageCheckBox->isChecked() ? imEntry->langCode().startsWith(KGlobal::locale()->language().left(2)) : true );
    if (!impage_d->filterTextEdit->text().isEmpty()) {
        flag = flag &&
               (imEntry->name().contains(impage_d->filterTextEdit->text(), Qt::CaseInsensitive)
               || imEntry->uniqueName().contains(impage_d->filterTextEdit->text(), Qt::CaseInsensitive)
               || imEntry->langCode().contains(impage_d->filterTextEdit->text(), Qt::CaseInsensitive)
               || languageName(imEntry->langCode()).contains(impage_d->filterTextEdit->text(), Qt::CaseInsensitive));
    }
    return flag;
}

bool FcitxIMPage::Private::IMProxyModel::subSortLessThan(const QModelIndex& left, const QModelIndex& right) const
{
    return QString(static_cast<FcitxIM*>(left.internalPointer())->name()).compare((QString)(static_cast<FcitxIM*>(right.internalPointer())->name()), Qt::CaseInsensitive) < 0;
}

FcitxIMPage::FcitxIMPage(QWidget* parent): QWidget(parent),
    m_ui(new Ui::FcitxIMPage),
    d(new Private(this))
{
    m_ui->setupUi(this);

    m_ui->addIMButton->setIcon(KIcon("go-next"));
    m_ui->removeIMButton->setIcon(KIcon("go-previous"));
    m_ui->moveUpButton->setIcon(KIcon("go-up"));
    m_ui->moveDownButton->setIcon(KIcon("go-down"));

    d->addIMButton = m_ui->addIMButton;
    d->removeIMButton = m_ui->removeIMButton;
    d->moveUpButton = m_ui->moveUpButton;
    d->moveDownButton = m_ui->moveDownButton;
    d->availIMView = m_ui->availIMView;
    d->currentIMView = m_ui->currentIMView;

    d->onlyCurrentLanguageCheckBox = m_ui->onlyCurrentLanguageCheckBox;
    d->filterTextEdit = m_ui->filterTextEdit;

    d->filterTextEdit->setClearButtonShown(true);
    d->filterTextEdit->setClickMessage(i18n("Search Input Method"));

    d->availIMView->setVerticalScrollMode(QListView::ScrollPerPixel);
    d->availIMView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    d->categoryDrawer = new KCategoryDrawerV3(d->availIMView);
    d->availIMView->setCategoryDrawer(d->categoryDrawer);

    d->availIMProxyModel = new Private::IMProxyModel(d, this);
    d->availIMModel = new Private::IMModel(d, this);
    d->availIMProxyModel->setSourceModel(d->availIMModel);
    d->availIMProxyModel->setCategorizedModel(true);
    d->availIMView->setModel(d->availIMProxyModel);
    d->availIMView->setAlternatingBlockColors(true);
    d->availIMView->setSelectionMode(QAbstractItemView::SingleSelection);
    d->availIMView->setMouseTracking(true);
    d->availIMView->viewport()->setAttribute(Qt::WA_Hover);

    d->currentIMModel = new Private::IMModel(d, this);
    d->currentIMModel->setShowOnlyEnabled(true);
    d->currentIMView->setModel(d->currentIMModel);
    d->currentIMView->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(d->filterTextEdit, SIGNAL(textChanged(QString)), this, SLOT(invalidate()));
    connect(d->onlyCurrentLanguageCheckBox, SIGNAL(toggled(bool)), this, SLOT(invalidate()));
    connect(d->availIMView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), d, SLOT(availIMSelectionChanged()));
    connect(d->currentIMView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), d, SLOT(currentIMCurrentChanged()));
    connect(d->addIMButton, SIGNAL(clicked(bool)), d, SLOT(addIM()));
    connect(d->removeIMButton, SIGNAL(clicked(bool)), d, SLOT(removeIM()));
    connect(d->moveUpButton, SIGNAL(clicked(bool)), d, SLOT(moveUpIM()));
    connect(d->moveDownButton, SIGNAL(clicked(bool)), d, SLOT(moveDownIM()));
    connect(d, SIGNAL(changed()), this, SIGNAL(changed()));
    connect(d->availIMModel, SIGNAL(select(QModelIndex)), d, SLOT(selectAvailIM(QModelIndex)));
    connect(d->currentIMModel, SIGNAL(select(QModelIndex)), d, SLOT(selectCurrentIM(QModelIndex)));

    d->fetchIMList();
}

void FcitxIMPage::invalidate()
{
    d->availIMProxyModel->setCategorizedModel(false);
    d->availIMProxyModel->invalidate();
    d->availIMProxyModel->sort(0);
    d->availIMProxyModel->setCategorizedModel(true);
}

void FcitxIMPage::save()
{
    d->save();
}

void FcitxIMPage::load()
{
    d->fetchIMList();
}

FcitxIMPage::Private::Private(QObject* parent)
    : QObject(parent),
      availIMModel(0),
      m_connection(QDBusConnection::sessionBus())
{
    m_inputmethod = new org::fcitx::Fcitx::InputMethod(
        QString("%1-%2").arg(FCITX_DBUS_SERVICE).arg(fcitx_utils_get_display_number()),
        FCITX_IM_DBUS_PATH,
        m_connection,
        this
    );
}

FcitxIMPage::Private::~Private()
{
}

void FcitxIMPage::Private::availIMSelectionChanged()
{
    if (!availIMView->currentIndex().isValid())
        addIMButton->setEnabled(false);
    else
        addIMButton->setEnabled(true);
}

void FcitxIMPage::Private::currentIMCurrentChanged()
{
    if (!currentIMView->currentIndex().isValid()) {
        removeIMButton->setEnabled(false);
        moveUpButton->setEnabled(false);
        moveDownButton->setEnabled(false);
    } else {
        if (currentIMView->currentIndex().row() == 0)
            moveUpButton->setEnabled(false);
        else
            moveUpButton->setEnabled(true);
        if (currentIMView->currentIndex().row() == currentIMModel->rowCount() - 1)
            moveDownButton->setEnabled(false);
        else
            moveDownButton->setEnabled(true);
        removeIMButton->setEnabled(true);
    }
}

void FcitxIMPage::Private::selectCurrentIM(const QModelIndex& index)
{
    currentIMView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
}

void FcitxIMPage::Private::selectAvailIM(const QModelIndex& index)
{
    availIMView->selectionModel()->setCurrentIndex(
        availIMProxyModel->mapFromSource(index),
        QItemSelectionModel::ClearAndSelect
    );
}

void FcitxIMPage::Private::addIM()
{
    if (availIMView->currentIndex().isValid()) {
        FcitxIM* im = static_cast<FcitxIM*>(availIMProxyModel->mapToSource(availIMView->currentIndex()).internalPointer());
        int i = 0;
        for (i = 0; i < m_list.size(); i ++) {
            if (im->uniqueName() == m_list[i].uniqueName()) {
                m_list[i].setEnabled(true);
                qStableSort(m_list.begin(), m_list.end());
                emit updateIMList(im->uniqueName());
                emit changed();
                break;
            }
        }
    }
}

void FcitxIMPage::Private::removeIM()
{
    if (currentIMView->currentIndex().isValid()) {
        FcitxIM* im = static_cast<FcitxIM*>(currentIMView->currentIndex().internalPointer());
        int i = 0;
        for (i = 0; i < m_list.size(); i ++) {
            if (im->uniqueName() == m_list[i].uniqueName()) {
                m_list[i].setEnabled(false);
                qStableSort(m_list.begin(), m_list.end());
                emit updateIMList(im->uniqueName());
                emit changed();
                break;
            }
        }
    }
}

void FcitxIMPage::Private::moveDownIM()
{
    QModelIndex curIndex = currentIMView->currentIndex();
    if (curIndex.isValid()) {
        QModelIndex nextIndex = currentIMModel->index(curIndex.row() + 1, 0);

        FcitxIM* curIM = static_cast<FcitxIM*>(curIndex.internalPointer());
        FcitxIM* nextIM = static_cast<FcitxIM*>(nextIndex.internalPointer());

        if (curIM == NULL || nextIM == NULL)
            return;

        int i = 0, curIMIdx = -1, nextIMIdx = -1;
        for (i = 0; i < m_list.size(); i ++) {
            if (curIM->uniqueName() == m_list[i].uniqueName())
                curIMIdx = i;

            if (nextIM->uniqueName() == m_list[i].uniqueName())
                nextIMIdx = i;
        }

        if (curIMIdx >= 0 && nextIMIdx >= 0 && curIMIdx != nextIMIdx) {
            m_list.swap(curIMIdx, nextIMIdx);
            qStableSort(m_list.begin(), m_list.end());
            emit updateIMList(curIM->uniqueName());
            emit changed();
        }
    }
}

void FcitxIMPage::Private::moveUpIM()
{
    QModelIndex curIndex = currentIMView->currentIndex();
    if (curIndex.isValid() && curIndex.row() > 0) {
        QModelIndex nextIndex = currentIMModel->index(curIndex.row() - 1, 0);

        FcitxIM* curIM = static_cast<FcitxIM*>(curIndex.internalPointer());
        FcitxIM* nextIM = static_cast<FcitxIM*>(nextIndex.internalPointer());

        if (curIM == NULL || nextIM == NULL)
            return;

        int i = 0, curIMIdx = -1, nextIMIdx = -1;
        for (i = 0; i < m_list.size(); i ++) {
            if (curIM->uniqueName() == m_list[i].uniqueName())
                curIMIdx = i;

            if (nextIM->uniqueName() == m_list[i].uniqueName())
                nextIMIdx = i;
        }

        if (curIMIdx >= 0 && nextIMIdx >= 0 && curIMIdx != nextIMIdx) {
            m_list.swap(curIMIdx, nextIMIdx);
            qStableSort(m_list.begin(), m_list.end());
            emit updateIMList(curIM->uniqueName());
            emit changed();
        }
    }
}

void FcitxIMPage::Private::save()
{
    if (m_inputmethod->isValid())
        m_inputmethod->setIMList(m_list);
}

void FcitxIMPage::Private::fetchIMList()
{
    if (m_inputmethod->isValid()) {
        m_list = m_inputmethod->iMList();
        qStableSort(m_list.begin(), m_list.end());
        emit updateIMList(QString());
    }
}

const FcitxIMList& FcitxIMPage::Private::getIMList()
{
    return m_list;
}

int FcitxIMPage::Private::dependantLayoutValue(int value, int width, int totalWidth) const
{
    if (availIMView->layoutDirection() == Qt::LeftToRight) {
        return value;
    }

    return totalWidth - width - value;
}

FcitxIMPage::~FcitxIMPage()
{
    delete m_ui;
}

}
