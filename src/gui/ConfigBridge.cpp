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
#include "gui/PathText.h"

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

ConfigBridge::ConfigBridge(Notifier *notifier, Runs *runs, QObject *parent)
    : QObject(parent),
      _notifier(notifier),
      _runs(runs),
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

    emit profilesChanged();
}

FileList *ConfigBridge::files() const { return _files; }
NameList *ConfigBridge::iwads() const { return _iwads; }
NameList *ConfigBridge::ports() const { return _ports; }

QStringList ConfigBridge::profileNames() {
    QStringList names;

    for (const Profile &each : config().profiles) {
        names << (each.name.empty() ? QStringLiteral("(unnamed)") : text(each.name));
    }

    return names;
}

int ConfigBridge::profileIndex() { return config().activeProfileIndex(); }

QString ConfigBridge::profileName() { return text(profile().name); }

QString ConfigBridge::profileKey() {
    return QStringLiteral("profile:") + text(config().activeProfileId);
}

QVariantList ConfigBridge::profileCards() {
    QVariantList cards;
    const Config &current = config();

    for (size_t index = 0; index < current.profiles.size(); ++index) {
        const Profile &each = current.profiles[index];
        int loaded = 0;

        for (const FileEntry &file : each.files) {
            if (file.enabled) {
                ++loaded;
            }
        }

        cards.append(QVariantMap{
            {QStringLiteral("index"), static_cast<int>(index)},
            {QStringLiteral("id"), text(each.id)},

            {QStringLiteral("key"), QStringLiteral("profile:") + text(each.id)},
            {QStringLiteral("name"), each.name.empty() ? QStringLiteral("(unnamed)") : text(each.name)},
            {QStringLiteral("iwad"), text(each.iwad)},
            {QStringLiteral("port"), text(each.port)},
            {QStringLiteral("warp"), text(each.warp)},
            {QStringLiteral("files"), static_cast<int>(each.files.size())},
            {QStringLiteral("loaded"), loaded},
            {QStringLiteral("multiplayer"), each.multiplayer.gameType != 0},

            {QStringLiteral("ready"), !each.port.empty()},
        });
    }

    return cards;
}

QString ConfigBridge::iwad() { return text(profile().iwad); }
QString ConfigBridge::port() { return text(profile().port); }
int ConfigBridge::skill() { return profile().skill; }
int ConfigBridge::monsters() { return profile().monsters; }
QString ConfigBridge::warp() { return text(profile().warp); }
QString ConfigBridge::extra() { return text(profile().extra); }
bool ConfigBridge::multiplayerOpen() { return profile().dialogOpen; }
bool ConfigBridge::sharedConfig() { return profile().sharedConfig; }

QString ConfigBridge::configFile() {
    return PathText::fromPath(Launcher::getConfigPath(profile()));
}

int ConfigBridge::gameType() { return multiplayer().gameType; }
int ConfigBridge::players() { return multiplayer().players; }
QString ConfigBridge::host() { return text(multiplayer().host); }
QString ConfigBridge::netPort() { return text(multiplayer().port); }
QString ConfigBridge::fragLimit() { return text(multiplayer().fragLimit); }
QString ConfigBridge::timeLimit() { return text(multiplayer().timeLimit); }
QString ConfigBridge::dmflags() { return text(multiplayer().dmflags); }
QString ConfigBridge::dmflags2() { return text(multiplayer().dmflags2); }
int ConfigBridge::extratic() { return multiplayer().extratic; }
int ConfigBridge::netmode() { return multiplayer().netmode; }
int ConfigBridge::dup() { return multiplayer().dup; }
QString ConfigBridge::savegame() { return text(multiplayer().savegame); }

QString ConfigBridge::alwaysAdd() { return text(config().general.alwaysAdd); }
bool ConfigBridge::autoClose() { return config().general.autoClose; }
bool ConfigBridge::launchZdlImmediately() { return config().general.launchZdlImmediately; }
bool ConfigBridge::rememberFileList() { return config().general.rememberFileList; }
bool ConfigBridge::showPaths() { return config().general.showPaths; }
bool ConfigBridge::captureOutput() { return profile().captureOutput; }
bool ConfigBridge::profileConfigs() { return config().general.profileConfigs; }

QString ConfigBridge::path() { return PathText::fromPath(Session::get().path()); }

bool ConfigBridge::userConfig() {
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

QString ConfigBridge::commandLine() {
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

void ConfigBridge::setCaptureOutput(const bool value) {
    if (value == profile().captureOutput) {
        return;
    }

    profile().captureOutput = value;

    emit profileChanged();
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

void ConfigBridge::clearFiles() const {
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

bool ConfigBridge::save() const {
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

bool ConfigBridge::saveZdl(const QString &path) const {
    if (!Import::saveZdlFile(path.toStdString(), profile())) {
        _notifier->error("Could not write " + path + ".");

        return false;
    }

    _notifier->success("Saved " + profileName() + " to " + path + ".");

    return true;
}

bool ConfigBridge::launch() {
    return start(profileKey(), profileName(), config());
}

bool ConfigBridge::launchAt(const int index) {
    const std::vector<Profile> &profiles = config().profiles;

    if (index < 0 || std::cmp_greater_equal(index, profiles.size())) {
        return false;
    }

    setProfileIndex(index);

    return launch();
}

namespace {

// The port and the game and nothing else, on the port's own config. A copy,
// so playing off the library leaves the profile where it was.
Config oneGame(const QString &iwad) {
    Config copy = config();
    Profile &target = copy.activeProfile();
    const std::string port = target.port;

    target.clearSettings();
    target.port = port;
    target.iwad = iwad.toStdString();
    target.sharedConfig = true;

    return copy;
}

}

bool ConfigBridge::launchGame(const QString &iwad) {
    return start(gameKey(iwad), iwad, oneGame(iwad));
}

bool ConfigBridge::start(const QString &key, const QString &title, const Config &what) {
    std::string error;
    Process::Id started = 0;

    /*
    Output is only taken when the profile asks for it. Whoever takes it has to
    read it to the end, so it is not something to have open on the off chance
    that somebody opens the log later.
    */
    Process::Stream output = Process::NOTHING;
    const bool capture = what.activeProfile().captureOutput;
    const QString line = text(Launcher::commandLine(what));

    if (!Launcher::launch(what, &started, capture ? &output : nullptr, &error)) {
        _notifier->error(text(error), QStringLiteral("Nothing was launched"));
        _runs->refused(key, title, text(error));

        return false;
    }

    _runs->began(key, title, line, started, output);

    emit launched();

    return true;
}

QString ConfigBridge::gameKey(const QString &iwad) {
    return QStringLiteral("game:") + iwad;
}

QString ConfigBridge::gameCommandLine(const QString &iwad) {
    return text(Launcher::commandLine(oneGame(iwad)));
}
