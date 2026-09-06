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

#include <algorithm>

#include "core/Import.h"
#include "core/Launcher.h"
#include "core/Paths.h"
#include "core/Session.h"
#include "core/Text.h"
#include "gui/ConfigBridge.h"
#include "gui/PathText.h"

namespace {

// Long enough that a field being typed into is one write rather than twenty,
// short enough that a config is never more than a moment behind the window.
constexpr int AUTOSAVE_DELAY = 400;

Config &config() {
    return Session::get().config();
}

Profile &profile() {
    return config().activeProfile();
}

MultiplayerSettings &multiplayer() {
    return profile().multiplayer;
}

// 0 plays alone, 1 hosts, 2 joins. A count is what makes this machine the one
// others connect to.
int netRoleOf(const MultiplayerSettings &mp) {
    if (mp.gameType == 0) {
        return 0;
    }

    return mp.players > 0 ? 1 : 2;
}

/*
The port a game off the library runs on: the one the shelf was set to, or the
open profile's while it has not been set. A name the port list no longer has
counts as unset, which is what a port removed behind its back leaves.
*/
std::string gamePortName() {
    const std::string &chosen = config().general.gamePort;

    return !chosen.empty() && config().findPort(chosen) != nullptr ? chosen : profile().port;
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
      _ports(new NameList(NameList::Kind::Ports, this)),
      _profiles(new ProfileList(this)),
      _autosave(new QTimer(this)) {
    /*
    Nothing here has a Save beside it, so the config is written a moment after
    it changes rather than only on the way out. A moment, not at once: a field
    being typed into changes on every key, and each of those is a whole file.
    */
    _autosave->setSingleShot(true);
    _autosave->setInterval(AUTOSAVE_DELAY);

    connect(_autosave, &QTimer::timeout, this, &ConfigBridge::flush);

    connect(this, &ConfigBridge::profilesChanged, this, &ConfigBridge::scheduleSave);
    connect(this, &ConfigBridge::profileChanged, this, &ConfigBridge::scheduleSave);
    connect(this, &ConfigBridge::multiplayerChanged, this, &ConfigBridge::scheduleSave);
    connect(this, &ConfigBridge::generalChanged, this, &ConfigBridge::scheduleSave);
    connect(this, &ConfigBridge::commandLineChanged, this, &ConfigBridge::scheduleSave);

    // Anything at all about a profile is on its card somewhere, so every one of
    // them is drawn again whenever any of them changes.
    connect(this, &ConfigBridge::profilesChanged, _profiles, &ProfileList::refresh);

    // Reordering the shelf leaves the active profile on another row, and the
    // picker on the profile page is drawn from that row.
    connect(_profiles, &ProfileList::changed, this, [this] {
        emit profilesChanged();
        emit profileChanged();
    });

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

        if (config().general.gamePort == before.toStdString()) {
            config().general.gamePort = after.toStdString();

            emit generalChanged();
        }

        emit profileChanged();
        touch();
    });

    connect(_iwads, &NameList::changed, this, &ConfigBridge::touch);
    connect(_ports, &NameList::changed, this, [this] {
        // Pointing the shelf at a port that has since been removed is the same
        // as not having pointed it anywhere.
        if (const std::string &chosen = config().general.gamePort;
            !chosen.empty() && config().findPort(chosen) == nullptr) {
            config().general.gamePort.clear();

            emit generalChanged();
        }

        // Marking the port a profile is on as a DOS one changes what that
        // profile can do, which is read off the profile rather than the list.
        emit profileChanged();
        emit commandLineChanged();
    });
}

void ConfigBridge::scheduleSave() {
    _pendingSave = true;

    _autosave->start();
}

