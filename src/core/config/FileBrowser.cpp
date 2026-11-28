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
#include <cstdint>
#include <iterator>
#include <system_error>
#include <unordered_set>
#include <utility>

#include "ttk/system/Text.h"

#include "core/config/FileBrowser.h"

namespace fs = std::filesystem;

namespace FileBrowser {

namespace {

struct Walk {
    const std::function<bool(std::string_view name)> &wanted;
    std::size_t left;
    std::vector<Entry> out;
};

void walk(const fs::path &folder, const std::size_t depth, Walk &into) {
    std::vector<std::pair<std::string, Entry>> found;
    std::error_code code;

    for (auto at = fs::directory_iterator(folder, fs::directory_options::skip_permission_denied, code);
         !code && at != fs::directory_iterator() && into.left > 0;
         at.increment(code), --into.left) {
        std::error_code unread;
        std::string name = at->path().filename().string();
        const bool sub = !name.starts_with('.') && !at->is_symlink(unread) && at->is_directory(unread);

        if (sub || (at->is_regular_file(unread) && into.wanted(name))) {
            std::string key = ttk::Text::lower(name);

            found.emplace_back(std::move(key),
                               Entry{
                                       .path = at->path().generic_string(),
                                       .name = std::move(name),
                                       .depth = depth,
                                       .folder = sub,
                               });
        }
    }

    std::ranges::sort(found, [](const auto &left, const auto &right) {
        return left.second.folder != right.second.folder ? left.second.folder
                                                         : ttk::Text::natural_less(left.first, right.first);
    });

    for (auto &[key, entry] : found) {
        const std::size_t mark = into.out.size();

        into.out.push_back(std::move(entry));

        if (into.out[mark].folder) {
            walk(fs::path(into.out[mark].path), depth + 1, into);

            if (into.out.size() == mark + 1) {
                into.out.pop_back();
            }
        }
    }
}

}

std::vector<Entry>
scan(const fs::path &folder, const std::function<bool(std::string_view name)> &wanted, const std::size_t cap) {
    Walk into{.wanted = wanted, .left = cap, .out = {}};

    walk(folder, 0, into);

    return std::move(into.out);
}

bool pick(std::vector<FileEntry> &files, const std::vector<std::string> &rows, const std::vector<std::string> &picks) {
    std::vector<std::string> keys;
    std::unordered_set<std::string> listed;
    std::unordered_set<std::string> wanted;
    std::vector<FileEntry> kept;
    std::size_t place = 0;
    std::uint16_t group = 0;

    for (const std::string &path : picks) {
        wanted.insert(fileKey(path));
    }

    keys.reserve(files.size());

    for (const FileEntry &entry : files) {
        keys.push_back(fileKey(entry.file));
    }

    // Matched against the few files of the list, so the rows of a large folder are not copied on every click.
    const std::unordered_set<std::string> held(keys.begin(), keys.end());

    for (const std::string &row : rows) {
        if (held.contains(row)) {
            listed.insert(row);
        }
    }

    kept.reserve(files.size() + picks.size());

    for (std::size_t at = 0; at < files.size(); ++at) {
        FileEntry &entry = files[at];
        const std::string &key = keys[at];

        if (listed.contains(key)) {
            const bool stays = wanted.erase(key) > 0;

            place = kept.size() + (stays ? 1 : 0);
            group = entry.group;

            if (!stays) {
                continue;
            }
        }

        kept.push_back(std::move(entry));
    }

    const bool removed = kept.size() < files.size();
    std::vector<FileEntry> added;

    for (const std::string &path : picks) {
        if (wanted.erase(fileKey(path)) > 0) {
            added.push_back(FileEntry{.file = path, .enabled = true, .gone = false, .group = group});
        }
    }

    kept.insert(kept.begin() + static_cast<std::ptrdiff_t>(place),
                std::make_move_iterator(added.begin()),
                std::make_move_iterator(added.end()));
    files = std::move(kept);

    return removed || !added.empty();
}

}
