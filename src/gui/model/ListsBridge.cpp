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

#include <chrono>
#include <map>
#include <utility>

#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "core/util/Text.h"
#include "core/wad/FileInfo.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ListsBridge.h"
#include "gui/util/Format.h"

namespace {

// Cached briefly: one list change stats every game and add-on.
bool missing(const std::filesystem::path &path) {
    using Clock = std::chrono::steady_clock;

    struct Known {
        Clock::time_point asked;
        bool gone = false;
    };

    static constexpr std::chrono::seconds FRESH{2};
    static std::map<std::filesystem::path, Known> seen;

    const Clock::time_point now = Clock::now();

    if (const auto found = seen.find(path);
        found != seen.end() && now - found->second.asked < FRESH) {
        return found->second.gone;
    }

    std::error_code code;
    const bool gone = !std::filesystem::exists(path, code);

    seen[path] = Known{.asked = now, .gone = gone};

    return gone;
}

bool folder(const std::filesystem::path &path) {
    std::error_code code;

    return std::filesystem::is_directory(path, code);
}

}

const std::vector<NameEntry> &ListsBridge::ports() {
    return config().ports;
}

State::NameRow ListsBridge::rowOf(const std::vector<NameEntry> &list, const int index,
                                  const bool ports) {
    const NameEntry &entry = list[static_cast<size_t>(index)];
    const std::filesystem::path path(entry.file);
    const std::string root = Format::fromPath(Catalog::directory());

    return State::NameRow{
        .index = index,
        .name = entry.name,
        .file = entry.file,
        .directory = Format::fromPath(path.parent_path()),
        .kind = Text::lower(path.extension().string()),
        .missing = missing(path),
        .dosbox = entry.dosbox,
        .fetched = ports && !root.empty() && Format::fromPath(path).starts_with(root + "/"),
        .detected = ports && Detect::of(path) != nullptr,
    };
}

std::string ListsBridge::uniqueName(const std::vector<NameEntry> &list, const std::string &base,
                                    const int ignoring) {
    std::string candidate = Text::trim(base);

    if (candidate.empty()) {
        candidate = "Unnamed";
    }

    const auto taken = [&list, ignoring](const std::string &name) {
        for (size_t index = 0; index < list.size(); ++index) {
            if (std::cmp_not_equal(index, ignoring) && Text::iequals(list[index].name, name)) {
                return true;
            }
        }

        return false;
    };

    if (!taken(candidate)) {
        return candidate;
    }

    for (int suffix = 2;; suffix++) {
        if (std::string numbered = candidate + " (" + std::to_string(suffix) + ")";
            !taken(numbered)) {
            return numbered;
        }
    }
}

void ListsBridge::push() const {
    State::Cfg &state = cfg();
    const Profile &profile = active();

    std::vector<State::FileRow> files;
    int enabled = 0;

    files.reserve(profile.files.size());

    for (size_t index = 0; index < profile.files.size(); ++index) {
        const FileEntry &entry = profile.files[index];
        const std::filesystem::path path(entry.file);

        if (entry.enabled) {
            ++enabled;
        }

        files.push_back(State::FileRow{
            .index = static_cast<int>(index),
            .file = entry.file,
            .name = path.filename().string(),
            .directory = Format::fromPath(path.parent_path()),
            .loaded = entry.enabled,
            .missing = missing(path),
        });
    }

    state.files = std::move(files);
    state.enabledCount = enabled;

    std::vector<State::NameRow> iwads;
    std::vector<State::NameRow> ports;
    std::vector<std::string> iwadNames;
    std::vector<std::string> portNames;
    std::vector<std::string> portBadges;

    for (size_t index = 0; index < config().iwads.size(); ++index) {
        iwads.push_back(rowOf(config().iwads, static_cast<int>(index), false));
        iwadNames.push_back(config().iwads[index].name);
    }

    for (size_t index = 0; index < config().ports.size(); ++index) {
        ports.push_back(rowOf(config().ports, static_cast<int>(index), true));
        portNames.push_back(config().ports[index].name);
        portBadges.emplace_back(config().ports[index].dosbox ? "DOS" : "");
    }

    state.iwads = std::move(iwads);
    state.ports = std::move(ports);
    state.iwadNames = std::move(iwadNames);
    state.portNames = std::move(portNames);
    state.portBadges = std::move(portBadges);

    _hub->library().pushShelf();
    _hub->library().pushGameRev();

    State::get().touch();
}

void ListsBridge::renamedIwad(const std::string &before, const std::string &after) const {
    for (Profile &each : config().profiles) {
        if (each.iwad == before) {
            each.iwad = after;
        }
    }

    _hub->profile().push();
    _hub->profile().touch();
}

