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
#include "ui/lists/ZDLSourcePortList.h"

#include <QPushButton>

#include "ui/lists/ZDLNameListable.h"
#include "config/ZDLConfigurationManager.h"
#include "ui/dialogs/ZDLNameInput.h"
#include "wad/ZDLFileInfo.h"
#include "gph_ast.xpm"
#include "core/zdlcommon.h"

namespace {
/* Built on first use so that no QString is constructed before main(). */
const QString &srcFilters() {
#ifdef _WIN32
    static const QString filters = "Executables (*.exe);;All files (*.*)";
#elif defined(Q_WS_MAC)
    static const QString filters = "Applications (*.app);;All files (*)";
#else
    static const QString filters = "All files (*)";
#endif
    return filters;
}
}  // namespace

ZDLSourcePortList::ZDLSourcePortList(ZDLWidget *parent) : ZDLListWidget(parent) {
    auto *btnWizardAdd = new QPushButton(this);
    btnWizardAdd->setIcon(QPixmap(glyph_asterisk));
    btnWizardAdd->setToolTip("Add and name item");
    buttonRow->insertWidget(0, btnWizardAdd);

    QObject::connect(btnWizardAdd, SIGNAL(clicked()), this, SLOT(wizardAddButton()));
}

void ZDLSourcePortList::wizardAddButton() {
    ZDLAppInfo zdl_fi;
    ZDLNameInput diag(this, getSrcLastDir(), &zdl_fi, false, true);
    diag.setWindowTitle("Add source port");
    diag.setFilter(srcFilters());
    if (diag.exec() != 0) {
        saveSrcLastDir(diag.getFile());
        insert(new ZDLNameListable(pList, 1001, diag.getFile(), diag.getName()), -1);
    }
}

void ZDLSourcePortList::newConfig() {
    pList->clear();
    const ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    for (const ZDLNameEntry &entry: config->ports) {
        insert(new ZDLNameListable(pList, 1001, entry.file, entry.name), -1);
    }
}

void ZDLSourcePortList::rebuild() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    config->ports.clear();
    for (int i = 0; i < count(); i++) {
        auto *fitm = dynamic_cast<ZDLNameListable *>(pList->item(i));
        config->ports.append(ZDLNameEntry{.name = fitm->getName(), .file = fitm->getFile()});
    }
}

void ZDLSourcePortList::newDrop(const QStringList &fileList) {
    LOGDATAO() << "newDrop" << Qt::endl;
    for (const QString &i: fileList) {
        insert(new ZDLNameListable(pList, 1001, i, ZDLAppInfo(i).GetFileDescription()), -1);
    }
}

void ZDLSourcePortList::addButton() {
    LOGDATAO() << "Adding new source ports" << Qt::endl;

    QStringList const fileNames = QFileDialog::getOpenFileNames(this, "Add source ports", getSrcLastDir(), srcFilters());
    for (const QString &fileName: fileNames) {
        LOGDATAO() << "Adding file " << fileName << Qt::endl;
        saveSrcLastDir(fileName);
        insert(new ZDLNameListable(pList, 1001, fileName, ZDLAppInfo(fileName).GetFileDescription()), -1);
    }
}

void ZDLSourcePortList::editButton(QListWidgetItem *item) {
    if (item != nullptr) {
        auto *zitem = dynamic_cast<ZDLNameListable *>(item);
        ZDLAppInfo zdl_fi;
        ZDLNameInput diag(this, getSrcLastDir(), &zdl_fi, false, true);
        diag.setWindowTitle("Edit source port");
        diag.setFilter(srcFilters());
        diag.basedOff(zitem);
        if (diag.exec() != 0) {
            saveSrcLastDir(diag.getFile());
            zitem->setDisplayName(diag.getName());
            zitem->setFile(diag.getFile());
        }
    }
}
