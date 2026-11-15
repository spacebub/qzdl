/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "ttk/system/Text.h"
#include "ttk/util/Format.h"

#include "core/config/Import.h"
#include "core/config/Schema.h"
#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfileBridge.h"
#include "gui/model/ProfilePanels.h"

#ifdef _WIN32
#include "core/config/ConfigPaths.h"
#include "core/config/Session.h"
#include "core/launch/Shortcut.h"
#endif

using namespace ttk;

namespace {

std::string artKeyOf(const Profile &each, const GameEntry *game) {
    std::string key = game == nullptr ? std::string() : game->file;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            key += '\n';
            key += file.file;
        }
    }

    return key;
}

bool dosPortOf(const Config &config, const Profile &profile) {
    const PortEntry *port = config.findPort(profile.port);

    return port != nullptr && port->dosbox;
}

// Whether the profile's port takes the side the profile is on. The dialect is read
// off the port entry alone, so it is kept per port rather than worked out per card.
bool netSupportedOf(const Config &config, const Profile &each, const NetRole role) {
    static std::unordered_map<std::string, Dialect::NetSupport> known;

    const PortEntry *entry = config.findPort(each.port);
    const std::string key =
            entry == nullptr ? std::string() : entry->file + (entry->dosbox ? "\n1\n" : "\n0\n") + entry->dialect;

    auto found = known.find(key);

    if (found == known.end()) {
        if (known.size() >= 64) {
            known.clear();
        }

        found = known.emplace(key, Dialect::net(Dialect::of(config, each))).first;
    }

    return role == NetRole::Host ? found->second.hosts : found->second.joins;
}

std::string zdlNameOf(const std::string &name) {
    static constexpr std::string_view FORBIDDEN = R"(/\:*?"<>|)";
    std::string stem;

    for (const char each : name) {
        stem.push_back(static_cast<unsigned char>(each) < 0x20 || FORBIDDEN.contains(each) ? '-' : each);
    }

    stem = Text::trim(stem);

    // Windows drops trailing dots and spaces.
    while (!stem.empty() && (stem.back() == '.' || stem.back() == ' ')) {
        stem.pop_back();
    }

    return (stem.empty() ? ConfigFile::PROFILE_STEM : stem) + ConfigFile::ZDL_EXT;
}

}

std::vector<State::BadgeSpec> ProfileBridge::badgesOf(const State::ProfileCard &card) {
    std::vector<State::BadgeSpec> badges;

    if (card.dosPort) {
        badges.push_back(State::BadgeSpec{.text = "DOS", .kind = State::BadgeKind::Muted, .dot = true});
    }

    if (!card.ready) {
        badges.push_back(State::BadgeSpec{.text = "No port", .kind = State::BadgeKind::Warning, .dot = true});
    } else if (card.files > 0) {
        const std::string said =
                card.loaded == card.files
                        ? std::to_string(card.files) + (card.files == 1 ? " file" : " files")
                        : std::to_string(card.loaded) + " of " + std::to_string(card.files) + " loaded";

        badges.push_back(State::BadgeSpec{.text = said, .kind = State::BadgeKind::Muted, .dot = true});
    }

    if (card.netRole != NetRole::Alone && card.netSupported) {
        badges.push_back(State::BadgeSpec{
                .text = card.netRole == NetRole::Host ? "Hosting" : "Multiplayer",
                .kind = State::BadgeKind::Muted,
                .dot = true,
        });
    }

    if (card.replayMode != ReplayMode::Off) {
        badges.push_back(State::BadgeSpec{
                .text = card.replayMode == ReplayMode::Play ? "Replay" : "Recording",
                .kind = State::BadgeKind::Muted,
                .dot = true,
        });
    }

    return badges;
}