void ConfigBridge::flush() {
    if (!_pendingSave) {
        return;
    }

    _autosave->stop();
    _pendingSave = false;

    std::string error;

    // Said once. A config that cannot be written fails on every keystroke after
    // it too, and a wall of the same complaint helps nobody.
    if (Session::get().save(&error)) {
        _warnedSave = false;

        return;
    }

    if (!_warnedSave) {
        _warnedSave = true;
        _notifier->error("Could not save the config: " + text(error));
    }
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
ProfileList *ConfigBridge::profiles() const { return _profiles; }

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

namespace {

// Whether this profile is on a port that only runs under DOSBox.
bool dosPortOf(const Profile &profile) {
    const NameEntry *port = config().findPort(profile.port);

    return port != nullptr && port->dosbox;
}

}

QVariantList ConfigBridge::profileCards() {
    QVariantList cards;

    for (size_t index = 0; index < config().profiles.size(); ++index) {
        cards.append(profileCard(static_cast<int>(index)));
    }

    return cards;
}

QVariantMap ConfigBridge::profileCard(const int index) {
    const std::vector<Profile> &profiles = config().profiles;

    if (index < 0 || std::cmp_greater_equal(index, profiles.size())) {
        return {};
    }

    const Profile &each = profiles[static_cast<size_t>(index)];
    int loaded = 0;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            ++loaded;
        }
    }

    return QVariantMap{
        {QStringLiteral("index"), index},
        {QStringLiteral("id"), text(each.id)},

        {QStringLiteral("key"), QStringLiteral("profile:") + text(each.id)},
        {QStringLiteral("name"), each.name.empty() ? QStringLiteral("(unnamed)") : text(each.name)},
        {QStringLiteral("iwad"), text(each.iwad)},
        {QStringLiteral("iwadFile"), iwadFile(text(each.iwad))},
        {QStringLiteral("port"), text(each.port)},
        {QStringLiteral("dosPort"), dosPortOf(each)},
        {QStringLiteral("warp"), text(each.warp)},
        {QStringLiteral("files"), static_cast<int>(each.files.size())},
        {QStringLiteral("loaded"), loaded},
        {QStringLiteral("netRole"), netRoleOf(each.multiplayer)},

        // One that writes its own command needs no port to run.
        {QStringLiteral("ready"), !each.port.empty() || each.customCommand},
    };
}

