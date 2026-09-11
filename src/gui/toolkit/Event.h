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

#include <cstdint>
#include <string>

namespace toolkit {

enum class Click : std::uint8_t {
    Left,
    Middle,
    Right,
    Back,
    Forward,
};

struct Pointer {
    double x = 0.0;
    double y = 0.0;
    Click button = Click::Left;

    bool ctrl = false;
    bool shift = false;
};

// SDL keycodes are passed through; only the ones the interface acts on are named.
namespace Code {

inline constexpr int Escape = 27;
inline constexpr int Return = 13;
inline constexpr int Tab = 9;
inline constexpr int Backspace = 8;
inline constexpr int Delete = 0x4000004C;
inline constexpr int Left = 0x40000050;
inline constexpr int Right = 0x4000004F;
inline constexpr int Up = 0x40000052;
inline constexpr int Down = 0x40000051;
inline constexpr int Home = 0x4000004A;
inline constexpr int End = 0x4000004D;
inline constexpr int PageUp = 0x4000004B;
inline constexpr int PageDown = 0x4000004E;
inline constexpr int F1 = 0x4000003A;

}

// What the pointer turns into over a widget.
enum class Cursor : std::uint8_t {
    Default,
    Pointer,
    Text,
    Resize,
    Grab,
    Grabbing,
};

struct Key {
    int code = 0;

    // Empty for a key that produces no text.
    std::string text;

    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

}