State::ProfileCard ProfileBridge::cardOf(const int index) {
    const Profile &each = config().profiles[static_cast<size_t>(index)];
    const GameEntry *game = config().findIwad(each.iwad);
    const NetRole role = ProfilePanels::netRoleOf(each.multiplayer);
    int loaded = 0;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            ++loaded;
        }
    }

    return State::ProfileCard{
            .index = index,
            .id = each.id,
            .key = ConfigBridge::profileKey(each.id),
            .name = each.name.empty() ? "(unnamed)" : each.name,
            .iwad = each.iwad,
            .artKey = artKeyOf(each, game),
            .port = each.port,
            .dosPort = dosPortOf(config(), each),
            .warp = each.warp,
            .files = static_cast<int>(each.files.size()),
            .loaded = loaded,
            .netRole = role,
            .ready = !each.port.empty() || each.on(Switch::customCommand),
            .netSupported = role != NetRole::Alone && netSupportedOf(config(), each, role),
            .replayMode = each.replay.mode,
    };
}

std::string ProfileBridge::artKey() {
    const Profile &profile = active();

    return artKeyOf(profile, config().findIwad(profile.iwad));
}

std::string ProfileBridge::zdlFileName() {
    return zdlNameOf(active().name);
}

bool ProfileBridge::launchable() {
    return cfg().commandOverride ? cfg().commandTrouble.empty() : !cfg().port.empty();
}

void ProfileBridge::pushCards() const {
    std::vector<State::ProfileCard> cards;

    cards.reserve(config().profiles.size());

    for (size_t index = 0; index < config().profiles.size(); ++index) {
        cards.push_back(cardOf(static_cast<int>(index)));
    }

    cfg().profileCards = std::move(cards);

    _hub->bumpRev();
    _hub->library().pushShelf();
}

void ProfileBridge::pushConfigDonors() {
    const Profile &profile = active();
    std::vector<State::ConfigDonor> donors;

    if (!profile.port.empty()) {
        for (const Profile &other : config().profiles) {
            if (other.id == profile.id || !Text::iequals(other.port, profile.port)) {
                continue;
            }

            const std::filesystem::path file = Storage::writtenPortConfig(config(), other);
            std::error_code asked;

            if (file.empty() || !std::filesystem::is_regular_file(file, asked)) {
                continue;
            }

            donors.push_back(State::ConfigDonor{
                    .id = other.id,
                    .name = other.name,
                    .file = Format::from_path(file),
                    .shared = other.on(Switch::sharedConfig),
            });
        }
    }

    cfg().configDonors = std::move(donors);

    State::get().touch();
}

void ProfileBridge::push() const {
    State::Cfg &state = cfg();
    const Profile &profile = active();

    state.profileIndex = config().activeProfileIndex();
    state.profileName = profile.name;
    state.profileKey = ConfigBridge::profileKey(config().activeProfileId);
    state.iwad = profile.iwad;
    state.port = profile.port;
    state.skill = profile.skill;
    state.monsters = profile.monsters;
    state.warp = profile.warp;
    state.extra = profile.extra;
    state.multiplayerOpen = profile.on(Switch::dialogOpen);
    state.sharedConfig = profile.on(Switch::sharedConfig);
    state.commandOverride = profile.on(Switch::customCommand);
    state.command = profile.command;
    state.prefix = profile.prefix;
    state.environment = profile.environment;
    state.dosFullscreen = profile.on(Switch::dosFullscreen);
    state.dosExit = profile.on(Switch::dosExit);
    state.captureOutput = profile.on(Switch::captureOutput);
    state.levelstat = profile.on(Switch::levelstat);
    const Dialect::Port speaks = Dialect::of(config());

    state.hasLevelstat = speaks.levelstat;
    state.pistolStart = profile.on(Switch::pistolStart);
    state.hasPistolStart = speaks.pistolStart != Dialect::PistolStart::none;
    state.pistolStartSwitch = speaks.pistolStart == Dialect::PistolStart::flag;
    state.cheats = profile.on(Switch::cheats);
    state.hasCheats = speaks.cheats;
    state.skipAutoload = profile.on(Switch::skipAutoload);
    state.hasNoAutoload = !speaks.noAutoload.empty();
    state.music = profile.on(Switch::music);
    state.hasMusic = speaks.nomusic;
    state.soundEffects = profile.on(Switch::soundEffects);
    state.hasSoundEffects = !speaks.nosfx.empty();
    state.display = profile.display;
    state.hasDisplay = speaks.display != Dialect::Windowing::none;
    state.displayKept = speaks.displayKept;
    state.hasShotdir = speaks.shotdir;
    state.generalOpen = profile.on(Switch::generalOpen);
    state.screenshots = profile.screenshots;
    state.screenshotPath = Format::from_path(Storage::screenshotDirectory(config()));
    state.screenshotFolder = profile.screenshotFolder;
    state.profileDirectory = Format::from_path(Storage::profileDirectory(profile));
    state.dosPort = Launcher::isDosPort(config());

    _hub->bumpRev();

    _hub->panels().pushReplay();
    _hub->panels().pushSave();
    _hub->panels().pushGameplay();
    ProfilePanels::pushMultiplayer();
    _hub->library().pushGameRev();
}

