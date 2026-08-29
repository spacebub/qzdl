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
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include "config/ZDLConfigurationManager.h"
#include "ui/ZDLInputWidgets.h"
#include "ui/ZDLMultiPane.h"

void PlayersValidator::fixup([[maybe_unused]] QString &input) const {
    validated_cb->setEditable(false);
    validated_cb->setCurrentIndex(0);
}

ZDLMultiPane::ZDLMultiPane(ZDLWidget *parent) : ZDLWidget(parent) {
    launch_btn = nullptr;

    auto *box = new QVBoxLayout(this);

    gMode = new QComboBox(this);
    gMode->addItem("Singleplayer");
    gMode->addItem("Co-op");
    gMode->addItem("Deathmatch");
    gMode->addItem("AltDeathmatch");

    max_int_validator = new QIntValidator(0, INT_MAX, this);

    tHostAddy = new QLineEdit(this);

    gPlayers = new QComboBox(this);
    gPlayers->setInsertPolicy(QComboBox::NoInsert);
    gPlayers->addItem("Joining");
    gPlayers->addItem("1");
    gPlayers->addItem("2");
    gPlayers->addItem("3");
    gPlayers->addItem("4");
    gPlayers->addItem("5");
    gPlayers->addItem("6");
    gPlayers->addItem("7");
    gPlayers->addItem("8");
    gPlayers->addItem("(More...)");

    players_validator = new PlayersValidator(this, gPlayers);

    tFragLimit = new QLineEdit(this);
    tFragLimit->setValidator(max_int_validator);

    tTimeLimit = new QLineEdit(this);
    tTimeLimit->setValidator(max_int_validator);

    extratic = new QComboBox(this);
    netmode = new QComboBox(this);
    portNo = new QLineEdit(this);
    portNo->setValidator(new QIntValidator(0, 65535, this));
    dupmode = new QComboBox(this);
    savegame = new VerboseComboBox(this);
    savegame->setInsertPolicy(QComboBox::NoInsert);
    savegame->setEditable(true);
    savegame->setValidator(new EvilValidator(this));
    savegame->setCompleter(nullptr);
    savegame->addItem("(None)");

    netmode->addItem("(Default)");
    netmode->addItem("0 (Classic P2P)");
    netmode->addItem("1 (Client/Server)");

    extratic->addItem("Off (Default)");
    extratic->addItem("On");

    dupmode->addItem("(Default)");
    dupmode->addItem("1");
    dupmode->addItem("2");
    dupmode->addItem("3");
    dupmode->addItem("4");
    dupmode->addItem("5");
    dupmode->addItem("6");
    dupmode->addItem("7");
    dupmode->addItem("8");
    dupmode->addItem("9");

    bDMFlags = new QLineEdit("", this);
    bDMFlags->setValidator(max_int_validator);
    bDMFlags2 = new QLineEdit("", this);
    bDMFlags2->setValidator(max_int_validator);

    auto *topGrid = new QGridLayout();

    topGrid->addWidget(new QLabel("Game mode", this), 0, 0);
    topGrid->addWidget(gMode, 1, 0);
    topGrid->addWidget(new QLabel("Hostname/IP", this), 0, 1, 1, 3);
    topGrid->addWidget(tHostAddy, 1, 1, 1, 3);
    topGrid->addWidget(new QLabel("Port", this), 0, 4);
    topGrid->addWidget(portNo, 1, 4);

    topGrid->addWidget(new QLabel("Players", this), 2, 0);
    topGrid->addWidget(gPlayers, 3, 0);
    topGrid->addWidget(new QLabel("Frag limit", this), 2, 1);
    topGrid->addWidget(tFragLimit, 3, 1);
    topGrid->addWidget(new QLabel("Time limit", this), 2, 2);
    topGrid->addWidget(tTimeLimit, 3, 2);
    topGrid->addWidget(new QLabel("DMFLAGS", this), 2, 3);
    topGrid->addWidget(bDMFlags, 3, 3);
    topGrid->addWidget(new QLabel("DMFLAGS2", this), 2, 4);
    topGrid->addWidget(bDMFlags2, 3, 4);

    topGrid->addWidget(new QLabel("Net mode", this), 4, 0);
    topGrid->addWidget(netmode, 5, 0);
    topGrid->addWidget(new QLabel("Dup", this), 4, 1);
    topGrid->addWidget(dupmode, 5, 1);
    topGrid->addWidget(new QLabel("Extratic", this), 4, 2);
    topGrid->addWidget(extratic, 5, 2);
    topGrid->addWidget(new QLabel("Savegame", this), 4, 3, 1, 2);
    topGrid->addWidget(savegame, 5, 3, 1, 2);

    topGrid->setColumnStretch(1, 2);
    topGrid->setColumnStretch(2, 2);
    topGrid->setColumnStretch(3, 3);
    topGrid->setColumnStretch(4, 3);

    topGrid->setSpacing(2);
    box->addLayout(topGrid);

    setContentsMargins(0, 0, 0, 0);
    layout()->setContentsMargins(0, 0, 0, 0);

    connect(gMode, SIGNAL(currentIndexChanged(int)), this, SLOT(ModePlayerChanged(int)));
    connect(gPlayers, SIGNAL(currentIndexChanged(int)), this, SLOT(ModePlayerChanged(int)));

    connect(gPlayers, SIGNAL(activated(int)), this, SLOT(EditPlayers(int)));
    connect(savegame, SIGNAL(activated(int)), this, SLOT(EditSave(int)));
    connect(savegame, SIGNAL(onPopup()), this, SLOT(VerbosePopup()));
}

