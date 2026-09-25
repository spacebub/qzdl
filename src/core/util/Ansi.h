/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <cstdint>
#include <string>
#include <string_view>

// Keeps only what a terminal would print in line: control sequences go, and so does
// text drawn while the cursor is saved away, such as a status bar redrawn at the
// screen edge.
class Ansi {
public:
    // Appends the printable part of chunk to out. State carries across chunks, so a
    // sequence split between two reads is still dropped.
    void filter(std::string_view chunk, std::string &out);

private:
    enum class Mode : std::uint8_t { Text, Escape, Csi, String, StringEnd, Charset };

    Mode _mode{Mode::Text};

    bool _aside{false};
};
