/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "core/Import.h"
#include "core/Launcher.h"
#include "core/Paths.h"
#include "core/Session.h"
#include "core/Text.h"
#include "gui/ConfigBridge.h"

namespace {

Config &config() {
    return Session::get().config();
}

Profile &profile() {
    return config().activeProfile();
}

MultiplayerSettings &multiplayer() {
    return profile().multiplayer;
}

QString text(const std::string &value) {
    return QString::fromStdString(value);
}

}

ConfigBridge::ConfigBridge(Notifier *notifier, QObject *parent)
    : QObject(parent),
      _notifier(notifier),
      _files(new FileList(this)),
      _iwads(new NameList(NameList::Kind::Iwads, this)),
      _ports(new NameList(NameList::Kind::Ports, this)) {
    // What is loaded decides which maps can be warped to and what the command
    // line comes out as, so the panel showing those is told when it changes.
    connect(_files, &FileList::changed, this, &ConfigBridge::touch);

    /*
    An entry renamed in Settings is still the same IWAD or port, and profiles
    name them by the name, so every profile pointing at the old one is carried
    across rather than being quietly emptied.
    */
    connect(_iwads, &NameList::renamed, this, [this](const QString &before, const QString &after) {
        for (Profile &each : config().profiles) {
            if (each.iwad == before.toStdString()) {
                each.iwad = after.toStdString();
            }
        }

        emit profileChanged();
        touch();
    });

    connect(_ports, &NameList::renamed, this, [this](const QString &before, const QString &after) {
        for (Profile &each : config().profiles) {
            if (each.port == before.toStdString()) {
                each.port = after.toStdString();
            }
        }

        emit profileChanged();
        touch();
    });

    connect(_iwads, &NameList::changed, this, &ConfigBridge::touch);
    connect(_ports, &NameList::changed, this, &ConfigBridge::commandLineChanged);
}

void ConfigBridge::touch() {
    _mapsKnown = false;

    emit mapsChanged();
    emit commandLineChanged();
}

FileList *ConfigBridge::files() const { return _files; }
NameList *ConfigBridge::iwads() const { return _iwads; }
NameList *ConfigBridge::ports() const { return _ports; }

QStringList ConfigBridge::profileNames() const {
    QStringList names;

    for (const Profile &each : config().profiles) {
        names << (each.name.empty() ? QStringLiteral("(unnamed)") : text(each.name));
    }

    return names;
}

int ConfigBridge::profileIndex() const { return config().activeProfileIndex(); }

QString ConfigBridge::profileName() const { return text(profile().name); }

QString ConfigBridge::iwad() const { return text(profile().iwad); }
QString ConfigBridge::port() const { return text(profile().port); }
int ConfigBridge::skill() const { return profile().skill; }
int ConfigBridge::monsters() const { return profile().monsters; }
QString ConfigBridge::warp() const { return text(profile().warp); }
QString ConfigBridge::extra() const { return text(profile().extra); }
bool ConfigBridge::multiplayerOpen() const { return profile().dialogOpen; }
bool ConfigBridge::sharedConfig() const { return profile().sharedConfig; }

QString ConfigBridge::configFile() const {
    return text(Launcher::getConfigPath(profile()).string());
}

int ConfigBridge::gameType() const { return multiplayer().gameType; }
int ConfigBridge::players() const { return multiplayer().players; }
QString ConfigBridge::host() const { return text(multiplayer().host); }
QString ConfigBridge::netPort() const { return text(multiplayer().port); }
QString ConfigBridge::fragLimit() const { return text(multiplayer().fragLimit); }
QString ConfigBridge::timeLimit() const { return text(multiplayer().timeLimit); }
QString ConfigBridge::dmflags() const { return text(multiplayer().dmflags); }
QString ConfigBridge::dmflags2() const { return text(multiplayer().dmflags2); }
int ConfigBridge::extratic() const { return multiplayer().extratic; }
int ConfigBridge::netmode() const { return multiplayer().netmode; }
int ConfigBridge::dup() const { return multiplayer().dup; }
QString ConfigBridge::savegame() const { return text(multiplayer().savegame); }

QString ConfigBridge::alwaysAdd() const { return text(config().general.alwaysAdd); }
bool ConfigBridge::autoClose() const { return config().general.autoClose; }
bool ConfigBridge::launchZdlImmediately() const { return config().general.launchZdlImmediately; }
bool ConfigBridge::rememberFileList() const { return config().general.rememberFileList; }
bool ConfigBridge::showPaths() const { return config().general.showPaths; }
bool ConfigBridge::profileConfigs() const { return config().general.profileConfigs; }

QString ConfigBridge::path() const { return text(Session::get().path().string()); }

