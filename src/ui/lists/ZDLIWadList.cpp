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
#include "core/zdlcommon.h"
#include "ui/lists/ZDLIWadList.h"
#include "ui/lists/ZDLNameListable.h"
#include "config/ZDLConfigurationManager.h"
#include "ui/dialogs/ZDLNameInput.h"
#include "wad/ZDLFileInfo.h"
#include "gph_ast.xpm"

const QString iwad_filters =
        "IWAD files (*.wad" QFD_FILTER_DELIM "*.WAD" QFD_FILTER_DELIM "*.iwad" QFD_FILTER_DELIM "*.IWAD" QFD_FILTER_DELIM "*.ipk3" QFD_FILTER_DELIM "*.ipk7);;"
        "All supported archives (*.zip" QFD_FILTER_DELIM "*.pk3" QFD_FILTER_DELIM "*.ipk3" QFD_FILTER_DELIM "*.7z" QFD_FILTER_DELIM "*.pk7" QFD_FILTER_DELIM "*.ipk7" QFD_FILTER_DELIM "*.p7z" QFD_FILTER_DELIM "*.pkz" QFD_FILTER_DELIM "*.pke);;"
        "Specialized archives (*.pk3" QFD_FILTER_DELIM "*.ipk3" QFD_FILTER_DELIM "*.pk7" QFD_FILTER_DELIM "*.ipk7" QFD_FILTER_DELIM "*.p7z" QFD_FILTER_DELIM "*.pkz" QFD_FILTER_DELIM "*.pke);;"
        "All files (" QFD_FILTER_ALL ")";

ZDLIWadList::ZDLIWadList(ZDLWidget *parent) : ZDLListWidget(parent) {
    auto *btnWizardAdd = new QPushButton(this);
    btnWizardAdd->setIcon(QPixmap(glyph_asterisk));
    btnWizardAdd->setToolTip("Add and name item");
    buttonRow->insertWidget(0, btnWizardAdd);

    QObject::connect(btnWizardAdd, SIGNAL(clicked()), this, SLOT(wizardAddButton()));
}

void ZDLIWadList::wizardAddButton() {
    ZDLIwadInfo zdl_fi;
    ZDLNameInput diag(this, getWadLastDir(true), &zdl_fi, true, false);
    diag.setWindowTitle("Add IWAD");
    diag.setFilter(iwad_filters);
    if (diag.exec() != 0) {
        saveWadLastDir(diag.getFile());
        insert(new ZDLNameListable(pList, 1001, diag.getFile(), diag.getName()), -1);
    }
}

void ZDLIWadList::newConfig() {
    pList->clear();
    const ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    for (const ZDLNameEntry &entry: config->iwads) {
        insert(new ZDLNameListable(pList, 1001, entry.file, entry.name), -1);
    }
}

void ZDLIWadList::rebuild() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    config->iwads.clear();
    for (int i = 0; i < count(); i++) {
        auto *fitm = static_cast<ZDLNameListable *>(pList->item(i));
        config->iwads.append(ZDLNameEntry{fitm->getName(), fitm->getFile()});
    }
}

void ZDLIWadList::newDrop(const QStringList &fileList) {
    LOGDATAO() << "newDrop" << Qt::endl;

    for (const QString &i: fileList)
        insert(new ZDLNameListable(pList, 1001, i, ZDLIwadInfo(i).GetFileDescription()), -1);
}

void ZDLIWadList::addButton() {
    LOGDATAO() << "Adding new IWADs" << Qt::endl;

    QStringList const fileNames = QFileDialog::getOpenFileNames(this, "Add IWADs", getWadLastDir(), iwad_filters);
    for (const QString &fileName: fileNames) {
        LOGDATAO() << "Adding file " << fileName << Qt::endl;
        saveWadLastDir(fileName);
        insert(new ZDLNameListable(pList, 1001, fileName, ZDLIwadInfo(fileName).GetFileDescription()), -1);
    }
}

void ZDLIWadList::editButton(QListWidgetItem *item) {
    if (item != nullptr) {
        auto *zitem = static_cast<ZDLNameListable *>(item);
        ZDLIwadInfo zdl_fi;
        ZDLNameInput diag(this, getWadLastDir(true), &zdl_fi, true, false);
        diag.setWindowTitle("Edit IWAD");
        diag.setFilter(iwad_filters);
        diag.basedOff(zitem);
        if (diag.exec() != 0) {
            saveWadLastDir(diag.getFile());
            zitem->setDisplayName(diag.getName());
            zitem->setFile(diag.getFile());
        }
    }
}

