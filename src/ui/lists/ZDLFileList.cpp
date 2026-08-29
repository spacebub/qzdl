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

#include <QFileDialog>
#include <utility>
#include "ui/lists/ZDLFileList.h"

#include <QPushButton>

#include "ui/lists/ZDLFileListable.h"
#include "config/ZDLConfigurationManager.h"
#include "gph_fld.xpm"
#include "core/zdlcommon.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

ZDLFileList::ZDLFileList(ZDLWidget *parent) :
        ZDLListWidget(parent) {
    LOGDATAO() << "ZDLFileList" << Qt::endl;

    auto *btnFolder = new QPushButton(this);
    btnFolder->setIcon(QPixmap(glyph_folder));
    btnFolder->setToolTip("Add directory");
    buttonRow->insertWidget(0, btnFolder);

    btnAdd->setToolTip("Add files");
    btnRem->setToolTip("Remove selected files and directories");
    btnEdt->setToolTip("Disable (enable) files and directories");
    btnUp->setToolTip("Move selected files and directories up");
    btnDn->setToolTip("Move selected files and directories down");

    QObject::connect(btnFolder, SIGNAL(clicked()), this, SLOT(folderButton()));

#ifdef _WIN32
    //On Win32 QFileDialog::getExistingDirectory(QFileDialog::ShowDirsOnly) will try to use native Win32 dialog for selecting directories
    //Since ancient times it was just shitty SHBrowseForFolder but in Vista Microsoft introduced new and shiny Common Item Dialog family available via COM
    //And they handle 'dir' param of getExistingDirectory differently
    //Being tree dialog, SHBrowseForFolder will expand directory tree right up to 'dir' and select it, so 'dir' itself will be visible but not it's nodes
    //Common Item Dialog, behaving more like traditional GetOpenFileName, will instead show user contents of the 'dir'
    //That's why we check if Common Item Dialog is available and, if available, pass last opened dir's parent instead of actual path to getExistingDirectory
    //The same goes to Linux select file dialogs which are handled by Qt
    CoInitialize(nullptr);
    IFileDialog *pDialog;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDialog))))
        pDialog->Release();
    else
        basic_fileopendialog=true;
    CoUninitialize();
#endif
}

void ZDLFileList::newDrop(const QStringList &fileList) {
    LOGDATAO() << "newDrop" << Qt::endl;
    for (const QString &i: fileList) {
        insert(new ZDLFileListable(pList, 1001, i), -1);
    }
}

void ZDLFileList::newConfig() {
    LOGDATAO() << "Reading new config" << Qt::endl;
    pList->clear();
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    for (const ZDLFileEntry &entry: std::as_const(config->activeProfile().files)) {
        auto *zList = new ZDLFileListable(pList, 1001, entry.file);
        if (!entry.enabled) {
            QFont item_font = zList->font();
            item_font.setStrikeOut(true);
            zList->setFont(item_font);
        }
        insert(zList, -1);
    }
}

void ZDLFileList::rebuild() {
    LOGDATAO() << "Saving config" << Qt::endl;
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    QVector<ZDLFileEntry> &files = config->activeProfile().files;
    files.clear();
    for (int i = 0; i < count(); i++) {
        auto *fitm = dynamic_cast<ZDLFileListable *>(pList->item(i));
        // A struck through item is disabled: kept in the list, off the command line.
        files.append(ZDLFileEntry{.file = fitm->getFile(), .enabled = !fitm->font().strikeOut()});
    }
}

void ZDLFileList::addButton() {
    LOGDATAO() << "Adding new file" << Qt::endl;
    QString const filters =
            "Doom resource files (*.wad" QFD_FILTER_DELIM "*.iwad" QFD_FILTER_DELIM "*.zip" QFD_FILTER_DELIM "*.pk3" QFD_FILTER_DELIM "*.ipk3" QFD_FILTER_DELIM "*.7z" QFD_FILTER_DELIM "*.pk7" QFD_FILTER_DELIM "*.ipk7" QFD_FILTER_DELIM "*.p7z" QFD_FILTER_DELIM "*.pkz" QFD_FILTER_DELIM "*.pke" QFD_FILTER_DELIM "*.bex" QFD_FILTER_DELIM "*.deh" QFD_FILTER_DELIM "*.cfg);;"
            "WAD files (*.wad" QFD_FILTER_DELIM "*.iwad);;"
            "Patch files (*.bex" QFD_FILTER_DELIM "*.deh);;"
            "Config files (*.cfg);;"
            "Demo files (*.lmp);;"
            "All supported archives (*.zip" QFD_FILTER_DELIM "*.pk3" QFD_FILTER_DELIM "*.ipk3" QFD_FILTER_DELIM "*.7z" QFD_FILTER_DELIM "*.pk7" QFD_FILTER_DELIM "*.ipk7" QFD_FILTER_DELIM "*.p7z" QFD_FILTER_DELIM "*.pkz" QFD_FILTER_DELIM "*.pke);;"
            "Specialized archives (*.pk3" QFD_FILTER_DELIM "*.ipk3" QFD_FILTER_DELIM "*.pk7" QFD_FILTER_DELIM "*.ipk7" QFD_FILTER_DELIM "*.p7z" QFD_FILTER_DELIM "*.pkz" QFD_FILTER_DELIM "*.pke);;"
            "All files (" QFD_FILTER_ALL ")";

    QStringList const fileNames = QFileDialog::getOpenFileNames(this, "Add files", getWadLastDir(), filters);
    for (const QString &fileName: fileNames) {
        LOGDATAO() << "Adding file " << fileName << Qt::endl;
        saveWadLastDir(fileName);
        auto *zList = new ZDLFileListable(pList, 1001, fileName);
        insert(zList, -1);
    }
}

void ZDLFileList::editButton(QListWidgetItem *item) {
    if (item != nullptr) {
        QFont item_font = item->font();
        if (item_font.strikeOut()) {
            item_font.setStrikeOut(false);
        } else {
            item_font.setStrikeOut(true);
        }
        item->setFont(item_font);
    }
}

void ZDLFileList::editButton(const QList<QListWidgetItem *> &items) {
    QList<QListWidgetItem *> en_items;
    bool disable_all = true;

    for (QListWidgetItem *item: items) {
        if (!item->font().strikeOut()) {
            en_items.append(item);
            disable_all = false;
        }
    }

    for (QListWidgetItem *item: (disable_all ? items : en_items)) {
        editButton(item);
    }
}

void ZDLFileList::editButton() {
    if (!pList->selectedItems().empty()) {
        editButton(pList->selectedItems());
    } else {
        editButton(pList->findItems("*", Qt::MatchWildcard));
    }
}

void ZDLFileList::folderButton() {
    LOGDATAO() << "Adding new dir" << Qt::endl;

    QString last_dir = getWadLastDir();
    if (!basic_fileopendialog) {
        QDir parent_dir(last_dir);
        if (parent_dir.cdUp()) {
            last_dir = parent_dir.path();
        }
    }

    QString const dirName = QFileDialog::getExistingDirectory(this, "Add directory", last_dir,
                                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dirName.isEmpty()) {
        LOGDATAO() << "Adding dir " << dirName << Qt::endl;
        saveWadLastDir(dirName, true);
        auto *zList = new ZDLFileListable(pList, 1001, QFD_QT_SEP(dirName));
        insert(zList, -1);
    }
}
