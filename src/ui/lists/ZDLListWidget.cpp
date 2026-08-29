/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023  spacebub
 * 
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QApplication>
#include <QAction>
#include <QMessageBox>
#include <QDragLeaveEvent>
#include "config/ZDLConfigurationManager.h"
#include "ui/lists/ZDLListWidget.h"
#include "gph_upt.xpm"
#include "gph_dna.xpm"
#include "gph_upa.xpm"
#include "gph_pls.xpm"
#include "gph_mns.xpm"

ZDLListWidget::ZDLListWidget(ZDLWidget *parent) : ZDLWidget(parent) {
    auto *column = new QVBoxLayout(this);
    pList = new QListWidget(this);
    pList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    buttonRow = new QHBoxLayout();

    auto *delact = new QAction(this);
    delact->setShortcut(Qt::Key_Delete);
    delact->setShortcutContext(Qt::WidgetShortcut);
    connect(delact, SIGNAL(triggered()), this, SLOT(removeButton()));

    auto *insact = new QAction(this);
    insact->setShortcut(Qt::Key_Insert);
    insact->setShortcutContext(Qt::WidgetShortcut);
    connect(insact, SIGNAL(triggered()), this, SLOT(addButton()));

    pList->addAction(delact);
    pList->addAction(insact);

    btnAdd = new QPushButton(this);
    btnAdd->setIcon(QPixmap(glyph_plus));

    btnRem = new QPushButton(this);
    btnRem->setIcon(QPixmap(glyph_minus));

    btnEdt = new QPushButton(this);
    btnEdt->setIcon(QPixmap(glyph_up_trg));

    btnUp = new QPushButton(this);
    btnUp->setIcon(QPixmap(glyph_up_arr));

    btnDn = new QPushButton(this);
    btnDn->setIcon(QPixmap(glyph_down_arr));

    btnAdd->setToolTip("Add items");
    btnRem->setToolTip("Remove selected items");
    btnEdt->setToolTip("Edit selected item");
    btnUp->setToolTip("Move selected items up");
    btnDn->setToolTip("Move selected items down");

    buttonRow->addWidget(btnAdd);
    buttonRow->addWidget(btnRem);
    buttonRow->addWidget(btnEdt);
    buttonRow->addStretch();
    buttonRow->addWidget(btnUp);
    buttonRow->addWidget(btnDn);
    buttonRow->setSpacing(0);

    //Glue it together
    column->addWidget(pList);
    column->addLayout(buttonRow);
    column->setSpacing(0);

    setContentsMargins(0, 0, 0, 0);
    layout()->setContentsMargins(0, 0, 0, 0);

    //signal time
    connect(btnAdd, &QPushButton::clicked, this, &ZDLListWidget::addButton);
    connect(btnRem, &QPushButton::clicked, this, &ZDLListWidget::removeButton);
    connect(btnUp, &QPushButton::clicked, this, &ZDLListWidget::upButton);
    connect(btnDn, &QPushButton::clicked, this, &ZDLListWidget::downButton);
    connect(btnEdt, &QPushButton::clicked, [this]() {
        editButton();
    });
    connect(pList, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        editButton(item);
    });
}

void ZDLListWidget::doDragDrop(int enabled) {
    setAcceptDrops(enabled);
}

void ZDLListWidget::newDrop([[maybe_unused]] const QStringList &files) {
}

void ZDLListWidget::dragEnterEvent(QDragEnterEvent *event) {
    setBackgroundRole(QPalette::Highlight);
    event->acceptProposedAction();
}

void ZDLListWidget::dragMoveEvent(QDragMoveEvent *event) {
    event->acceptProposedAction();
}

void ZDLListWidget::dragLeaveEvent(QDragLeaveEvent *event) {
    setBackgroundRole(QPalette::Dark);
    event->accept();
}

void ZDLListWidget::dropEvent(QDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList(mimeData->urls());
        QStringList files;
        for (int i = 0; i < urlList.size() && i < 32; ++i) {
            QUrl url = (QUrl) urlList.at(i);
            LOGDATAO() << "url " << i << "=" << url.toString() << Qt::endl;
            if (url.scheme() == "file") {
                QString path = url.path();
#ifdef _WIN32
                if(path[2] == ':'){
                    path.remove(0,1);
                }
#endif
                QFileInfo urlDecoder(path);
                LOGDATAO() << "Adding path " << urlDecoder.absoluteFilePath() << Qt::endl;
                files << urlDecoder.absoluteFilePath();
            }
        }

        newDrop(files);
        event->accept();
    }
}