void ListsBridge::renamedPort(const std::string &before, const std::string &after) const {
    for (Profile &each : config().profiles) {
        if (each.port == before) {
            each.port = after;
        }
    }

    if (config().general.gamePort == before) {
        config().general.gamePort = after;

        _hub->settings().push();
    }

    _hub->profile().push();
    _hub->profile().touch();
}

std::string ListsBridge::addPort(const std::string &file, const std::string &name,
                                 const bool dosbox) const {
    if (file.empty()) {
        return {};
    }

    const std::string chosen = uniqueName(config().ports,
                                          name.empty() ? FileInfo::describePort(file) : name);

    config().ports.push_back(NameEntry{.name = chosen, .file = file, .dosbox = dosbox});

    push();
    _hub->profile().push();
    _hub->profile().pushCommand();

    return chosen;
}

void ListsBridge::updatePort(const int row, const std::string &name, const std::string &file,
                             const bool dosbox) const {
    std::vector<NameEntry> &list = config().ports;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    NameEntry &entry = list[static_cast<size_t>(row)];
    const std::string before = entry.name;
    const std::string after = uniqueName(list,
                                         name.empty() ? FileInfo::describePort(file) : name, row);

    entry.name = after;
    entry.file = file;
    entry.dosbox = dosbox;

    push();

    if (before != after) {
        renamedPort(before, after);
    }

    _hub->profile().push();
    _hub->profile().pushCommand();
}

void ListsBridge::removePort(const int row) const {
    std::vector<NameEntry> &list = config().ports;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    list.erase(list.begin() + row);

    if (const std::string &chosen = config().general.gamePort;
        !chosen.empty() && config().findPort(chosen) == nullptr) {
        config().general.gamePort.clear();

        _hub->settings().push();
    }

    push();
    _hub->profile().push();
    _hub->profile().pushCommand();
}

void ListsBridge::movePort(const int from, const int to) const {
    moveTo(config().ports, from, to);

    push();
}

void ListsBridge::addFiles(const std::vector<std::string> &paths) const {
    for (const std::string &path : paths) {
        active().files.push_back(FileEntry{.file = path, .enabled = true});
    }

    push();
    _hub->profile().touch();
}

void ListsBridge::removeFile(const int row) const {
    std::vector<FileEntry> &files = active().files;

    if (row < 0 || std::cmp_greater_equal(row, files.size())) {
        return;
    }

    files.erase(files.begin() + row);

    push();
    _hub->profile().touch();
}

void ListsBridge::clearFiles() const {
    active().files.clear();

    push();
    _hub->profile().touch();
}

void ListsBridge::moveFile(const int from, const int to) const {
    moveTo(active().files, from, to);

    push();
    _hub->profile().touch();
}

void ListsBridge::setFileEnabled(const int row, const bool enabled) const {
    std::vector<FileEntry> &files = active().files;

    if (row < 0 || std::cmp_greater_equal(row, files.size())) {
        return;
    }

    files[static_cast<size_t>(row)].enabled = enabled;

    push();
    _hub->profile().touch();
}

void ListsBridge::addIwads(const std::vector<std::string> &paths) const {
    for (const std::string &file : paths) {
        if (file.empty()) {
            continue;
        }

        if (folder(file)) {
            _notifier->warning("A game has to be a file: a source port cannot be pointed "
                               "at a folder as one. " + file + " was left out.");

            continue;
        }

        config().iwads.push_back(NameEntry{
            .name = uniqueName(config().iwads, FileInfo::describeIwad(file)),
            .file = file,
        });
    }

    push();
    _hub->profile().touch();
}

void ListsBridge::updateIwad(const int row, const std::string &name,
                             const std::string &file) const {
    std::vector<NameEntry> &list = config().iwads;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    NameEntry &entry = list[static_cast<size_t>(row)];

    // Only a changed file is checked, so a legacy folder entry may stay.
    if (file != entry.file && folder(std::filesystem::path(file))) {
        _notifier->warning("A game has to be a file: a source port cannot be pointed at a "
                           "folder as one. " + entry.name + " is unchanged.");
        push();

        return;
    }

    const std::string before = entry.name;
    const std::string after = uniqueName(
        list, name.empty() ? FileInfo::describeIwad(std::filesystem::path(file)) : name, row);

    entry.name = after;
    entry.file = file;

    push();

    if (before != after) {
        renamedIwad(before, after);
    }

    _hub->profile().touch();
}

void ListsBridge::removeIwad(const int row) const {
    std::vector<NameEntry> &list = config().iwads;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    list.erase(list.begin() + row);

    push();
    _hub->profile().touch();
}

void ListsBridge::moveIwad(const int from, const int to) const {
    moveTo(config().iwads, from, to);

    push();
    _hub->profile().touch();
}