QString ConfigBridge::iwadFile(const QString &name) {
    const NameEntry *entry = config().findIwad(name.toStdString());

    return entry == nullptr ? QString() : text(entry->file);
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

int ConfigBridge::netRole() { return netRoleOf(multiplayer()); }

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

bool ConfigBridge::multiplayerSet() { return multiplayer() != MultiplayerSettings(); }

QString ConfigBridge::gamePort() { return text(config().general.gamePort); }
QString ConfigBridge::alwaysAdd() { return text(config().general.alwaysAdd); }
QString ConfigBridge::dosbox() { return text(config().general.dosbox); }
bool ConfigBridge::dosPort() { return Launcher::isDosPort(config()); }
QString ConfigBridge::systemDosbox() { return PathText::fromPath(Launcher::systemDosbox()); }
bool ConfigBridge::autoClose() { return config().general.autoClose; }
bool ConfigBridge::launchZdlImmediately() { return config().general.launchZdlImmediately; }
bool ConfigBridge::showPaths() { return config().general.showPaths; }

QString ConfigBridge::startView() {
    return config().general.startView == "games" ? QStringLiteral("games")
                                                 : QStringLiteral("profiles");
}

bool ConfigBridge::captureOutput() { return profile().captureOutput; }
bool ConfigBridge::profileConfigs() { return config().general.profileConfigs; }

QString ConfigBridge::path() { return PathText::fromPath(Session::get().path()); }

bool ConfigBridge::userConfig() {
    return Session::get().path() == Paths::get().configPath(Paths::USER);
}

bool ConfigBridge::ignoreUserConfig() { return Session::get().userConfigIgnored(); }

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

bool ConfigBridge::commandOverride() { return profile().customCommand; }
QString ConfigBridge::command() { return text(profile().command); }
QString ConfigBridge::commandTrouble() { return text(Launcher::commandTrouble(config())); }
bool ConfigBridge::dosFullscreen() { return profile().dosFullscreen; }

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

void ConfigBridge::setCommandOverride(const bool value) {
    if (value == profile().customCommand) {
        return;
    }

    /*
    Taken over for the first time, it starts as what ZDL would have run, with
    the port, the game and the add-ons put back as what they stand for. It is
    a line to edit rather than a blank one to work out from nothing.
    */
    if (value && profile().command.empty()) {
        profile().command = Launcher::commandTemplate(config());
    }

    profile().customCommand = value;

    emit profileChanged();
    emit commandLineChanged();
}

void ConfigBridge::setCommand(const QString &value) {
    if (value.toStdString() == profile().command) {
        return;
    }

    profile().command = value.toStdString();

    emit profileChanged();
    emit commandLineChanged();
}

void ConfigBridge::setDosFullscreen(const bool value) {
    if (value == profile().dosFullscreen) {
        return;
    }

    profile().dosFullscreen = value;

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
        multiplayerTouched();                                 \
    }

void ConfigBridge::multiplayerTouched() {
    emit multiplayerChanged();
    emit commandLineChanged();
}

void ConfigBridge::setNetRole(const int value) {
    if (value == netRole()) {
        return;
    }

    MultiplayerSettings &mp = multiplayer();

    if (value == 0) {
        mp.gameType = 0;
    } else {
        if (mp.gameType == 0) {
            mp.gameType = 1;
        }

        // The count is the difference between the two, so setting the role is
        // what puts it right: a host needs one and a joiner must not have it.
        mp.players = value == 1 ? std::max(mp.players, 2) : 0;
    }

    emit multiplayerChanged();
    emit commandLineChanged();
    emit profilesChanged();
}

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

// The two a card is drawn from. Between them they say which side the profile is
// on, which the shelf shows on every one of them, so the shelf is told too.
void ConfigBridge::setGameType(const int value) {
    if (value == multiplayer().gameType) {
        return;
    }

    multiplayer().gameType = value;

    emit multiplayerChanged();
    emit commandLineChanged();
    emit profilesChanged();
}

void ConfigBridge::setPlayers(const int value) {
    if (value == multiplayer().players) {
        return;
    }

    multiplayer().players = value;

    emit multiplayerChanged();
    emit commandLineChanged();
    emit profilesChanged();
}

void ConfigBridge::setGamePort(const QString &value) {
    if (value.toStdString() == config().general.gamePort) {
        return;
    }

    config().general.gamePort = value.toStdString();

    emit generalChanged();
}

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

void ConfigBridge::setDosbox(const QString &value) {
    const std::string wanted = value.toStdString();

    if (wanted == config().general.dosbox) {
        return;
    }

    config().general.dosbox = wanted;

    emit generalChanged();

    // It is the front of the command line for every DOS port there is.
    emit commandLineChanged();
}

void ConfigBridge::setLaunchZdlImmediately(const bool value) {
    if (value == config().general.launchZdlImmediately) {
        return;
    }

    config().general.launchZdlImmediately = value;

    emit generalChanged();
}

void ConfigBridge::setIgnoreUserConfig(const bool value) {
    if (value == Session::get().userConfigIgnored()) {
        return;
    }

    std::string error;

    // The flag belongs to the user config, so unless that is the one open this
    // writes another file there and then rather than at shutdown.
    if (!Session::get().setUserConfigIgnored(value, &error)) {
        _notifier->error("Could not write the user config: " + text(error));

        return;
    }

    emit generalChanged();
}

void ConfigBridge::setStartView(const QString &value) {
    const std::string wanted = value == QLatin1String("games") ? "games" : "profiles";

    if (wanted == config().general.startView) {
        return;
    }

    config().general.startView = wanted;

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
    _profiles->reload();

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

    // uniqueProfileName compares against every profile including this one, so a
    // name left as it was must not turn into "name (2)".
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

void ConfigBridge::clearMultiplayer() {
    if (!multiplayerSet()) {
        return;
    }

    multiplayer() = MultiplayerSettings();

    emit multiplayerChanged();
    emit commandLineChanged();
    emit profilesChanged();
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

    // Whatever the config being left behind still owed is written to it, not
    // to the file about to take its place.
    flush();

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

QString ConfigBridge::zdlFileName() {
    // A profile is named by hand, so it can hold anything; a file name cannot.
    static const QString FORBIDDEN = QStringLiteral(R"(/\:*?"<>|)");
    QString stem;

    const QString name = profileName();

    for (const QChar each : name) {
        stem.append(each.unicode() < 0x20 || FORBIDDEN.contains(each) ? QChar('-') : each);
    }

    stem = stem.trimmed();

    // Trailing dots and spaces are dropped by Windows, which would leave the
    // name it saved under different from the name it shows.
    while (!stem.isEmpty() && (stem.endsWith('.') || stem.endsWith(' '))) {
        stem.chop(1);
    }

    return (stem.isEmpty() ? QStringLiteral("profile") : stem) + QStringLiteral(".zdl");
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
    const std::string port = gamePortName();

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
    /*
    A DOS port prints into DOSBox's window and nowhere this can read, so
    there is nothing to hand a pipe to. Closing on launch takes the log with
    it, which leaves a pipe nobody is left to drain.
    */
    const bool capture = what.activeProfile().captureOutput
        && !Launcher::isDosPort(what)
        && !what.general.autoClose;
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
