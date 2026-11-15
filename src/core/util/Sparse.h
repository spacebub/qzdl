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

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

// Text for the few keys of an enum that are set, in key order. A key left out reads as empty.
template <typename Key>
class Sparse {
public:
    using Entry = std::pair<Key, std::string>;

    [[nodiscard]] const std::string &get(const Key key) const {
        const auto found = std::ranges::lower_bound(_set, key, {}, &Entry::first);

        return found != _set.end() && found->first == key ? found->second : none();
    }

    // Empty text drops the key.
    void set(const Key key, std::string text) {
        const auto found = std::ranges::lower_bound(_set, key, {}, &Entry::first);
        const bool held = found != _set.end() && found->first == key;

        if (text.empty()) {
            if (held) {
                _set.erase(found);
            }
        } else if (held) {
            found->second = std::move(text);
        } else {
            _set.emplace(found, key, std::move(text));
        }
    }

    [[nodiscard]] const std::vector<Entry> &entries() const { return _set; }

    [[nodiscard]] bool empty() const { return _set.empty(); }

    void clear() { _set.clear(); }

    friend bool operator==(const Sparse &, const Sparse &) = default;

private:
    static const std::string &none() {
        static const std::string empty;

        return empty;
    }

    std::vector<Entry> _set;
};
