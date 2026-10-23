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
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <blend2d/blend2d.h>

#include "ttk/toolkit/Widget.h"
#include "ttk/toolkit/controls/Label.h"

#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Rig.h"

namespace pgo {

// What a stretch of driving added up to.
struct Tally {
    std::size_t frames = 0;
    std::size_t pixels = 0;
    std::vector<std::string> failed;
};

// The interface clocked at a display's rate, with the per-turn work App::run does
// around a frame: sync when touched, point the tips, hand the toasts their messages.
class Drive {
public:
    Drive(int width, int height, double hz);
    ~Drive();

    Drive(const Drive &) = delete;
    Drive &operator=(const Drive &) = delete;
    Drive(Drive &&) = delete;
    Drive &operator=(Drive &&) = delete;

    tests::Rig &rig() { return *_rig; }
    tests::Canvas &canvas() { return *_canvas; }
    ttk::Root &ui() { return _canvas->ui(); }

    [[nodiscard]] double hz() const { return _hz; }

    // One turn of the loop and one frame.
    std::size_t turn();

    // Turns until the clock has moved `seconds`.
    std::size_t turnFor(double seconds);

    // Frames while anything animates, up to `most`.
    std::size_t rest(int most = 120);

    // Frames with nothing happening.
    std::size_t idle(int frames);

    // Pointer and keyboard, each followed by a frame as the loop would.
    void move(double x, double y);
    void click(double x, double y);
    void click(const ttk::Widget *widget);
    void drag(double fromX, double fromY, double toX, double toY, int steps);
    void wheel(double steps, double x, double y);
    void type(const std::string &text);
    void key(int code, bool ctrl = false, bool shift = false);

    // Clicks the title bar's tab for `page`.
    void tab(State::Page page);

    // Turns the loop for up to `seconds` of wall time until `done`.
    bool until(const std::function<bool()> &done, double seconds = 5.0);

    void check(bool ok, const std::string &what);

    Tally tally;

private:
    void point();

    std::unique_ptr<tests::Canvas> _canvas;
    std::unique_ptr<tests::Rig> _rig;
    double _hz;
};

template <typename Kind>
void gather(ttk::Widget *from, std::vector<Kind *> &out) {
    if (from == nullptr) {
        return;
    }

    if (auto *hit = dynamic_cast<Kind *>(from); hit != nullptr) {
        out.push_back(hit);
    }

    for (const ttk::Widget::Ptr &child : from->children()) {
        gather(child.get(), out);
    }
}

template <typename Kind>
std::vector<Kind *> all(ttk::Widget *from) {
    std::vector<Kind *> out;

    gather(from, out);

    return out;
}

template <typename Kind>
Kind *hinted(ttk::Widget *from, const std::string &hint) {
    for (Kind *each : all<Kind>(from)) {
        if (each->hint.starts_with(hint)) {
            return each;
        }
    }

    return nullptr;
}

// The first widget of its kind with a label reading `caption` inside it.
template <typename Kind>
Kind *captioned(ttk::Widget *from, const std::string &caption) {
    for (Kind *each : all<Kind>(from)) {
        for (const ttk::Label *label : all<ttk::Label>(each)) {
            if (label->text() == caption) {
                return each;
            }
        }
    }

    return nullptr;
}

inline BLPoint middle(const BLRect &box) {
    return BLPoint{box.x + (box.w / 2.0), box.y + (box.h / 2.0)};
}

}