// Opens every ticked file, so only redone when they changed.
void ProfileBridge::pushMaps() {
    const Profile &profile = active();
    const GameEntry *game = config().findIwad(profile.iwad);
    std::string mark = game == nullptr ? std::string() : game->file;

    for (const FileEntry &entry : profile.files) {
        if (entry.enabled) {
            mark += '\n';
            mark += entry.file;
        }
    }

    if (_mapsKnown && mark == _mapsMark) {
        return;
    }

    _maps = Arguments::maps(config());
    _mapsMark = std::move(mark);
    _mapsKnown = true;

    cfg().maps = _maps;

    State::get().touch();
}

// Building the line opens the game, so it is debounced.
void ProfileBridge::pushCommand() const {
    _hub->bumpRev();
    _hub->schedulePreview();
}

void ProfileBridge::showCommand() {
    cfg().commandLine = Command::line(config());
    cfg().commandTrouble = Command::trouble(config());
    cfg().dosCommands = Command::dosSpend(config());

    State::get().touch();
}

void ProfileBridge::touch() {
    pushMaps();
    pushCommand();
    pushCards();
}

void ProfileBridge::setProfileIndex(const int index) const {
    const std::vector<Profile> &profiles = config().profiles;

    if (index < 0 || std::cmp_greater_equal(index, profiles.size())
        || profiles[static_cast<size_t>(index)].id == config().activeProfileId) {
        return;
    }

    config().setActiveProfile(profiles[static_cast<size_t>(index)].id);
    _hub->scheduleSave();
    _hub->reload();
}

void ProfileBridge::setIwad(const std::string &value) {
    if (value == active().iwad) {
        return;
    }

    active().iwad = value;

    _hub->scheduleSave();
    push();
    touch();
}

void ProfileBridge::setPort(const std::string &value) const {
    if (value == active().port) {
        return;
    }

    active().port = value;

    _hub->scheduleSave();
    push();
    pushCards();
    pushCommand();
}

