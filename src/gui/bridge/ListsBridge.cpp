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
#include "gui/Convert.h"
#include "gui/Models.h"
#include "gui/bridge/ConfigBridge.h"
#include "gui/bridge/ListsBridge.h"

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

ui::NameRow ListsBridge::rowOf(const std::vector<NameEntry> &list, const int index,
                              const bool ports) {
    const NameEntry &entry = list[static_cast<size_t>(index)];
    const std::filesystem::path path(entry.file);
    const std::string root = Convert::plain(Convert::fromPath(Catalog::directory()));

    return ui::NameRow{
        .index = index,
        .name = Convert::text(entry.name),
        .file = Convert::text(entry.file),
        .directory = Convert::fromPath(path.parent_path()),
        .kind = Convert::text(Text::lower(path.extension().string())),
        .missing = missing(path),
        .dosbox = entry.dosbox,
        .fetched = ports && !root.empty()
            && Convert::plain(Convert::fromPath(path)).starts_with(root + "/"),
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

void ListsBridge::push() {
    const ui::Cfg &state = cfg();
    const Profile &profile = active();

    std::vector<ui::FileRow> files;
    int enabled = 0;

    files.reserve(profile.files.size());

    for (size_t index = 0; index < profile.files.size(); ++index) {
        const FileEntry &entry = profile.files[index];
        const std::filesystem::path path(entry.file);

        if (entry.enabled) {
            ++enabled;
        }

        files.push_back(ui::FileRow{
            .index = static_cast<int>(index),
            .file = Convert::text(entry.file),
            .name = Convert::text(path.filename().string()),
            .directory = Convert::fromPath(path.parent_path()),
            .loaded = entry.enabled,
            .missing = missing(path),
        });
    }

    Models::reconcile(*_files, files);
    state.set_enabled_count(enabled);

    std::vector<ui::NameRow> iwads;
    std::vector<ui::NameRow> ports;
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

    Models::reconcile(*_iwads, iwads);
    Models::reconcile(*_ports, ports);
    state.set_iwad_names(Convert::strings(iwadNames));
    state.set_port_names(Convert::strings(portNames));
    state.set_port_badges(Convert::strings(portBadges));

    _hub->library().pushShelf();
    _hub->library().pushGameRev();
}

void ListsBridge::renamedIwad(const std::string &before, const std::string &after) {
    for (Profile &each : config().profiles) {
        if (each.iwad == before) {
            each.iwad = after;
        }
    }

    _hub->profile().push();
    _hub->profile().touch();
}

void ListsBridge::renamedPort(const std::string &before, const std::string &after) {
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
                                 const bool dosbox) {
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
                             const bool dosbox) {
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

void ListsBridge::removePort(const int row) {
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

void ListsBridge::bind() {
    const ui::Cfg &state = cfg();

    state.set_files(_files);
    state.set_iwads(_iwads);
    state.set_ports(_ports);

    state.on_index_of([](const std::shared_ptr<slint::Model<slint::SharedString>> &list,
                         const slint::SharedString &wanted) {
        for (size_t row = 0; row < list->row_count(); row++) {
            if (*list->row_data(row) == wanted) {
                return static_cast<int>(row);
            }
        }

        return -1;
    });

    state.on_describe_iwad([](const slint::SharedString &file) {
        return Convert::text(FileInfo::describeIwad(Convert::toPath(file)));
    });

    state.on_describe_port([](const slint::SharedString &file) {
        return Convert::text(FileInfo::describePort(Convert::toPath(file)));
    });

    state.on_add_files([this](const std::shared_ptr<slint::Model<slint::SharedString>> &paths) {
        for (size_t row = 0; row < paths->row_count(); row++) {
            active().files.push_back(FileEntry{
                .file = Convert::plain(*paths->row_data(row)),
                .enabled = true,
            });
        }

        push();
        _hub->profile().touch();
    });

    state.on_remove_file([this](const int row) {
        std::vector<FileEntry> &files = active().files;

        if (row < 0 || std::cmp_greater_equal(row, files.size())) {
            return;
        }

        files.erase(files.begin() + row);

        push();
        _hub->profile().touch();
    });

    state.on_clear_files([this] {
        active().files.clear();

        push();
        _hub->profile().touch();
    });

    state.on_move_file([this](const int from, const int to) {
        moveTo(active().files, from, to);

        push();
        _hub->profile().touch();
    });

    state.on_set_file_enabled([this](const int row, const bool enabled) {
        std::vector<FileEntry> &files = active().files;

        if (row < 0 || std::cmp_greater_equal(row, files.size())) {
            return;
        }

        files[static_cast<size_t>(row)].enabled = enabled;

        push();
        _hub->profile().touch();
    });

    state.on_add_iwads([this](const std::shared_ptr<slint::Model<slint::SharedString>> &paths) {
        for (size_t row = 0; row < paths->row_count(); row++) {
            const std::string file = Convert::plain(*paths->row_data(row));

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
    });

    state.on_update_iwad([this](const int row, const slint::SharedString &name,
                                const slint::SharedString &file) {
        std::vector<NameEntry> &list = config().iwads;

        if (row < 0 || std::cmp_greater_equal(row, list.size())) {
            return;
        }

        NameEntry &entry = list[static_cast<size_t>(row)];
        const std::string chosen = Convert::plain(file);

        // Only a changed file is checked, so a legacy folder entry may stay.
        if (chosen != entry.file && folder(Convert::toPath(file))) {
            _notifier->warning("A game has to be a file: a source port cannot be pointed at a "
                               "folder as one. " + entry.name + " is unchanged.");
            push();

            return;
        }

        const std::string wanted = Convert::plain(name);
        const std::string before = entry.name;
        const std::string after = uniqueName(list, wanted.empty()
            ? FileInfo::describeIwad(Convert::toPath(file))
            : wanted, row);

        entry.name = after;
        entry.file = chosen;

        push();

        if (before != after) {
            renamedIwad(before, after);
        }

        _hub->profile().touch();
    });

    state.on_remove_iwad([this](const int row) {
        std::vector<NameEntry> &list = config().iwads;

        if (row < 0 || std::cmp_greater_equal(row, list.size())) {
            return;
        }

        list.erase(list.begin() + row);

        push();
        _hub->profile().touch();
    });

    state.on_move_iwad([this](const int from, const int to) {
        moveTo(config().iwads, from, to);

        push();
        _hub->profile().touch();
    });

    state.on_add_port([this](const slint::SharedString &file, const bool dosbox) {
        addPort(Convert::plain(file), {}, dosbox);
    });

    state.on_update_port([this](const int row, const slint::SharedString &name,
                                const slint::SharedString &file, const bool dosbox) {
        updatePort(row, Convert::plain(name), Convert::plain(file), dosbox);
    });

    state.on_move_port([this](const int from, const int to) {
        moveTo(config().ports, from, to);

        push();
    });
}
