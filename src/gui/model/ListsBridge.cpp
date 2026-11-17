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

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>

#include "ttk/system/Text.h"
#include "ttk/util/Format.h"

#include "core/launch/Dialect.h"
#include "core/launch/Storage.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "core/wad/FileInfo.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ListsBridge.h"

using namespace ttk;

namespace {

// Two spellings of one path compare equal. Windows ignores case in file names.
std::string fileKey(const std::string &path) {
    std::string key = std::filesystem::path(path).lexically_normal().string();

#ifdef _WIN32
    key = Text::lower(key);
#endif

    return key;
}

// Cached briefly: one list change stats every game and engine.
bool missing(const std::filesystem::path &path) {
    using Clock = std::chrono::steady_clock;

    struct Known {
        Clock::time_point asked;
        bool gone = false;
    };

    static constexpr std::chrono::seconds FRESH{2};
    static std::map<std::filesystem::path, Known> seen;

    const Clock::time_point now = Clock::now();

    if (const auto found = seen.find(path); found != seen.end() && now - found->second.asked < FRESH) {
        return found->second.gone;
    }

    // Every answer is stale after FRESH, so a session that opens many configs
    // would otherwise keep a path per file it ever named.
    std::erase_if(seen, [now](const auto &entry) { return now - entry.second.asked >= FRESH; });

    std::error_code code;
    const bool gone = !std::filesystem::exists(path, code);

    seen[path] = Known{.asked = now, .gone = gone};

    return gone;
}

bool folder(const std::filesystem::path &path) {
    std::error_code code;

    return std::filesystem::is_directory(path, code);
}

State::NameRow rowFor(const int index, const std::string &name, const std::string &file) {
    const std::filesystem::path path(file);

    return State::NameRow{
            .index = index,
            .name = name,
            .file = file,
            .directory = Format::from_path(path.parent_path()),
            .kind = Text::lower(path.extension().string()),
            .missing = missing(path),
            .dosbox = false,
            .managed = false,
            .detected = false,
            .needsVersion = false,
    };
}

// Profiles name games and engines, so a name stays unique within its list.
template <typename Entry>
std::string uniqueName(const std::vector<Entry> &list, const std::string &base, const int ignoring = -1) {
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
        if (std::string numbered = candidate + " (" + std::to_string(suffix) + ")"; !taken(numbered)) {
            return numbered;
        }
    }
}

}

Engine ListsBridge::engine(const int row) {
    const std::vector<Engine> &list = config().engines;

    return row >= 0 && std::cmp_less(row, list.size()) ? list[static_cast<size_t>(row)] : Engine{};
}

State::NameRow ListsBridge::rowOf(const std::vector<GameEntry> &list, const int index) {
    const GameEntry &entry = list[static_cast<size_t>(index)];

    return rowFor(index, entry.name, entry.file);
}

State::NameRow ListsBridge::rowOf(const std::vector<Engine> &list, const int index) {
    const Engine &engine = list[static_cast<size_t>(index)];
    State::NameRow row = rowFor(index, engine.name, engine.file);

    row.dosbox = engine.dosbox;
    row.managed = engine.managed;
    row.detected = Detect::of(engine.file) != nullptr;
    row.needsVersion = Dialect::needsVersion(engine);

    return row;
}