void ZDLMultiPane::EditPlayers(int idx) {
    if (idx == 9) {
        gPlayers->setEditable(true);
        gPlayers->setCurrentIndex(-1);
        gPlayers->setCompleter(nullptr);
        gPlayers->setValidator(players_validator);
    } else {
        gPlayers->setEditable(false);
    }
}

void ZDLMultiPane::VerbosePopup() {
    QString prev_save = savegame->currentText();
    setProperty("prev_save", prev_save);

    QFileInfo fi(prev_save);
    QString save_path;

    if (prev_save.isEmpty())
        save_path = getSaveLastDir();
    else if (fi.isAbsolute() && fi.isFile())
        save_path = fi.absolutePath();

    savegame->setUpdatesEnabled(false);
    savegame->clear();
    savegame->addItem("(None)");
    savegame->addItem("(Browse...)");

    if (save_path.size()) {
        QStringList filter;
        filter << "*.zds" << "*.dsg" << "*.esg";
        QDir save_dir(save_path);
        QFileInfoList saves = save_dir.entryInfoList(filter);

        for (const QFileInfo &sfi: saves) {
            if (sfi.isFile())
                savegame->addItem(sfi.fileName(), sfi.absoluteFilePath());
        }
    }

    int idx;
    if (prev_save.isEmpty()) {
        savegame->setCurrentIndex(0);
        savegame->clearEditText();
    } else if ((idx = savegame->findData(prev_save, Qt::UserRole, Qt::MatchExactly)) > 1) {
        savegame->setCurrentIndex(idx);
        savegame->setEditText(prev_save);
    } else {
        savegame->setCurrentIndex(-1);
        savegame->setEditText(prev_save);
    }
    savegame->setUpdatesEnabled(true);
}

void ZDLMultiPane::EditSave(int idx) {
    savegame->setCurrentIndex(-1);
    if (idx == 1) {
        QString prev_save = property("prev_save").toString();

        QString filters =
                "Savefiles (*.zds" QFD_FILTER_DELIM "*.dsg" QFD_FILTER_DELIM "*.esg);;"
                "All files (" QFD_FILTER_ALL ")";
        QFileInfo fi(prev_save);
        QString save_path = (fi.isRelative() || !fi.isFile()) ? getSaveLastDir() : fi.absolutePath();
        QString new_save = QFileDialog::getOpenFileName(this, "Select savefile", save_path, filters);

        if (new_save.isEmpty()) {
            savegame->setEditText(prev_save);
        } else {
            savegame->setEditText(QFD_QT_SEP(new_save));
            saveSaveLastDir(new_save);
        }
    } else if (idx > 1) {
        savegame->setEditText(savegame->itemData(idx).toString());
    }
}

void ZDLMultiPane::enableAll() {
    gPlayers->setEnabled(true);
    tFragLimit->setEnabled(true);
    extratic->setEnabled(true);
    netmode->setEnabled(true);
    portNo->setEnabled(true);
    dupmode->setEnabled(true);
    bDMFlags2->setEnabled(true);
    bDMFlags->setEnabled(true);
    tHostAddy->setEnabled(true);
    tTimeLimit->setEnabled(true);
    savegame->setEnabled(true);
}

void ZDLMultiPane::disableAll() {
    gPlayers->setEnabled(false);
    tFragLimit->setEnabled(false);
    extratic->setEnabled(false);
    netmode->setEnabled(false);
    portNo->setEnabled(false);
    dupmode->setEnabled(false);
    bDMFlags2->setEnabled(false);
    bDMFlags->setEnabled(false);
    tHostAddy->setEnabled(false);
    tTimeLimit->setEnabled(false);
    savegame->setEnabled(false);
}

