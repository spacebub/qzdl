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
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <string_view>

#include "core/launch/Patches.h"
#include "core/util/FileStamp.h"

namespace Patches {

namespace {

// Longer than any signature, so a longer line never matches one.
constexpr std::size_t LONGEST = 64;

struct FirstLine {
    FileStamp stamp;
    bool opened{false};
    std::string text;
};

FirstLine read(const std::string &file, const FileStamp &stamp) {
    FirstLine out{.stamp = stamp, .opened = false, .text = {}};
    std::ifstream in(file, std::ios::binary);

    if (!in) {
        return out;
    }

    out.opened = true;

    bool ended = false;
    bool whole = false;
    std::size_t taken = 0;

    // A skipped NUL counts toward the cap, so a file of them is not read to the end.
    for (char each = 0; !whole && taken <= LONGEST; ++taken) {
        if (!in.get(each)) {
            whole = true;
        } else if (each == '\n') {
            ended = true;
            whole = true;
        } else if (each != '\0') {
            // The port skips a NUL in a line.
            out.text.push_back(each);
        }
    }

    // Empty past the cap, which no signature matches.
    if (!whole) {
        out.text.clear();
    }

    if (ended && out.text.ends_with('\r')) {
        out.text.pop_back();
    }

    return out;
}

FirstLine firstLine(const std::string &file) {
    // Bounded like MapFile::maps.
    constexpr std::size_t KEPT = 512;

    static std::mutex guard;
    static std::map<std::string, FirstLine, std::less<>> seen;

    const FileStamp now = FileStamp::of(file);
    const std::scoped_lock hold(guard);

    if (const auto found = seen.find(file); found != seen.end() && found->second.stamp == now) {
        return found->second;
    }

    if (seen.size() >= KEPT) {
        seen.clear();
    }

    return seen.insert_or_assign(file, read(file, now)).first->second;
}

}

bool refused(const Dialect::Traits &traits, const std::string &file) {
    if (!traits.has(Dialect::Feature::strictPatchHeader)) {
        return false;
    }

    const FirstLine line = firstLine(file);
    const std::span<const std::string_view> lines = Dialect::signatures(traits.patchHeader);

    return line.opened && std::ranges::find(lines, line.text) == lines.end();
}

std::string warning(const Dialect::Traits &traits, const std::span<const std::string> names) {
    if (names.empty()) {
        return {};
    }

    std::string out;

    for (std::size_t at = 0; at < names.size(); ++at) {
        if (at > 0) {
            out += at + 1 == names.size() ? " and " : ", ";
        }

        out += names[at];
    }

    out += names.size() == 1 ? " is" : " are";
    out += " left out of the launch. The port quits on a patch unless its first line is ";
    out += Dialect::signaturesPhrase(traits.patchHeader);
    out += '.';

    return out;
}

}
