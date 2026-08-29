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
#include <QLabel>
#include <QLineEdit>
#include "config/ZDLConfigurationManager.h"
#include "ui/ZDLSettingsTab.h"
#include "ui/ZDLQSplitter.h"

#if defined(_WIN32) && !defined(_ZDL_NO_WFA)
#include "platform/win32/ZDLFileAssociations.h"
#endif

ZDLSettingsTab::ZDLSettingsTab(QWidget *parent) :
        ZDLWidget(parent),
        alwaysArgs(new QLineEdit(this)),
        iwadList(new ZDLIWadList(this)),
        sourceList(new ZDLSourcePortList(this)) {
    LOGDATAO() << "New ZDLSettingsTab" << Qt::endl;
    auto *sections = new QVBoxLayout(this);

    auto *split = new ZDLQSplitter(this);
    split->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    QSplitter *rsplit = split->getSplit();

    //IWAD
    auto *rwidget = new QWidget(rsplit);
    auto *rpane = new QVBoxLayout();

    iwadList->doDragDrop(1);
    rpane->addWidget(new QLabel("IWADs", this));
    rpane->addWidget(iwadList);
    rwidget->setLayout(rpane);
    rpane->setContentsMargins(0, 0, 0, 0);

    //Source Port
    auto *lwidget = new QWidget(rsplit);
    auto *lpane = new QVBoxLayout();

    sourceList->doDragDrop(1);
    lpane->addWidget(new QLabel("Source ports", this));
    lpane->addWidget(sourceList);
    lwidget->setLayout(lpane);
    lpane->setContentsMargins(0, 0, 0, 0);

    split->addChild(lwidget);
    split->addChild(rwidget);

    //Add all the sections together
    sections->addWidget(new QLabel("Always add these parameters", this));

    launchClose = new QCheckBox("Close on launch", this);
    launchClose->setToolTip("Close ZDL completely when launching a new game");

    showPaths = new QCheckBox("Show file paths in lists", this);
    showPaths->setToolTip("Show the directory path in square brackets in list widgets");
    connect(showPaths, SIGNAL(stateChanged(int)), this, SLOT(pathToggled(int)));
    sections->addWidget(alwaysArgs);

    auto *fileassoc = new QHBoxLayout();
    launchZDL = new QCheckBox("Launch *.ZDL files transparently", this);
    launchZDL->setToolTip(
            "If a .ZDL file is specified on the command line path, launch the configuration without showing the interface");
    fileassoc->addWidget(launchZDL);

#if defined(_WIN32) && !defined(_ZDL_NO_WFA)
    QPushButton *assoc = new QPushButton("Associations", this);
    assoc->setToolTip("Associate various file types with ZDL");
    fileassoc->addWidget(assoc);
    connect(assoc, SIGNAL(clicked()), this, SLOT(fileAssociations()));
#endif

    savePaths = new QCheckBox("Remember external file list", this);
    savePaths->setToolTip("Save external file list on exit and load it on next program launch");

    sections->addLayout(fileassoc);
    sections->addWidget(split);
    sections->addWidget(launchClose);
    sections->addWidget(showPaths);
    sections->addWidget(savePaths);
    setContentsMargins(4, 4, 4, 4);
    layout()->setContentsMargins(0, 0, 0, 0);
}

void ZDLSettingsTab::pathToggled([[maybe_unused]] int state) {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }
    config->general.showPaths = showPaths->checkState() == Qt::Checked;
    iwadList->newConfig();
    sourceList->newConfig();
}

void ZDLSettingsTab::fileAssociations() {
#if defined(_WIN32) && !defined(_ZDL_NO_WFA)
    ZDLFileAssociations assoc(this);
    assoc.exec();
#endif
}

void ZDLSettingsTab::rebuild() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }
    ZDLGeneralSettings &general = config->general;

    general.autoClose = launchClose->checkState() == Qt::Checked;
    general.launchZdlImmediately = launchZDL->checkState() == Qt::Checked;
    general.alwaysAdd = alwaysArgs->text();
    general.showPaths = showPaths->checkState() == Qt::Checked;
    general.rememberFileList = savePaths->checkState() == Qt::Checked;
}

void ZDLSettingsTab::newConfig() {
    const ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }
    const ZDLGeneralSettings &general = config->general;

    showPaths->setCheckState(general.showPaths ? Qt::Checked : Qt::Unchecked);
    alwaysArgs->setText(general.alwaysAdd);
    launchClose->setCheckState(general.autoClose ? Qt::Checked : Qt::Unchecked);
    launchZDL->setCheckState(general.launchZdlImmediately ? Qt::Checked : Qt::Unchecked);
    savePaths->setCheckState(general.rememberFileList ? Qt::Checked : Qt::Unchecked);
}

void ZDLSettingsTab::reloadConfig() {
    LOGDATAO() << "Reloading config" << Qt::endl;
    writeConfig();
    startRead();
    LOGDATAO() << "Reload complete" << Qt::endl;
}

void ZDLSettingsTab::startRead() {
    LOGDATAO() << "Reading new configuration" << Qt::endl;
    emit readChildren(this);
    newConfig();
}

void ZDLSettingsTab::writeConfig() {
    LOGDATAO() << "Writing configuration" << Qt::endl;
    emit buildChildren(this);
    rebuild();
}
