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

#include "core/config/Import.h"
#include "core/config/Schema.h"
#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "core/util/Text.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfileBridge.h"
#include "gui/model/ProfilePanels.h"
#include "gui/util/Format.h"

namespace {

std::string artKeyOf(const Profile &each, const NameEntry *game) {
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
    const NameEntry *port = config.findPort(profile.port);

    return port != nullptr && port->dosbox;
}

// Whether the profile's port takes the side the profile is on. The dialect is read
// off the port entry alone, so it is kept per port rather than worked out per card.
bool netSupportedOf(const Config &config, const Profile &each, const NetRole role) {
    static std::unordered_map<std::string, Dialect::NetSupport> known;

    const NameEntry *entry = config.findPort(each.port);
    const std::string key = entry == nullptr
        ? std::string()
        : entry->file + (entry->dosbox ? "\n1" : "\n0");

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
        stem.push_back(static_cast<unsigned char>(each) < 0x20 || FORBIDDEN.contains(each)
                       ? '-'
                       : each);
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
        const std::string said = card.loaded == card.files
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
            .text = card.replayMode == ReplayMode::Record ? "Recording" : "Replay",
            .kind = State::BadgeKind::Muted,
            .dot = true,
        });
    }

    return badges;
}

State::ProfileCard ProfileBridge::cardOf(const int index) {
    const Profile &each = config().profiles[static_cast<size_t>(index)];
    const NameEntry *game = config().findIwad(each.iwad);
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
        .ready = !each.port.empty() || each.customCommand,
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

            const std::filesystem::path file = Storage::portConfigFile(config(), other);
            std::error_code asked;

            if (file.empty() || !std::filesystem::is_regular_file(file, asked)) {
                continue;
            }

            donors.push_back(State::ConfigDonor{
                .id = other.id,
                .name = other.name,
                .file = Format::fromPath(file),
                .shared = other.sharedConfig,
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
    state.multiplayerOpen = profile.dialogOpen;
    state.sharedConfig = profile.sharedConfig;
    state.commandOverride = profile.customCommand;
    state.command = profile.command;
    state.dosFullscreen = profile.dosFullscreen;
    state.captureOutput = profile.captureOutput;
    state.levelstat = profile.levelstat;
    state.hasLevelstat = Dialect::of(config()).levelstat;
    state.profileDirectory = Format::fromPath(Storage::profileDirectory(profile));
    state.dosPort = Launcher::isDosPort(config());

    _hub->bumpRev();

    _hub->panels().pushReplay();
    _hub->panels().pushSave();
    _hub->library().pushGameRev();
}

// Opens every ticked file, so only redone when they changed.
void ProfileBridge::pushMaps() {
    const Profile &profile = active();
    const NameEntry *game = config().findIwad(profile.iwad);
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
    active().sharedConfig = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setCommandOverride(const bool value) const {
    if (value && active().command.empty()) {
        active().command = Command::pattern(config());
    }

    active().customCommand = value;

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

void ProfileBridge::setDosFullscreen(const bool value) const {
    active().dosFullscreen = value;

    _hub->scheduleSave();
    push();
    pushCommand();
}

void ProfileBridge::setCaptureOutput(const bool value) const {
    active().captureOutput = value;

    _hub->scheduleSave();
    push();
}

void ProfileBridge::setLevelstat(const bool value) const {
    active().levelstat = value;

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

    _notifier->warning(profile.name + " is still running. Its folder is where the port writes "
                       "its config, its saves and its screenshots, so close it first.");

    return true;
}

void ProfileBridge::duplicateProfile() const {
    if (config().profiles.empty()) {
        return;
    }

    config().setActiveProfile(config().duplicateActiveProfile(active().name));
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
        _notifier->warning("Copied " + source.name + "'s engine config, but this profile "
                           "launches on the port's config, so nothing reads it yet.");

        return;
    }

    _notifier->success("This profile now starts on a copy of " + source.name
                       + "'s engine config.");
}

void ProfileBridge::renameProfile(const std::string &name) const {
    if (name.empty()) {
        return;
    }

    Profile &profile = active();

    if (running(profile)) {
        return;
    }

    // uniqueProfileName would turn an unchanged name into "name (2)".
    const bool same = Text::iequals(profile.name, name);
    const std::string renamed = same ? Text::trim(name) : config().uniqueProfileName(name);

    // The name is taken up only once its folder has followed.
    if (!same && Storage::ownsDirectory(config(), profile)) {
        const std::string file = config().uniqueConfigFile(renamed, profile.id);

        if (std::string error; !Storage::renameDirectory(profile, file, &error)) {
            _notifier->error("Could not move " + profile.name + "'s folder, so the name "
                             "stays: " + error);

            return;
        }
    }

    profile.name = renamed;

    _hub->scheduleSave();
    pushCards();
    push();
}

void ProfileBridge::removeProfile() const {
    const Profile profile = active();

    if (running(profile)) {
        return;
    }

    if (Storage::ownsDirectory(config(), profile)) {
        if (std::string error; !Storage::discardDirectory(profile, &error)) {
            _notifier->warning("Deleted " + profile.name + ", but its folder is still there: "
                               + error);
        }
    }

    config().removeProfile(profile.id);
    _hub->scheduleSave();
    _hub->reload();
}

std::string ProfileBridge::removalNote() {
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
    Profile loaded;

    if (!Import::loadZdlFile(std::filesystem::path(path), loaded)) {
        _notifier->error("Could not read " + path + " as a .zdl file.");

        return;
    }

    loaded.name = config().uniqueProfileName(loaded.name);
    config().profiles.push_back(loaded);

    config().ensureConfigFiles();
    config().setActiveProfile(loaded.id);

    _hub->scheduleSave();
    _hub->reload();

    _notifier->success("Added " + loaded.name + " from " + path + ".");
}

void ProfileBridge::saveZdl(const std::string &path) const {
    if (!Import::saveZdlFile(std::filesystem::path(path), active())) {
        _notifier->error("Could not write " + path + ".");

        return;
    }

    _notifier->success("Saved " + active().name + " to " + path + ".");
}

void ProfileBridge::launch() const {
    if (config().profiles.empty()) {
        return;
    }

    _hub->start(ConfigBridge::profileKey(config().activeProfileId), active().name, config());
}

void ProfileBridge::launchAt(const int index) const {
    const std::vector<Profile> &profiles = config().profiles;

    if (index < 0 || std::cmp_greater_equal(index, profiles.size())) {
        return;
    }

    if (profiles[static_cast<size_t>(index)].id != config().activeProfileId) {
        config().setActiveProfile(profiles[static_cast<size_t>(index)].id);

        _hub->scheduleSave();
        _hub->reload();
    }

    _hub->start(ConfigBridge::profileKey(config().activeProfileId), active().name, config());
}