bool ConfigBridge::userConfig() const {
    return Session::get().path() == Paths::get().configPath(Paths::USER);
}

QStringList ConfigBridge::maps() const {
    if (!_mapsKnown) {
        _maps.clear();

        for (const std::string &name : Launcher::maps(config())) {
            _maps << text(name);
        }

        _mapsKnown = true;
    }

    return _maps;
}

QString ConfigBridge::commandLine() const {
    return text(Launcher::commandLine(config()));
}

void ConfigBridge::setProfileIndex(const int index) {
    const std::vector<Profile> &profiles = config().profiles;

    if (index < 0 || std::cmp_greater_equal(index, profiles.size())
        || profiles[static_cast<size_t>(index)].id == config().activeProfileId) {
        return;
    }

    config().setActiveProfile(profiles[static_cast<size_t>(index)].id);
    reload();
}

void ConfigBridge::setIwad(const QString &value) {
    if (value.toStdString() == profile().iwad) {
        return;
    }

    profile().iwad = value.toStdString();

    emit profileChanged();

    // A different IWAD is a different set of maps to warp to.
    touch();
}

void ConfigBridge::setPort(const QString &value) {
    if (value.toStdString() == profile().port) {
        return;
    }

    profile().port = value.toStdString();

    emit profileChanged();
    emit commandLineChanged();
}

void ConfigBridge::setSkill(const int value) {
    if (value == profile().skill) {
        return;
    }

    profile().skill = value;

    emit profileChanged();
    emit commandLineChanged();
}

void ConfigBridge::setMonsters(const int value) {
    if (value == profile().monsters) {
        return;
    }

    profile().monsters = value;

    emit profileChanged();
    emit commandLineChanged();
}

void ConfigBridge::setWarp(const QString &value) {
    if (value.toStdString() == profile().warp) {
        return;
    }

    profile().warp = value.toStdString();

    emit profileChanged();
    emit commandLineChanged();
}

void ConfigBridge::setExtra(const QString &value) {
    if (value.toStdString() == profile().extra) {
        return;
    }

    profile().extra = value.toStdString();

    emit profileChanged();
    emit commandLineChanged();
}

void ConfigBridge::setMultiplayerOpen(const bool value) {
    if (value == profile().dialogOpen) {
        return;
    }

    profile().dialogOpen = value;

    emit profileChanged();
}

void ConfigBridge::setSharedConfig(const bool value) {
    if (value == profile().sharedConfig) {
        return;
    }

    profile().sharedConfig = value;

    emit profileChanged();
    emit commandLineChanged();
}

/*
Every one of these is the same three lines, and the macro says so once rather
than eighteen times. What is worth reading about a multiplayer field is its
name, and that is all the expansion leaves.
*/
#define MULTIPLAYER_SETTER(Setter, Field, Type, Convert)      \
    void ConfigBridge::Setter(Type value) {                   \
        if ((Convert) == multiplayer().Field) {               \
            return;                                           \
        }                                                     \
                                                              \
        multiplayer().Field = (Convert);                      \
                                                              \
        emit multiplayerChanged();                            \
        emit commandLineChanged();                            \
    }

MULTIPLAYER_SETTER(setGameType, gameType, int, value)
MULTIPLAYER_SETTER(setPlayers, players, int, value)
MULTIPLAYER_SETTER(setExtratic, extratic, int, value)
MULTIPLAYER_SETTER(setNetmode, netmode, int, value)
MULTIPLAYER_SETTER(setDup, dup, int, value)
MULTIPLAYER_SETTER(setHost, host, const QString &, value.toStdString())
MULTIPLAYER_SETTER(setNetPort, port, const QString &, value.toStdString())
MULTIPLAYER_SETTER(setFragLimit, fragLimit, const QString &, value.toStdString())
MULTIPLAYER_SETTER(setTimeLimit, timeLimit, const QString &, value.toStdString())
MULTIPLAYER_SETTER(setDmflags, dmflags, const QString &, value.toStdString())
MULTIPLAYER_SETTER(setDmflags2, dmflags2, const QString &, value.toStdString())
MULTIPLAYER_SETTER(setSavegame, savegame, const QString &, value.toStdString())

#undef MULTIPLAYER_SETTER

void ConfigBridge::setAlwaysAdd(const QString &value) {
    if (value.toStdString() == config().general.alwaysAdd) {
        return;
    }

    config().general.alwaysAdd = value.toStdString();

    emit generalChanged();
    emit commandLineChanged();
}

void ConfigBridge::setAutoClose(const bool value) {
    if (value == config().general.autoClose) {
        return;
    }

    config().general.autoClose = value;

    emit generalChanged();
}

