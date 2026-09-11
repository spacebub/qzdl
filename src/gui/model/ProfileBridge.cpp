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

#include <string_view>
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

std::vector<State::BadgeSpec> ProfileBridge::badgesOf(const int index) {
    std::vector<State::BadgeSpec> badges;

    if (index < 0 || std::cmp_greater_equal(index, config().profiles.size())) {
        return badges;
    }

    const Profile &each = config().profiles[static_cast<size_t>(index)];
    const bool ready = !each.port.empty() || each.customCommand;
    int loaded = 0;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            ++loaded;
        }
    }

    if (dosPortOf(config(), each)) {
        badges.push_back(State::BadgeSpec{.text = "DOS", .kind = "muted", .dot = true});
    }

    if (!ready) {
        badges.push_back(State::BadgeSpec{.text = "No port", .kind = "warning", .dot = true});
    } else if (!each.files.empty()) {
        const size_t count = each.files.size();
        const std::string said = std::cmp_equal(loaded, count)
            ? std::to_string(count) + (count == 1 ? " file" : " files")
            : std::to_string(loaded) + " of " + std::to_string(count) + " loaded";

        badges.push_back(State::BadgeSpec{.text = said, .kind = "muted", .dot = true});
    }

    const Dialect::NetSupport net = Dialect::net(Dialect::of(config(), each));

    if (const int role = ProfilePanels::netRoleOf(each.multiplayer);
        role != 0 && (role == 1 ? net.hosts : net.joins)) {
        badges.push_back(State::BadgeSpec{
            .text = role == 1 ? "Hosting" : "Multiplayer",
            .kind = "muted",
            .dot = true,
        });
    }

    if (each.replay.mode != 0) {
        badges.push_back(State::BadgeSpec{
            .text = each.replay.mode == 1 ? "Recording" : "Replay",
            .kind = "muted",
            .dot = true,
        });
    }

    return badges;
}

State::ProfileCard ProfileBridge::cardOf(const int index) {
    const Profile &each = config().profiles[static_cast<size_t>(index)];
    int loaded = 0;

    for (const FileEntry &file : each.files) {
        if (file.enabled) {
            ++loaded;
        }
    }

    const NameEntry *game = config().findIwad(each.iwad);

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
        .netRole = ProfilePanels::netRoleOf(each.multiplayer),
        .ready = !each.port.empty() || each.customCommand,
    };
}

std::string ProfileBridge::artKey() {
    const Profile &profile = active();

    return artKeyOf(profile, config().findIwad(profile.iwad));
}

std::string ProfileBridge::zdlFileName() {
    return zdlNameOf(active().name);
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
    _hub->scheduleSave();
}

void ProfileBridge::pushConfigDonors() {
    const Profile &profile = active();
    std::vector<State::ConfigDonor> donors;

    if (!profile.port.empty()) {
        for (const Profile &other : config().profiles) {
            if (other.id == profile.id || !Text::iequals(other.port, profile.port)) {
                continue;
            }

            const std::filesystem::path file = Storage::configFile(other);
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
    _hub->scheduleSave();
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
void ProfileBridge::pushCommand() {
    _hub->bumpRev();
    _hub->schedulePreview();
    _hub->scheduleSave();
}

void ProfileBridge::showCommand() {
    cfg().commandLine = Command::line(config());
    cfg().commandTrouble = Command::trouble(config());

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
    _hub->reload();
}

void ProfileBridge::setIwad(const std::string &value) {
    if (value == active().iwad) {
        return;
    }

    active().iwad = value;

    push();
    touch();
}

void ProfileBridge::setPort(const std::string &value) {
    if (value == active().port) {
        return;
    }

    active().port = value;

    push();
    pushCommand();
}

void ProfileBridge::setSkill(const int value) {
    active().skill = value;

    push();
    pushCommand();
}

void ProfileBridge::setMonsters(const int value) {
    active().monsters = value;

    push();
    pushCommand();
}

void ProfileBridge::setWarp(const std::string &value) {
    active().warp = value;

    push();
    pushCommand();
}

void ProfileBridge::setExtra(const std::string &value) {
    active().extra = value;

    push();
    pushCommand();
}

void ProfileBridge::setSharedConfig(const bool value) {
    active().sharedConfig = value;

    push();
    pushCommand();
}

void ProfileBridge::setCommandOverride(const bool value) {
    if (value && active().command.empty()) {
        active().command = Command::pattern(config());
    }

    active().customCommand = value;

    push();
    pushCommand();
    pushCards();
}

void ProfileBridge::setCommand(const std::string &value) {
    active().command = value;

    push();
    pushCommand();
}

void ProfileBridge::setDosFullscreen(const bool value) {
    active().dosFullscreen = value;

    push();
    pushCommand();
}

void ProfileBridge::setCaptureOutput(const bool value) const {
    active().captureOutput = value;

    push();
}

void ProfileBridge::setLevelstat(const bool value) {
    active().levelstat = value;

    push();
    pushCommand();
}

void ProfileBridge::moveProfile(const int from, const int to) const {
    moveTo(config().profiles, from, to);

    pushCards();
    push();
}

void ProfileBridge::addProfile(const std::string &name) const {
    config().setActiveProfile(config().addProfile(name));
    _hub->reload();
}

void ProfileBridge::duplicateProfile() const {
    if (config().profiles.empty()) {
        return;
    }

    config().setActiveProfile(config().duplicateActiveProfile(active().name));
    _hub->reload();
}

void ProfileBridge::copyEngineConfig(const std::string &id) const {
    const int index = config().indexOfProfile(id);

    if (index < 0) {
        return;
    }

    const Profile &source = config().profiles[static_cast<size_t>(index)];
    const Profile &profile = active();

    const std::filesystem::path taken = Storage::configFile(source);
    const std::filesystem::path here = Storage::configFile(profile);

    if (taken.empty() || here.empty() || taken == here) {
        return;
    }

    std::error_code code;

    std::filesystem::create_directories(here.parent_path(), code);

    if (!std::filesystem::copy_file(taken, here,
                                    std::filesystem::copy_options::overwrite_existing, code)) {
        _notifier->error("Could not copy " + source.name + "'s engine config: " + code.message());

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

    // uniqueProfileName would turn an unchanged name into "name (2)".
    Profile &profile = active();

    profile.name = Text::iequals(profile.name, name) ? Text::trim(name)
                                                     : config().uniqueProfileName(name);

    pushCards();
    push();
}

void ProfileBridge::removeProfile() const {
    config().removeProfile(config().activeProfileId);
    _hub->reload();
}

void ProfileBridge::clearProfile() const {
    active().clearSettings();
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
        _hub->reload();
    }

    _hub->start(ConfigBridge::profileKey(config().activeProfileId), active().name, config());
}