void ZDLListWidget::insert(ZDLListable *item, int index) {
    if (index < 0) {
        pList->addItem(item);
    } else {
        pList->insertItem(index, item);
    }
}

int ZDLListWidget::count() {
    return pList->count();
}

void ZDLListWidget::remove(int index) {
    QListWidgetItem *item = pList->takeItem(index);

    if (!item) {
        QMessageBox::warning(this, "ZDL Error", "You didn't make a selection.");
    } else {
        delete item;
    }
}

ZDLListable *ZDLListWidget::get(int index) {
    if (index >= 0 && index < list.length()) {
        return list[index];
    }
    return nullptr;
}

void ZDLListWidget::addButton() {

}

void ZDLListWidget::removeButton() {
    QList<QListWidgetItem *> slist = pList->selectedItems();
    int selected = -1;
    if (slist.size() == 1) {
        selected = pList->currentRow();
    }
    for (auto &i: slist) {
        int rowid = pList->row(i);
        remove(rowid);
    }
    if (selected != -1) {
        if (pList->count() > 0) {
            if (pList->count() <= selected) {
                pList->setCurrentRow(pList->count() - 1);
            } else {
                pList->setCurrentRow(selected);
            }
        }
    }
}

static void sortItemsByRow(QListWidget *pList, QList<QListWidgetItem *> &items) {
    if (items.empty()) {
        return;
    }
    QList<QListWidgetItem *> ilist;
    ilist.append(items.takeLast());
    while (!items.empty()) {
        QListWidgetItem *item = items.takeLast();
        int newRow = pList->row(item);
        int firstRow = pList->row(ilist.first());
        int lastRow = pList->row(ilist.last());
        if (newRow < firstRow) {
            ilist.push_front(item);
        } else if (newRow > lastRow) {
            ilist.push_back(item);
        } else {
            QList<QListWidgetItem *>::iterator i;
            for (i = ilist.begin(); i != ilist.end(); ++i) {
                int oldRow = pList->row(*i);
                if (newRow < oldRow) {
                    ilist.insert(i, item);
                    break;
                }
            }
        }
    }
    items.append(ilist);
}

void ZDLListWidget::upButton() {
    if (pList->selectedItems().size() == 1) {
        if (pList->currentRow() > 0) {
            int oldRow = pList->currentRow();
            QListWidgetItem *item = pList->takeItem(oldRow);
            insert((ZDLListable *) item, oldRow - 1);
            pList->setCurrentRow(oldRow - 1);
        }
    } else if (pList->selectedItems().size() > 1) {
        QList<QListWidgetItem *> items = pList->selectedItems();
        sortItemsByRow(pList, items);
        for (auto item: items) {
            int row = pList->row(item);
            // Make sure we don't move up and out of the list
            if (row <= 0) {
                return;
            }
        }
        for (auto item: items) {
            int row = pList->row(item);
            item = pList->takeItem(row);
            pList->insertItem(row - 1, item);
        }
        for (auto &item: items) {
            pList->setCurrentItem(item, QItemSelectionModel::Select);
        }
    }
}

void ZDLListWidget::downButton() {
    if (pList->selectedItems().size() == 1) {
        if (pList->currentRow() < pList->count() - 1) {
            int oldRow = pList->currentRow();
            QListWidgetItem *item = pList->takeItem(oldRow);
            insert((ZDLListable *) item, oldRow + 1);
            pList->setCurrentRow(oldRow + 1);
        }
    } else if (pList->selectedItems().size() > 1) {
        QList<QListWidgetItem *> items = pList->selectedItems();
        // Need this check in here to ensure we don't use an invalid index
        if (items.size() <= 1) {
            return;
        }

        sortItemsByRow(pList, items);
        int max = pList->count();
        for (auto item: items) {
            int row = pList->row(item);
            // Make sure we don't run off the end
            if (row >= max - 1) {
                return;
            }
        }
        for (int i = (int) items.size() - 1; i >= 0; i--) {
            QListWidgetItem *item = items[i];
            int row = pList->row(item);
            item = pList->takeItem(row);
            pList->insertItem(row + 1, item);
        }
        for (auto &item: items) {
            pList->setCurrentItem(item, QItemSelectionModel::Select);
        }
    }
}

void ZDLListWidget::editButton([[maybe_unused]] QListWidgetItem *item) {
}

void ZDLListWidget::editButton() {
    editButton(pList->currentItem());
}