void ConfigBridge::setLaunchZdlImmediately(const bool value) {
    if (value == config().general.launchZdlImmediately) {
        return;
    }

    config().general.launchZdlImmediately = value;

    emit generalChanged();
}

void ConfigBridge::setRememberFileList(const bool value) {
    if (value == config().general.rememberFileList) {
        return;
    }

    config().general.rememberFileList = value;

    emit generalChanged();
}

void ConfigBridge::setShowPaths(const bool value) {
    if (value == config().general.showPaths) {
        return;
    }

    config().general.showPaths = value;

    emit generalChanged();
}

void ConfigBridge::setProfileConfigs(const bool value) {
    if (value == config().general.profileConfigs) {
        return;
    }

    config().general.profileConfigs = value;

    emit generalChanged();

    // Which config the port is pointed at is part of every profile's command
    // line, and the launch page shows the profile's own alongside it.
    emit profileChanged();
    emit commandLineChanged();
}

void ConfigBridge::reload() {
    _files->reload();
    _iwads->reload();
    _ports->reload();

    emit profilesChanged();
    emit profileChanged();
    emit multiplayerChanged();
    emit generalChanged();
    emit pathChanged();
    touch();
}

void ConfigBridge::addProfile(const QString &name) {
    config().setActiveProfile(config().addProfile(name.toStdString()));
    reload();
}

void ConfigBridge::duplicateProfile() {
    config().setActiveProfile(config().duplicateActiveProfile(profile().name));
    reload();
}

void ConfigBridge::renameProfile(const QString &name) {
    const std::string wanted = name.toStdString();

    if (wanted.empty()) {
        return;
    }

    /*
    uniqueProfileName compares against every profile including this one, so a
    name left as it was must not turn into "name (2)".
    */
    Profile &active = profile();

    active.name = Text::iequals(active.name, wanted)
        ? Text::trim(wanted)
        : config().uniqueProfileName(wanted);

    emit profilesChanged();
    emit profileChanged();
}

void ConfigBridge::removeProfile() {
    config().removeProfile(config().activeProfileId);
    reload();
}

void ConfigBridge::clearFiles() {
    _files->clear();
}

void ConfigBridge::clearProfile() {
    // The profile itself stays; only what it launches is wiped.
    profile().clearSettings();
    reload();
}

void ConfigBridge::clearEverything() {
    config().clear();
    reload();
}

bool ConfigBridge::save() {
    std::string error;

    if (!Session::get().save(&error)) {
        _notifier->error("Could not save the config: " + text(error));

        return false;
    }

    return true;
}

bool ConfigBridge::saveAs(const QString &path) {
    std::string error;

    if (!Session::get().saveAs(path.toStdString(), &error)) {
        _notifier->error("Could not save to " + path + ": " + text(error));

        return false;
    }

    emit pathChanged();
    _notifier->success("Saved to " + path + ".");

    return true;
}

bool ConfigBridge::load(const QString &path) {
    std::string error;

    if (!Session::get().load(path.toStdString(), &error)) {
        _notifier->error("Could not read " + path + ": " + text(error));

        return false;
    }

    reload();
    _notifier->success("Loaded " + path + ".");

    return true;
}

bool ConfigBridge::adoptAsUserConfig() {
    std::string error;

    if (!Session::get().adoptAsUserConfig(&error)) {
        _notifier->error("Could not write the user config: " + text(error));

        return false;
    }

    emit pathChanged();
    _notifier->success("This config is now the one ZDL opens by default.");

    return true;
}

bool ConfigBridge::loadZdl(const QString &path) {
    Profile loaded;

    if (!Import::loadZdlFile(path.toStdString(), loaded)) {
        _notifier->error("Could not read " + path + " as a .zdl file.");

        return false;
    }

    loaded.name = config().uniqueProfileName(loaded.name);
    config().profiles.push_back(loaded);

    // A .zdl is a launch config other Doom tools write too, and carries no
    // config file name for the profile it becomes.
    config().ensureConfigFiles();
    config().setActiveProfile(loaded.id);
    reload();

    _notifier->success("Added " + text(loaded.name) + " from " + path + ".");

    return true;
}

bool ConfigBridge::saveZdl(const QString &path) {
    if (!Import::saveZdlFile(path.toStdString(), profile())) {
        _notifier->error("Could not write " + path + ".");

        return false;
    }

    _notifier->success("Saved " + profileName() + " to " + path + ".");

    return true;
}

bool ConfigBridge::launch() {
    std::string error;

    if (!Launcher::launch(config(), &error)) {
        _notifier->error(text(error), QStringLiteral("Nothing was launched"));

        return false;
    }

    emit launched();

    return true;
}
