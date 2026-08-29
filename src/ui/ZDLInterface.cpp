/*
 * This file is part of qZDL
 * Copyright (C) 2007-2011  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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
#include <QProcessEnvironment>
#include <QMessageBox>
#include <QMenu>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QSignalBlocker>
#include <QToolButton>
#include "config/ZDLConfigurationManager.h"
#include "config/ZDLIniImport.h"
#include "ui/dialogs/ZDLAboutDialog.h"

#include "ui/ZDLMultiPane.h"
#include "ui/ZDLInterface.h"
#include "ui/ZDLMainWindow.h"
#include "ui/ZDLFilePane.h"
#include "ui/ZDLSettingsPane.h"
#include "ui/ZDLQSplitter.h"

#include "gph_dnt.xpm"
#include "gph_upt.xpm"

ZDLInterface::ZDLInterface(QWidget *parent) :
        ZDLWidget(parent),
        box(new QVBoxLayout(this))
        {
    LOGDATAO() << "New ZDLInterface" << Qt::endl;
    ZDLConfigurationManager::setInterface(this);

    QLayout *ppane = getProfilePane();
    QLayout *tpane = getTopPane();
    QLayout *bpane = getBottomPane();

    setContentsMargins(4, 4, 4, 4);
    layout()->setContentsMargins(0, 0, 0, 0);

    box->addLayout(ppane);
    box->addLayout(tpane);
    box->addLayout(bpane);
    box->setSpacing(2);
    LOGDATAO() << "Done creating interface" << Qt::endl;
}

QLayout *ZDLInterface::getProfilePane() {
    auto *boxLayout = new QHBoxLayout();

    boxLayout->addWidget(new QLabel("Profile", this));

    profileCombo = new QComboBox(this);
    profileCombo->setToolTip("Launch configuration to edit and launch");
    profileCombo->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    connect(profileCombo, SIGNAL(activated(int)), this, SLOT(profileSelected(int)));

    auto *btnProfile = new QToolButton(this);
    btnProfile->setText("\u22ef");
    btnProfile->setToolTip("Manage profiles");
    btnProfile->setPopupMode(QToolButton::InstantPopup);

    auto *menu = new QMenu(btnProfile);
    const QAction *newAction = menu->addAction("New profile");
    const QAction *duplicateAction = menu->addAction("Duplicate profile");
    const QAction *renameAction = menu->addAction("Rename profile...");
    menu->addSeparator();
    const QAction *deleteAction = menu->addAction("Delete profile");
    btnProfile->setMenu(menu);

    connect(newAction, SIGNAL(triggered()), this, SLOT(newProfile()));
    connect(duplicateAction, SIGNAL(triggered()), this, SLOT(duplicateProfile()));
    connect(renameAction, SIGNAL(triggered()), this, SLOT(renameProfile()));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteProfile()));

    boxLayout->addWidget(profileCombo, 1);
    boxLayout->addWidget(btnProfile);
    boxLayout->setSpacing(2);
    return boxLayout;
}

void ZDLInterface::refreshProfileCombo() const {
    const ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if ((config == nullptr) || (profileCombo == nullptr)) {
        return;
    }

    // Rebuilding the list must not read back as the user choosing a profile.
    QSignalBlocker const blocker(profileCombo);
    profileCombo->clear();
    for (const ZDLProfile &profile: config->profiles) {
        profileCombo->addItem(profile.name.isEmpty() ? QString("(unnamed)") : profile.name, profile.id);
    }
    profileCombo->setCurrentIndex(config->activeProfileIndex());
}

void ZDLInterface::switchToProfile(const QString &id) {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if ((config == nullptr) || switchingProfile || id.isEmpty() || id == config->activeProfileId) {
        return;
    }

    switchingProfile = true;
    // Flush the widgets into the profile being left, then reload from the new one.
    mw->writeConfig();
    config->setActiveProfile(id);
    mw->startRead();
    switchingProfile = false;
}

void ZDLInterface::profileSelected(const int index) {
    if ((profileCombo == nullptr) || index < 0) {
        return;
    }
    switchToProfile(profileCombo->itemData(index).toString());
}

void ZDLInterface::newProfile() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    switchingProfile = true;
    mw->writeConfig();
    config->setActiveProfile(config->addProfile("New profile"));
    mw->startRead();
    switchingProfile = false;
}

void ZDLInterface::duplicateProfile() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    switchingProfile = true;
    mw->writeConfig();
    config->setActiveProfile(config->duplicateActiveProfile(config->activeProfile().name));
    mw->startRead();
    switchingProfile = false;
}

void ZDLInterface::renameProfile() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    bool ok = false;
    QString const name = QInputDialog::getText(this, "Rename profile", "Profile name:", QLineEdit::Normal,
                                         config->activeProfile().name, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    ZDLProfile &profile = config->activeProfile();
    // uniqueProfileName compares against every profile including this one, so
    // keeping the name unchanged must not turn it into "name (2)".
    if (profile.name.compare(name.trimmed(), Qt::CaseInsensitive) != 0) {
        profile.name = config->uniqueProfileName(name);
    } else {
        profile.name = name.trimmed();
    }
    refreshProfileCombo();
}

void ZDLInterface::deleteProfile() {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    QString const text = QString("Delete the profile \"%1\"?").arg(config->activeProfile().name);
    if (QMessageBox::warning(this, "ZDL", text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }

    switchingProfile = true;
    config->removeProfile(config->activeProfileId);
    mw->startRead();
    switchingProfile = false;
}

void ZDLInterface::onIwadSelected(const QString &iwadName) {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if ((config == nullptr) || switchingProfile) {
        return;
    }

    if (iwadName.isEmpty()) {
        // The selection was cleared, unbinding the profile.  Record that now so
        // that picking a game next is treated as a fresh binding rather than as
        // a request to navigate somewhere else.
        mw->writeConfig();
        return;
    }

    // Picking a game is also how a profile gets bound to one, so only navigate
    // away when this profile is already committed to a different game.  A
    // profile with no game yet, or one being pointed at a game that no other
    // profile owns, keeps the selection and binds to it.
    const ZDLProfile &active = config->activeProfile();
    if (active.iwad.isEmpty() || active.iwad == iwadName) {
        return;
    }

    QString const targetId = config->profileForIwad(iwadName);
    if (targetId.isEmpty() || targetId == config->activeProfileId) {
        return;
    }

    // The click asked to switch games, not to rebind the profile being left, so
    // its own game goes back after the widgets are flushed.
    QString const outgoingId = config->activeProfileId;
    QString const outgoingIwad = active.iwad;

    switchingProfile = true;
    mw->writeConfig();
    int const outgoing = config->indexOfProfile(outgoingId);
    if (outgoing >= 0) {
        config->profiles[outgoing].iwad = outgoingIwad;
    }
    config->setActiveProfile(targetId);
    // Flushing the widgets recorded this game against the outgoing profile;
    // the profile actually being switched to is the right answer.
    config->general.lastProfileByIwad[iwadName] = targetId;
    mw->startRead();
    switchingProfile = false;
}

QLayout *ZDLInterface::getTopPane() {
    auto *boxLayout = new QHBoxLayout();

    auto *split = new ZDLQSplitter(this);
    split->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    QSplitter *rsplit = split->getSplit();

    auto *fpane = new ZDLFilePane(rsplit);
    auto *spane = new ZDLSettingsPane(rsplit);

    connect(spane, SIGNAL(iwadSelected(QString)), this, SLOT(onIwadSelected(QString)));

    split->addChild(fpane);
    split->addChild(spane);
    boxLayout->setSpacing(2);
    boxLayout->addWidget(rsplit);
    return boxLayout;
}

QLayout *ZDLInterface::getBottomPane() {
    auto *pBoxLayout = new QVBoxLayout();
    auto *box_inside_box = new QVBoxLayout();
    auto *ecla = new QLabel("Extra command line arguments", this);
    ecla->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed));
    extraArgs = new QLineEdit(this);
    QLayout *pan = getButtonPane();
    layout()->setContentsMargins(0, 0, 0, 0);
    setContentsMargins(0, 0, 0, 0);
    box_inside_box->addWidget(ecla);
    box_inside_box->addWidget(extraArgs);
    box_inside_box->setSpacing(2);
    pBoxLayout->addLayout(box_inside_box);
    pBoxLayout->addLayout(pan);
    pBoxLayout->setSpacing(4);
    return pBoxLayout;
}

void ZDLInterface::exitzdl() {
    LOGDATAO() << "Closing ZDL" << Qt::endl;
    mw->close();
}

void ZDLInterface::importCurrentConfig() {
    LOGDATAO() << "Asking if they'd really like to import" << Qt::endl;
    QString const text(
            "Are you sure you'd like to <b>replace</b> the current <b>global</b> configurationw with the one currently loaded?");
    QMessageBox::StandardButton const btnrc = QMessageBox::warning(this, "ZDL", text,
                                                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (btnrc != QMessageBox::Yes) {
        LOGDATAO() << "They said no, bailing" << Qt::endl;
        return;
    }
    const ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
    const ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }
    QString const userConfPath = conf->getPath(ZDLConfiguration::CONF_USER);
    QFileInfo const userConf(userConfPath);
    if (!userConf.exists()) {
        LOGDATAO() << "File " << userConfPath << " doesn't exist" << Qt::endl;
        QDir const dir = userConf.dir();
        if (!dir.exists()) {
            LOGDATAO() << "Nor does it's path" << Qt::endl;
            if (!dir.mkpath(dir.absolutePath())) {
                LOGDATAO() << "Couldn't create the path, bailing" << Qt::endl;
                QMessageBox::critical(this, "ZDL",
                                      QString("Unable to create the directories for a global user configuration at ")
                                      + dir.absolutePath());
                return;
            }
        }
    }
    LOGDATAO() << "Triggering write" << Qt::endl;
    mw->writeConfig();
    QString error;
    if (!config->save(userConfPath, &error)) {
        LOGDATAO() << "Couldn't write to configuration file " << userConfPath << Qt::endl;
        QMessageBox::critical(this, "ZDL",
                              QString("Unable to write the configuration file at %1:\n%2").arg(userConfPath, error));
        return;
    }
    ZDLConfigurationManager::setConfigFileName(userConfPath);
    LOGDATAO() << "Triggering read" << Qt::endl;
    mw->startRead();
}

QLayout *ZDLInterface::getButtonPane() {
    auto *boxLayout = new QHBoxLayout();

    auto *btnExit = new QPushButton("Exit", this);
    btnZDL = new QPushButton("ZDL", this);
    btnEpr = new QPushButton(this);
    btnLaunch = new QPushButton("Launch", this);

    auto *context = new QMenu(btnZDL);
    auto *actions = new QMenu("Actions", context);

    const QAction *showCommandline = actions->addAction("Show command line");
    const QAction *clearAllPWadsAction = actions->addAction("Clear external file list");
    QAction *clearAllFieldsAction = actions->addAction("Clear all fields");
    clearAllFieldsAction->setShortcut(QKeySequence::New);
    const QAction *clearEverythingAction = actions->addAction("Clear everything");
    actions->addSeparator();
#ifndef NO_IMPORT
    const QAction *actImportCurrentConfig = actions->addAction("Import current config");
#endif
    QAction *clearCurrentGlobalConfig = actions->addAction("Clear current global config");
    clearCurrentGlobalConfig->setEnabled(false);

    context->addMenu(actions);
    context->addSeparator();
    QAction *loadZdlFileAction = context->addAction("Load .zdl");
    loadZdlFileAction->setShortcut(QKeySequence::Open);
    QAction *saveZdlFileAction = context->addAction("Save .zdl");
    saveZdlFileAction->setShortcut(QKeySequence::Save);
    context->addSeparator();
    const QAction *loadAction = context->addAction("Load .json");
    const QAction *saveAction = context->addAction("Save .json");
    const QAction *importIniAction = context->addAction("Import legacy zdl.ini...");
    context->addSeparator();
    QAction *aboutAction = context->addAction("About");
    aboutAction->setShortcut(QKeySequence::HelpContents);

    connect(loadAction, SIGNAL(triggered()), this, SLOT(loadConfigFile()));
    connect(saveAction, SIGNAL(triggered()), this, SLOT(saveConfigFile()));
    connect(loadZdlFileAction, SIGNAL(triggered()), this, SLOT(loadZdlFile()));
    connect(saveZdlFileAction, SIGNAL(triggered()), this, SLOT(saveZdlFile()));
    connect(importIniAction, SIGNAL(triggered()), this, SLOT(importLegacyIni()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClick()));
#ifndef NO_IMPORT
    connect(actImportCurrentConfig, SIGNAL(triggered()), this, SLOT(importCurrentConfig()));
#endif

    connect(clearAllPWadsAction, SIGNAL(triggered()), this, SLOT(clearAllPWads()));
    connect(clearAllFieldsAction, SIGNAL(triggered()), this, SLOT(clearAllFields()));
    connect(clearEverythingAction, SIGNAL(triggered()), this, SLOT(clearEverything()));

    connect(showCommandline, SIGNAL(triggered()), this, SLOT(showCommandline()));
    //connect(newDMFlagger, SIGNAL(triggered()),this,SLOT(showNewDMFlagger()));
    connect(btnExit, SIGNAL(clicked()), this, SLOT(exitzdl()));

    btnZDL->setMenu(context);

    constexpr int minBtnWidth = 50;

    btnExit->setMinimumWidth(minBtnWidth - 15);
    btnZDL->setMinimumWidth(minBtnWidth - 15);
    btnEpr->setMinimumWidth(20);
    btnLaunch->setMinimumWidth(minBtnWidth);

    btnExit->setMinimumHeight(26);
    btnZDL->setMinimumHeight(26);
    btnLaunch->setMinimumHeight(26);
    //26 is a cherry-picked value for Win32 system font
    //On other OSes we can have fonts with greater sizes so text buttons will be taller than 26 but glyph button will remain the same
    //To accomodate this, we set minimum height for glyph button dynamically
    btnEpr->setMinimumHeight(btnLaunch->sizeHint().height());

    connect(btnLaunch, SIGNAL(clicked()), this, SLOT(launch()));

    setContentsMargins(0, 0, 0, 0);
    layout()->setContentsMargins(0, 0, 0, 0);
    boxLayout->addWidget(btnExit, 1);
    boxLayout->addWidget(btnZDL, 1);
    boxLayout->addStretch(2);
    boxLayout->addWidget(btnEpr, 1);
    boxLayout->addWidget(btnLaunch, 1);
    boxLayout->setSpacing(4);
    connect(btnEpr, SIGNAL(clicked()), this, SLOT(mclick()));
    return boxLayout;
}

void ZDLInterface::clearAllPWads() {
    LOGDATAO() << "Clearing all PWads" << Qt::endl;
    mw->writeConfig();
    if (ZDLConfigModel *config = ZDLConfigurationManager::getConfig()) {
        config->activeProfile().files.clear();
    }
    mw->startRead();
}

void ZDLInterface::clearEverything() {
    LOGDATAO() << "Clearing everything question" << Qt::endl;
    QString const text(
            "Warning!\n\nIf you proceed, you will lose <b>EVERYTHING</b>!\n All IWAD, PWAD, and source port settings will be wiped.\n\nWould you like to continue?");
    QMessageBox::StandardButton const btnrc = QMessageBox::warning(this, "ZDL", text,
                                                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (btnrc != QMessageBox::Yes) {
        return;
    }
    LOGDATAO() << "They said yes, clearing..." << Qt::endl;
    if (ZDLConfigModel *config = ZDLConfigurationManager::getConfig()) {
        config->clear();
    }
    LOGDATAO() << "Clearing done" << Qt::endl;
    mw->startRead();

}

void ZDLInterface::clearAllFields() {
    LOGDATAO() << "Clearing the active profile" << Qt::endl;
    mw->writeConfig();
    if (ZDLConfigModel *config = ZDLConfigurationManager::getConfig()) {
        // The profile itself stays; only what it launches is wiped.
        config->activeProfile().clearSettings();
    }
    mw->startRead();
    LOGDATAO() << "Complete" << Qt::endl;
}

void ZDLInterface::launch() {
    LOGDATAO() << "Launching" << Qt::endl;
    mw->launch();
}

void ZDLInterface::buttonPaneNewConfig() const {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    bool const open = (config != nullptr) && config->activeProfile().dialogOpen;
    btnEpr->setIcon(QPixmap(open ? glyph_down_trg : glyph_up_trg));
}

void ZDLInterface::mclick() {
    writeConfig();
    if (ZDLConfigModel *config = ZDLConfigurationManager::getConfig()) {
        ZDLProfile &profile = config->activeProfile();
        profile.dialogOpen = !profile.dialogOpen;
    }
    // startRead() runs buttonPaneNewConfig(), which sets the arrow to match.
    startRead();
}

void ZDLInterface::sendSignals() {
    rebuild();
    emit buildParent(this);
    emit buildChildren(this);
}

void ZDLInterface::saveConfigFile() {
    LOGDATAO() << "Saving config file" << Qt::endl;
    sendSignals();
    const ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }
    QString const filters =
            "JSON files (*.json);;"
            "All files (" QFD_FILTER_ALL ")";

    QString fileName = QFileDialog::getSaveFileName(this, "Save configuration", getConfigLastDir(), filters);
    if (fileName.isNull() || fileName.isEmpty()) {
        return;
    }

    QFileInfo const fi(fileName);
    if (!fi.fileName().contains(".")) {
        fileName += ".json";
    }
    ZDLConfigurationManager::setConfigFileName(fileName);
    saveConfigLastDir(fileName);

    QString error;
    if (!config->save(fileName, &error)) {
        QMessageBox::critical(this, "ZDL", QString("Unable to save %1:\n%2").arg(fileName, error));
        return;
    }
    mw->startRead();
}

void ZDLInterface::loadConfigFile() {
    LOGDATAO() << "Loading config file" << Qt::endl;
    QString const filters =
            "JSON files (*.json);;"
            "All files (" QFD_FILTER_ALL ")";

    QString const fileName = QFileDialog::getOpenFileName(this, "Load configuration", getConfigLastDir(), filters);
    if (fileName.isNull() || fileName.isEmpty()) {
        return;
    }

    auto *model = new ZDLConfigModel();
    QString error;
    if (!model->load(fileName, &error)) {
        QMessageBox::critical(this, "ZDL", QString("Unable to load %1:\n%2").arg(fileName, error));
        delete model;
        return;
    }

    const ZDLConfigModel *previous = ZDLConfigurationManager::getConfig();
    ZDLConfigurationManager::setConfigFileName(fileName);
    ZDLConfigurationManager::setConfig(model);
    delete previous;

    // Recorded against the config that is now active, not the one just freed.
    saveConfigLastDir(fileName);
    mw->startRead();
}

void ZDLInterface::importLegacyIni() {
    LOGDATAO() << "Importing legacy INI" << Qt::endl;
    QString const filters =
            "INI files (*.ini);;"
            "All files (" QFD_FILTER_ALL ")";

    QString const fileName = QFileDialog::getOpenFileName(this, "Import legacy configuration",
                                                          getConfigLastDir(), filters);
    if (fileName.isNull() || fileName.isEmpty()) {
        return;
    }

    auto *model = new ZDLConfigModel();
    if (!ZDLIniImport::loadLegacyFile(fileName, *model)) {
        QMessageBox::critical(this, "ZDL", QString("Unable to import %1").arg(fileName));
        delete model;
        return;
    }

    // The imported settings replace what's loaded, but keep saving to the
    // current config file rather than back to the .ini.
    const ZDLConfigModel *previous = ZDLConfigurationManager::getConfig();
    ZDLConfigurationManager::setConfig(model);
    delete previous;

    saveConfigLastDir(fileName);
    mw->startRead();
}

void ZDLInterface::loadZdlFile() {
    LOGDATAO() << "Loading ZDL file" << Qt::endl;
    QString const filters =
            "ZDL files (*.zdl);;"
            "All files (" QFD_FILTER_ALL ")";

    QString const fileName = QFileDialog::getOpenFileName(this, "Load ZDL", getZdlLastDir(), filters);
    if (fileName.isNull() || fileName.isEmpty()) {
        return;
    }

    ZDLProfile profile;
    if (!ZDLIniImport::loadZdlFile(fileName, profile)) {
        QMessageBox::critical(this, "ZDL", QString("%1 doesn't contain a launch configuration").arg(fileName));
        return;
    }

    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }

    // Added as a new profile rather than overwriting the current one.
    switchingProfile = true;
    mw->writeConfig();
    profile.name = config->uniqueProfileName(profile.name);
    config->profiles.append(profile);
    config->setActiveProfile(profile.id);
    saveZdlLastDir(fileName);
    mw->startRead();
    switchingProfile = false;
}

void ZDLInterface::saveZdlFile() {
    LOGDATAO() << "Saving ZDL File" << Qt::endl;
    sendSignals();
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }
    QString const filters =
            "ZDL files (*.zdl);;"
            "All files (" QFD_FILTER_ALL ")";

    QString fileName = QFileDialog::getSaveFileName(this, "Save ZDL", getZdlLastDir(), filters);
    if (fileName.isNull() || fileName.isEmpty()) {
        return;
    }

    QFileInfo const fi(fileName);
    if (!fi.fileName().contains(".")) {
        fileName += ".zdl";
    }
    saveZdlLastDir(fileName);

    // Written in the classic INI format so other Doom tools can still read it.
    if (!ZDLIniImport::saveZdlFile(fileName, config->activeProfile())) {
        QMessageBox::critical(this, "ZDL", QString("Unable to save %1").arg(fileName));
    }
}

void ZDLInterface::aboutClick() {
    LOGDATAO() << "Opening About dialog" << Qt::endl;
    ZDLAboutDialog zad(this);
    zad.exec();
}

void ZDLInterface::showCommandline() {
    LOGDATAO() << "Showing command line" << Qt::endl;
    writeConfig();

    QString const exec = ZDLMainWindow::getExecutable();

    if (exec.isEmpty()) {
        QMessageBox::critical(this, "ZDL", "Please select a source port");
        return;
    }

    QFileInfo const exec_fi(exec);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Command line and environment");
    QString dwd;
    QString args = ZDLMainWindow::getArgumentsString();
    if (args.length() != 0) {
        args = "\n\nArguments: " + args;
    }
    if (QProcessEnvironment::systemEnvironment().contains("DOOMWADDIR")) {
        dwd = "\n\nDOOMWADDIR: "
              + QDir::fromNativeSeparators(QProcessEnvironment::systemEnvironment().value("DOOMWADDIR"));
    }
    msgBox.setText(
            "Executable: " + exec_fi.fileName() + args + "\n\nWorking directory: " + exec_fi.canonicalPath() + dwd);
    msgBox.setStandardButtons(QMessageBox::Cancel);
    QPushButton *launch_btn = msgBox.addButton("Execute", QMessageBox::AcceptRole);
    msgBox.setDefaultButton(launch_btn);
    msgBox.setEscapeButton(QMessageBox::Cancel);
    msgBox.exec();

    if (msgBox.clickedButton() == launch_btn) {
        LOGDATAO() << "Asked to launch" << Qt::endl;
        mw->launch();
        return;
    }
    LOGDATAO() << "Done" << Qt::endl;
}

void ZDLInterface::rebuild() {
    LOGDATAO() << "Saving config" << Qt::endl;
    if (ZDLConfigModel *config = ZDLConfigurationManager::getConfig()) {
        config->activeProfile().extra = extraArgs->text();
    }
}

void ZDLInterface::bottomPaneNewConfig() const {
    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    extraArgs->setText((config != nullptr) ? config->activeProfile().extra : QString());
}

//Called when there's a change to the configuration that we need to look at.
//The button changed the configuration, and then notifies us that we need
//to look at the configuration to see what we need to do.
void ZDLInterface::newConfig() {
    LOGDATAO() << "Loading new config" << Qt::endl;
    refreshProfileCombo();
    buttonPaneNewConfig();
    bottomPaneNewConfig();

    ZDLConfigModel *config = ZDLConfigurationManager::getConfig();
    if (config == nullptr) {
        return;
    }
    const ZDLProfile &profile = config->activeProfile();

    if (profile.dialogOpen) {
        if (mpane == nullptr) {
            mpane = new ZDLMultiPane(this);
            mpane->setLaunchButton(btnLaunch);
            mpane->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum));
            mpane->newConfig();
        }
        box->addWidget(mpane);
        mpane->setVisible(true);
    } else {
        if (mpane != nullptr) {
            mpane->setVisible(false);
            box->removeWidget(mpane);
        }
        if (profile.multiplayer.gameType != 0) {
            btnLaunch->setText((profile.multiplayer.players != 0) ? "Host" : "Join");
        } else {
            btnLaunch->setText("Launch");
        }
    }
    this->update();
}

void ZDLInterface::startRead() {
    emit readChildren(this);
    newConfig();
}

void ZDLInterface::writeConfig() {
    rebuild();
    emit buildChildren(this);
}