void ZDLMultiPane::ModePlayerChanged([[maybe_unused]] int idx) {
    if (gMode->currentIndex()) {
        enableAll();
        if (gPlayers->currentIndex())
            launch_btn->setText("Host");
        else
            launch_btn->setText("Join");
    } else {
        disableAll();
        launch_btn->setText("Launch");
    }
}

namespace {

/** A stored number that isn't a valid non-negative integer shows as blank. */
QString sanitisedNumber(const QString &value) {
    bool ok = false;
    int parsed = value.toInt(&ok, 10);
    return (ok && parsed >= 0) ? value : QString();
}

}

void ZDLMultiPane::newConfig() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (!config) {
        return;
    }
    const ZDLMultiplayerSettings &mp = config->activeProfile().multiplayer;

    tHostAddy->setText(mp.host);

    if (mp.savegame.isEmpty()) {
        savegame->clearEditText();
    } else {
        savegame->setEditText(mp.savegame);
    }

    portNo->setText(sanitisedNumber(mp.port));

    {
        int new_gmode_idx = mp.gameType;
        if (new_gmode_idx < 0 || new_gmode_idx > 3)
            new_gmode_idx = 0;

        if (new_gmode_idx == gMode->currentIndex())
            ModePlayerChanged(new_gmode_idx);
        else
            gMode->setCurrentIndex(new_gmode_idx);
    }
    {
        // Player counts above 8 aren't in the combo, so the box goes editable
        // and holds the raw number instead.
        int new_pl_txt = mp.players;
        int new_pl_idx = mp.players;
        if (new_pl_idx < 0)
            new_pl_idx = 0;
        else if (new_pl_idx > 8)
            new_pl_idx = -1;

        if (new_pl_idx == gPlayers->currentIndex()) {
            ModePlayerChanged(new_pl_idx);
        } else {
            gPlayers->setCurrentIndex(new_pl_idx);
        }

        if (new_pl_idx >= 0) {
            gPlayers->setEditable(false);
        } else {
            gPlayers->setEditable(true);
            gPlayers->setEditText(QString::number(new_pl_txt));
            gPlayers->setCompleter(nullptr);
            gPlayers->setValidator(players_validator);
        }
    }

    extratic->setCurrentIndex((mp.extratic >= 0 && mp.extratic <= 1) ? mp.extratic : 0);
    netmode->setCurrentIndex((mp.netmode >= -1 && mp.netmode <= 1) ? mp.netmode + 1 : 0);
    dupmode->setCurrentIndex((mp.dup >= 0 && mp.dup <= 9) ? mp.dup : 0);

    bDMFlags->setText(sanitisedNumber(mp.dmflags));
    bDMFlags2->setText(sanitisedNumber(mp.dmflags2));
    tFragLimit->setText(sanitisedNumber(mp.fragLimit));
    tTimeLimit->setText(sanitisedNumber(mp.timeLimit));
}

void ZDLMultiPane::rebuild() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (!config) {
        return;
    }
    ZDLMultiplayerSettings &mp = config->activeProfile().multiplayer;

    mp.host = tHostAddy->text();
    mp.savegame = savegame->currentText();
    mp.port = portNo->text();
    mp.fragLimit = tFragLimit->text();
    mp.timeLimit = tTimeLimit->text();
    mp.dmflags = bDMFlags->text();
    mp.dmflags2 = bDMFlags2->text();

    mp.gameType = gMode->currentIndex();
    // An index of -1 means the combo is editable and holds a typed in count.
    mp.players = gPlayers->currentIndex() == -1 ? gPlayers->currentText().toInt() : gPlayers->currentIndex();
    mp.extratic = extratic->currentIndex();
    mp.netmode = netmode->currentIndex() - 1;
    mp.dup = dupmode->currentIndex();
}

void ZDLMultiPane::dmflags() {
    //Per issue #26, remove DMFlag picker
#if 0
    ZDMFlagDialog dialog(this);
    bool ok;
    dialog.setValue(bDMFlags->text().toInt(&ok, 10));
    dialog.setValue2(bDMFlags2->text().toInt(&ok, 10));
    int ret = dialog.exec();
    if (ret == 1){
        bDMFlags->setText(QString::number(dialog.value()));
        bDMFlags2->setText(QString::number(dialog.value2()));
    }
#endif
}

void ZDLMultiPane::dmflags2() {
    dmflags();
}