void ProfileBridge::setSkill(const int value) const {
    active().skill = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setMonsters(const int value) const {
    active().monsters = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setWarp(const std::string &value) const {
    active().warp = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setExtra(const std::string &value) const {
    active().extra = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setSharedConfig(const bool value) const {
    active().set(Switch::sharedConfig, value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setCommandOverride(const bool value) const {
    if (value && active().command.empty()) {
        active().command = Command::pattern(config());
    }

    active().set(Switch::customCommand, value);

    _hub->scheduleSave();
    push();
    pushCommand();
    pushCards();
}

void ProfileBridge::setCommand(const std::string &value) const {
    active().command = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setPrefix(const std::string &value) const {
    active().prefix = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setEnvironment(std::vector<EnvVariable> value) const {
    active().environment = std::move(value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setDosFullscreen(const bool value) const {
    active().set(Switch::dosFullscreen, value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setDosExit(const bool value) const {
    active().set(Switch::dosExit, value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setCaptureOutput(const bool value) const {
    active().set(Switch::captureOutput, value);

    _hub->scheduleSave();
    push();
}

void ProfileBridge::setLevelstat(const bool value) const {
    active().set(Switch::levelstat, value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setPistolStart(const bool value) const {
    active().set(Switch::pistolStart, value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setCheats(const bool value) const {
    active().set(Switch::cheats, value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setSkipAutoload(const bool value) const {
    active().set(Switch::skipAutoload, value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setMusic(const bool value) const {
    active().set(Switch::music, value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setSoundEffects(const bool value) const {
    active().set(Switch::soundEffects, value);

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setDisplay(const Display value) const {
    active().display = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setScreenshots(const Screenshots value) const {
    active().screenshots = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setScreenshotFolder(const std::string &value) const {
    active().screenshotFolder = value;
    active().screenshots = Screenshots::Custom;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::moveProfile(const int from, const int to) const {
    moveTo(config().profiles, from, to);

    _hub->scheduleSave();
    pushCards();
    push();
}

void ProfileBridge::addProfile(const std::string &name) const {
    config().setActiveProfile(config().addProfile(name));
    _hub->scheduleSave();
    _hub->reload();
}

// True with a toast when the profile's port is still up, so its folder must stay put.
bool ProfileBridge::running(const Profile &profile) const {
    if (!_hub->busy(profile.id)) {
        return false;
    }

    _notifier->warning(profile.name
                       + " is still running. Its folder is where the port writes "
                         "its config, its saves and its screenshots, so close it first.");

    return true;
}

void ProfileBridge::duplicateProfile() const {
    if (config().profiles.empty()) {
        return;
    }

    config().setActiveProfile(config().duplicateActiveProfile(active().name));
    Storage::forgetOwnSave(config());
    _hub->scheduleSave();
    _hub->reload();
}

void ProfileBridge::copyEngineConfig(const std::string &id) const {
    const int index = config().indexOfProfile(id);

    if (index < 0) {
        return;
    }

    const Profile &source = config().profiles[static_cast<size_t>(index)];

    // Either port up would be rewriting the file as it is copied.
    if (running(active()) || running(source)) {
        return;
    }

    std::string error;

    if (!Storage::copyPortConfig(config(), source, active(), &error)) {
        _notifier->error("Could not copy " + source.name + "'s engine config: " + error);

        return;
    }

    if (Storage::configFile(config()).empty()) {
        _notifier->warning("Copied " + source.name
                           + "'s engine config, but this profile "
                             "launches on the port's config, so nothing reads it yet.");

        return;
    }

    _notifier->success("This profile now starts on a copy of " + source.name + "'s engine config.");
}

void ProfileBridge::renameProfile(const std::string &name) const {
    const std::string wanted = Text::trim(name);

    if (wanted.empty()) {
        return;
    }

    Profile &profile = active();

    if (running(profile)) {
        return;
    }

    // uniqueProfileName would turn an unchanged name into "name (2)".
    const bool same = Text::iequals(profile.name, wanted);
    const std::string renamed = same ? wanted : config().uniqueProfileName(wanted);

    // The name is taken up only once its folder has followed.
    if (!same && Storage::ownsDirectory(config(), profile)) {
        const std::string file = config().uniqueConfigFile(renamed, profile.id);

        if (std::string error; !Storage::renameDirectory(profile, file, &error)) {
            _notifier->error("Could not move " + profile.name
                             + "'s folder, so the name "
                               "stays: "
                             + error);

            return;
        }
    }

    profile.name = renamed;

    _hub->scheduleSave();
    pushCards();
    push();

    // The folder went with the name, and the command names the folder.
    pushCommand();
}

void ProfileBridge::removeProfile() const {
    const Profile profile = active();

    if (running(profile)) {
        return;
    }

    if (Storage::ownsDirectory(config(), profile)) {
        if (std::string error; !Storage::discardDirectory(profile, &error)) {
            _notifier->warning("Deleted " + profile.name + ", but its folder is still there: " + error);
        }
    }

#ifdef _WIN32
    if (const std::filesystem::path data = ConfigPaths::dataDirectory(); !data.empty()) {
        std::error_code code;

        std::filesystem::remove(data / ConfigFile::SHORTCUTS_DIR / (profile.id + ConfigFile::ZDL_EXT), code);
    }
#endif

    config().removeProfile(profile.id);
    _hub->scheduleSave();
    _hub->reload();
}

const char *ProfileBridge::removalNote() {
    return Storage::ownsDirectory(config(), active())
                   ? "The profile goes, and so does its folder: the engine config it kept, its saves "
                     "and its replays. The files it loaded are left alone."
                   : "The profile goes. Its folder and the files it loaded are left alone.";
}

void ProfileBridge::clearProfile() const {
    active().clearSettings();
    _hub->scheduleSave();
    _hub->reload();
}

void ProfileBridge::loadZdl(const std::string &path) const {
    loadZdls({path});
}

void ProfileBridge::loadZdls(const std::vector<std::string> &paths) const {
    // The name each loaded one got, and where it came from.
    std::vector<std::pair<std::string, std::string>> added;
    std::vector<std::pair<std::string, std::string>> known;

    for (const std::string &path : paths) {
        std::string id;

        switch (Import::openZdlFile(config(), std::filesystem::path(path), id)) {
            case Import::ZdlOpen::Failed:
                _notifier->error("Could not read " + path + " as a .zdl file.");

                continue;
            case Import::ZdlOpen::Orphaned:
                _notifier->error(path + " belongs to a desktop shortcut whose profile is not in this config.");

                continue;
            case Import::ZdlOpen::Added:
                added.emplace_back(config().profiles.back().name, path);

                break;
            case Import::ZdlOpen::Known:
                known.emplace_back(config().profiles[static_cast<size_t>(config().indexOfProfile(id))].name, path);

                break;
        }

        config().setActiveProfile(id);
    }

    if (added.empty() && known.empty()) {
        return;
    }

    config().ensureConfigFiles();

    _hub->scheduleSave();
    _hub->reload();

    if (added.size() == 1) {
        _notifier->success("Added " + added.front().first + " from " + added.front().second + ".");
    } else if (!added.empty()) {
        _notifier->success("Added " + std::to_string(added.size()) + " profiles from .zdl files.");
    }

    if (known.size() == 1) {
        _notifier->info(known.front().first + " was already loaded from " + known.front().second + ".");
    } else if (!known.empty()) {
        _notifier->info(std::to_string(known.size()) + " of the .zdl files were already loaded.");
    }
}

void ProfileBridge::saveZdl(const std::string &path) const {
    if (!Import::saveZdlFile(std::filesystem::path(path), active())) {
        _notifier->error("Could not write " + path + ".");

        return;
    }

    config().claimZdl(config().activeProfileId, path);
    _hub->scheduleSave();

    _notifier->success("Saved " + active().name + " to " + path + ".");
}

#ifdef _WIN32
void ProfileBridge::createShortcut() const {
    const Profile &profile = active();
    const Session &session = Session::get();
    const std::filesystem::path data = ConfigPaths::dataDirectory();

    // Named by id, so a rename or a Save as .zdl leaves the icon on this profile.
    const std::filesystem::path zdl = data / ConfigFile::SHORTCUTS_DIR / (profile.id + ConfigFile::ZDL_EXT);
    std::error_code code;

    if (!data.empty()) {
        std::filesystem::create_directories(zdl.parent_path(), code);
    }

    if (data.empty() || code || !Import::saveZdlFile(zdl, profile, true)) {
        _notifier->error("Could not write a .zdl for " + profile.name + " to launch from the desktop.");

        return;
    }

    // A plain start reads another config than one opened by hand.
    const std::filesystem::path opened = session.openedByHand() ? session.path() : std::filesystem::path();
    std::string why;
    const std::filesystem::path made =
            Shortcut::toDesktop(std::filesystem::path(zdlNameOf(profile.name)).stem().string(), zdl, opened, &why);

    if (made.empty()) {
        _notifier->error("Could not create a desktop shortcut for " + profile.name + ": " + why + ".");

        return;
    }

    _notifier->success("Created " + made.stem().string() + " on the desktop. It launches " + profile.name
                       + " without opening ZDL4.");
}
#endif

void ProfileBridge::launch() const {
    if (config().profiles.empty()) {
        return;
    }

    // The add-ons are looked at again, and the rows and the preview follow what the launch saw.
    _hub->lists().push();
    pushCommand();

    _hub->start(ConfigBridge::profileKey(config().activeProfileId), active().name, config());
}

void ProfileBridge::launchAt(const int index) const {
    const std::vector<Profile> &profiles = config().profiles;

    if (index < 0 || std::cmp_greater_equal(index, profiles.size())) {
        return;
    }

    if (profiles[static_cast<size_t>(index)].id == config().activeProfileId) {
        launch();

        return;
    }

    config().setActiveProfile(profiles[static_cast<size_t>(index)].id);

    // The reload looks at the add-ons.
    _hub->scheduleSave();
    _hub->reload();

    _hub->start(ConfigBridge::profileKey(config().activeProfileId), active().name, config());
}