void ListsBridge::push() const {
    State::Cfg &state = cfg();
    Profile &profile = active();

    // The line reads the same mark, so a row shown missing is never on it.
    Storage::markGoneAddons(profile);

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
                .directory = Format::from_path(path.parent_path()),
                .loaded = entry.enabled,
                .missing = path.is_absolute() ? entry.gone : missing(path),
        });
    }

    if (files != state.files) {
        state.files = std::move(files);
        ++state.filesRev;
    }

    state.enabledCount = enabled;

    std::vector<State::NameRow> iwads;
    std::vector<State::NameRow> engines;
    std::vector<std::string> iwadNames;
    std::vector<std::string> engineNames;
    std::vector<std::string> engineBadges;

    for (size_t index = 0; index < config().iwads.size(); ++index) {
        iwads.push_back(rowOf(config().iwads, static_cast<int>(index)));
        iwadNames.push_back(config().iwads[index].name);
    }

    for (size_t index = 0; index < config().engines.size(); ++index) {
        engines.push_back(rowOf(config().engines, static_cast<int>(index)));
        engineNames.push_back(config().engines[index].name);
        engineBadges.emplace_back(config().engines[index].dosbox ? "DOS" : "");
    }

    state.iwads = std::move(iwads);
    state.engines = std::move(engines);
    state.iwadNames = std::move(iwadNames);
    state.engineNames = std::move(engineNames);
    state.engineBadges = std::move(engineBadges);

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

    _hub->scheduleSave();
    _hub->profile().push();
    _hub->library().pushShelf();
    _hub->profile().touch();
}

void ListsBridge::renamedEngine(const std::string &before, const std::string &after) const {
    for (Profile &each : config().profiles) {
        if (each.engine == before) {
            each.engine = after;
        }
    }

    if (config().general.gameEngine == before) {
        config().general.gameEngine = after;

        _hub->settings().push();
    }

    _hub->scheduleSave();
    _hub->profile().push();
    _hub->library().pushShelf();
    _hub->profile().touch();
}

std::string ListsBridge::addEngine(const Engine &engine) const {
    if (engine.file.empty()) {
        return {};
    }

    const std::string chosen =
            uniqueName(config().engines, engine.name.empty() ? FileInfo::describePort(engine.file) : engine.name);

    config().engines.push_back(engine);
    config().engines.back().name = chosen;

    _hub->scheduleSave();
    push();
    _hub->profile().push();
    _hub->profile().pushCommand();

    return chosen;
}

void ListsBridge::updateEngine(const int row, const Engine &edited) const {
    std::vector<Engine> &list = config().engines;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    Engine &engine = list[static_cast<size_t>(row)];
    const std::string before = engine.name;
    const std::string after =
            uniqueName(list, edited.name.empty() ? FileInfo::describePort(edited.file) : edited.name, row);

    engine.name = after;
    engine.file = edited.file;
    engine.dosbox = edited.dosbox;
    engine.port = edited.port;
    engine.version = edited.version;

    _hub->scheduleSave();
    push();

    if (before != after) {
        renamedEngine(before, after);
    }

    _hub->profile().push();
    _hub->profile().pushCommand();
    _hub->profile().pushCards();
}

void ListsBridge::removeEngine(const int row) const {
    std::vector<Engine> &list = config().engines;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    const std::string name = list[static_cast<size_t>(row)].name;

    list.erase(list.begin() + row);

    for (Profile &each : config().profiles) {
        if (each.engine == name) {
            each.engine.clear();
        }
    }

    if (const std::string &chosen = config().general.gameEngine;
        !chosen.empty() && config().findEngine(chosen) == nullptr) {
        config().general.gameEngine.clear();

        _hub->settings().push();
    }

    _hub->scheduleSave();
    push();
    _hub->profile().push();
    _hub->library().pushShelf();
    _hub->profile().touch();
}

void ListsBridge::moveEngine(const int from, const int to) const {
    moveTo(config().engines, from, to);

    _hub->scheduleSave();
    push();
}

void ListsBridge::addFiles(const std::vector<std::string> &paths) const {
    insertFiles(paths, static_cast<int>(active().files.size()));
}

