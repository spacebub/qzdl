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
#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "ttk/dialogs/FilePicker.h"
#include "ttk/system/Text.h"
#include "ttk/util/Format.h"

#include "core/config/FileBrowser.h"
#include "gui/model/BrowserBridge.h"
#include "gui/model/ConfigBridge.h"
#include "gui/services/Filters.h"

using namespace ttk;

namespace {

// As Text::lower, without a copy of every name per keystroke.
char lowered(const char letter) {
    return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
}

}

const std::string &BrowserBridge::folderInEffect() {
    static const std::string none;

    if (!config().general.fileBrowser) {
        return none;
    }

    const std::string &own = active().fileBrowserFolder;

    return own.empty() ? config().general.fileBrowserFolder : own;
}

void BrowserBridge::push() {
    State::BrowserState &state = State::get().browser;
    const std::string &folder = folderInEffect();
    const bool open = config().general.fileBrowserOpen;
    const bool own = !active().fileBrowserFolder.empty();
    std::string shown = Format::from_path(folder);
    bool moved = open != state.open || own != state.ownFolder || shown != state.folder;

    state.open = open;
    state.ownFolder = own;
    state.folder = std::move(shown);

    // A folded band keeps the rows it slides away and lists the folder again when it opens.
    if (open && folder != _scanned) {
        scan(folder);
        moved = true;
    } else if (open) {
        moved = markPicks() || moved;
    }

    if (moved) {
        ++state.rev;
        State::get().touch();
    }
}

void BrowserBridge::scan(const std::string &folder) {
    State::BrowserState &state = State::get().browser;
    std::unordered_map<std::string, bool> folds;
    std::vector<FileBrowser::Entry> found = FileBrowser::scan(
            folder, [](const std::string_view name) { return FilePicker::passes(Filters::addon(), name); });
    std::vector<size_t> above;

    for (const State::BrowserRow &row : state.rows) {
        if (row.folder) {
            folds.emplace(row.path, row.folded);
        }
    }

    _scanned = folder;
    _keys.clear();
    ++state.listing;
    state.files = 0;
    state.rows.clear();
    state.rows.reserve(found.size());

    for (FileBrowser::Entry &entry : found) {
        for (; above.size() > entry.depth; above.pop_back()) {
            state.rows[above.back()].end = state.rows.size();
        }

        if (entry.folder) {
            above.push_back(state.rows.size());
        }

        state.files += entry.folder ? 0 : 1;
        _keys.push_back(entry.folder ? std::string() : fileKey(entry.path));
        state.rows.push_back(State::BrowserRow{
                .name = std::move(entry.name),
                .path = std::move(entry.path),
                .depth = entry.depth,
                .end = 0,
                .picks = 0,
                .folder = entry.folder,
                .folded = false,
        });
    }

    for (const size_t at : above) {
        state.rows[at].end = state.rows.size();
    }

    (void)markPicks();

    // A folder new to the listing opens when it holds a pick.
    for (State::BrowserRow &row : state.rows) {
        if (row.folder) {
            const auto known = folds.find(row.path);

            row.folded = known != folds.end() ? known->second : row.picks == 0;
        }
    }

    match();
}

bool BrowserBridge::markPicks() const {
    std::vector<State::BrowserRow> &rows = State::get().browser.rows;

    if (rows.empty()) {
        return false;
    }

    std::unordered_set<std::string> listed;
    std::vector<size_t> above;
    bool moved = false;

    for (const FileEntry &entry : active().files) {
        listed.insert(fileKey(entry.file));
    }

    for (size_t at = 0; at < rows.size(); ++at) {
        State::BrowserRow &row = rows[at];

        above.resize(row.depth);

        if (row.folder) {
            row.picks = 0;
            above.push_back(at);

            continue;
        }

        const int picked = listed.contains(_keys[at]) ? 1 : 0;

        moved = moved || row.picks != picked;
        row.picks = picked;

        for (const size_t folder : above) {
            rows[folder].picks += picked;
        }
    }

    return moved;
}

void BrowserBridge::match() {
    State::BrowserState &state = State::get().browser;
    const std::vector<State::BrowserRow> &rows = state.rows;
    const std::string needle = Text::lower(state.filter);
    std::vector<int> above;
    size_t listed = 0;

    state.shown.clear();

    for (size_t at = 0; at < rows.size(); ++at) {
        const State::BrowserRow &row = rows[at];

        if (needle.empty()) {
            state.shown.push_back(static_cast<int>(at));

            if (row.folder && row.folded) {
                at = row.end - 1;
            }

            continue;
        }

        // The folders above a match are listed with it, once.
        above.resize(row.depth);
        listed = std::min(listed, row.depth);

        if (row.folder) {
            above.push_back(static_cast<int>(at));
        } else if (!std::ranges::search(row.name, needle, {}, lowered).empty()) {
            state.shown.insert(state.shown.end(), above.begin() + static_cast<std::ptrdiff_t>(listed), above.end());
            state.shown.push_back(static_cast<int>(at));
            listed = above.size();
        }
    }
}

void BrowserBridge::setOpen(const bool open) {
    config().general.fileBrowserOpen = open;

    if (open) {
        _scanned.clear();
    }

    _hub->scheduleSave();
    push();
}

void BrowserBridge::setFilter(const std::string &value) {
    State::BrowserState &state = State::get().browser;

    state.filter = value;
    match();
    ++state.rev;
    State::get().touch();
}

void BrowserBridge::fold(const int row, const bool folded) {
    State::BrowserState &state = State::get().browser;

    state.rows[static_cast<size_t>(row)].folded = folded;
    match();
    ++state.rev;
    State::get().touch();
}

void BrowserBridge::setOwnFolder(const std::string &folder) {
    active().fileBrowserFolder = folder;

    _hub->scheduleSave();
    push();
}

void BrowserBridge::applyPicks(const std::vector<std::string> &paths) const {
    if (folderInEffect().empty() || !FileBrowser::pick(active().files, _keys, paths)) {
        return;
    }

    _hub->scheduleSave();
    _hub->lists().push();
    _hub->panels().pushSave();
    _hub->profile().touch();
}
