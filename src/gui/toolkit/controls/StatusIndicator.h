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

#include <functional>
#include <string>

#include "gui/toolkit/Widget.h"

namespace toolkit {

// A pill saying what a launched port is doing: a word in its own tone on a dark
// tile, with a dot that beats while the run is still settling either way.
//
// Mirrors State::RunState; the toolkit cannot see the state tree, so whoever
// fills one in maps the two.
class StatusIndicator : public Widget {
public:
    enum class Status : std::uint8_t {
        Empty,
        Launching,
        Running,
        Stopping,
        Closed,
        Failed,
    };

    static constexpr double HEIGHT = 24.0;

    // For a face painted whole, with no room for a widget in it: the same pill,
    // drawn from its top-left corner and answering where it landed. `dim` is the
    // far half of the beat.
    static BLRect render(const Painter &painter, BLPoint at, Status status, bool dim);

    static double widthOf(Typeface &type, Status status);

    [[nodiscard]] static bool beats(Status status);

    // The sentence behind the word. A failure says `reason` instead, when it has one.
    [[nodiscard]] static std::string sayOf(Status status, const std::string &reason);

    StatusIndicator() = default;

    void set(Status status, std::string reason = {});

    // Gives the pill the pointer, and the sentence a line about what a click does.
    StatusIndicator *onClick(std::function<void()> clicked, std::string about);

    [[nodiscard]] Status status() const { return _status; }

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface & /*type*/, double /*width*/) override { return HEIGHT; }

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &at) override;

    bool advance(double now) override;

private:
    void retell();

    Status _status = Status::Empty;
    std::string _reason;

    std::function<void()> _clicked;
    std::string _about;

    double _blinked = 0.0;
    bool _dim = false;
};

}