void ListsBridge::insertFiles(const std::vector<std::string> &paths, const int at) const {
    std::vector<FileEntry> &files = active().files;
    const std::size_t place = std::min(static_cast<std::size_t>(std::max(at, 0)), files.size());
    std::vector<FileEntry> added;
    std::unordered_set<std::string> known;

    added.reserve(paths.size());

    for (const FileEntry &entry : files) {
        known.insert(fileKey(entry.file));
    }

    for (const std::string &path : paths) {
        if (known.insert(fileKey(path)).second) {
            added.push_back(FileEntry{.file = path, .enabled = true});
        }
    }

    if (added.empty()) {
        return;
    }

    const auto where = files.begin() + static_cast<std::ptrdiff_t>(place);

    files.insert(where, std::make_move_iterator(added.begin()), std::make_move_iterator(added.end()));

    _hub->scheduleSave();
    push();
    _hub->panels().pushSave();
    _hub->profile().touch();
}

void ListsBridge::removeFile(const int row) const {
    std::vector<FileEntry> &files = active().files;

    if (row < 0 || std::cmp_greater_equal(row, files.size())) {
        return;
    }

    files.erase(files.begin() + row);

    _hub->scheduleSave();
    push();
    _hub->panels().pushSave();
    _hub->profile().touch();
}

void ListsBridge::clearFiles() const {
    active().files.clear();

    _hub->scheduleSave();
    push();
    _hub->panels().pushSave();
    _hub->profile().touch();
}

void ListsBridge::moveFile(const int from, const int to) const {
    moveTo(active().files, from, to);

    _hub->scheduleSave();
    push();
    _hub->panels().pushSave();
    _hub->profile().touch();
}

void ListsBridge::setFileEnabled(const int row, const bool enabled) const {
    std::vector<FileEntry> &files = active().files;

    if (row < 0 || std::cmp_greater_equal(row, files.size())) {
        return;
    }

    files[static_cast<size_t>(row)].enabled = enabled;

    _hub->scheduleSave();
    push();
    _hub->panels().pushSave();
    _hub->profile().touch();
}

void ListsBridge::addIwads(const std::vector<std::string> &paths) const {
    for (const std::string &file : paths) {
        if (file.empty()) {
            continue;
        }

        if (folder(file)) {
            _notifier->warning("A game has to be a file: a source port cannot be pointed "
                               "at a folder as one. "
                               + file + " was left out.");

            continue;
        }

        config().iwads.push_back(GameEntry{
                .name = uniqueName(config().iwads, FileInfo::describeIwad(file)),
                .file = file,
        });
    }

    _hub->scheduleSave();
    push();
    _hub->profile().touch();
}

void ListsBridge::updateIwad(const int row, const std::string &name, const std::string &file) const {
    std::vector<GameEntry> &list = config().iwads;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    GameEntry &entry = list[static_cast<size_t>(row)];

    // Only a changed file is checked, so a legacy folder entry may stay.
    if (file != entry.file && folder(std::filesystem::path(file))) {
        _notifier->warning("A game has to be a file: a source port cannot be pointed at a "
                           "folder as one. "
                           + entry.name + " is unchanged.");
        push();

        return;
    }

    const std::string before = entry.name;
    const std::string after =
            uniqueName(list, name.empty() ? FileInfo::describeIwad(std::filesystem::path(file)) : name, row);

    entry.name = after;
    entry.file = file;

    _hub->scheduleSave();
    push();

    if (before != after) {
        renamedIwad(before, after);
    }

    _hub->panels().pushSave();
    _hub->profile().touch();
}

void ListsBridge::removeIwad(const int row) const {
    std::vector<GameEntry> &list = config().iwads;

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    const std::string name = list[static_cast<size_t>(row)].name;

    list.erase(list.begin() + row);

    for (Profile &each : config().profiles) {
        if (each.iwad == name) {
            each.iwad.clear();
        }
    }

    _hub->scheduleSave();
    push();
    _hub->profile().push();
    _hub->library().pushShelf();
    _hub->profile().touch();
}

void ListsBridge::moveIwad(const int from, const int to) const {
    moveTo(config().iwads, from, to);

    _hub->scheduleSave();
    push();
    _hub->profile().touch();
}
