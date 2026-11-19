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
#pragma once

#include <cstddef>
#include <map>
#include <string>

#include "core/util/FileStamp.h"

// What was read from each file, read again once the file changes. Not thread safe.
template <typename Value, typename File = std::string>
class FileCache {
public:
    // The answer stays valid until the next call, which may empty the cache.
    template <typename Read>
    const Value &get(const File &file, const Read &read) {
        const FileStamp now = FileStamp::of(file);

        if (const auto found = _seen.find(file); found != _seen.end() && found->second.stamp == now) {
            return found->second.value;
        }

        if (_seen.size() >= KEPT) {
            _seen.clear();
        }

        return _seen.insert_or_assign(file, Entry{.stamp = now, .value = read()}).first->second.value;
    }

private:
    // A config may name any number of files over a session.
    static constexpr std::size_t KEPT = 512;

    struct Entry {
        FileStamp stamp;
        Value value;
    };

    std::map<File, Entry> _seen;
};
