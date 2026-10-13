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
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <blend2d/blend2d.h>
#include <gtest/gtest.h>

#include "ttk/dialogs/DialogLayer.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/Widget.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/TextView.h"
#include "ttk/toolkit/overlays/Dialog.h"

#include "gui/state/State.h"
#include "support/Canvas.h"

namespace modal {

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

// The first one whose tooltip is `hint`, or null.
template <typename Kind>
Kind *hinted(ttk::Widget *from, const std::string &hint) {
    for (Kind *each : all<Kind>(from)) {
        if (each->hint == hint) {
            return each;
        }
    }

    return nullptr;
}

inline std::vector<std::string> texts(ttk::Widget *from) {
    std::vector<std::string> out;

    for (const ttk::Label *label : all<ttk::Label>(from)) {
        out.push_back(label->text());
    }

    return out;
}

inline bool shows(ttk::Widget *from, const std::string &text) {
    const std::vector<std::string> said = texts(from);

    return std::ranges::find(said, text) != said.end();
}

// Everything a text view holds, by selecting it.
inline std::string read(ttk::TextView *view) {
    view->select_all();

    std::string text = view->selection();

    view->clear_selection();

    return text;
}

struct Ink {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    std::size_t pixels = 0;
};

// The bounds of every pixel in `region` that differs from `ground`.
inline Ink ink(const tests::Canvas &canvas, const BLRectI &region, const std::uint32_t ground) {
    Ink found{.left = region.x + region.w, .top = region.y + region.h, .right = region.x, .bottom = region.y};

    for (int y = region.y; y < region.y + region.h; ++y) {
        for (int x = region.x; x < region.x + region.w; ++x) {
            if (canvas.at(x, y) != ground) {
                found.left = std::min(found.left, x);
                found.top = std::min(found.top, y);
                found.right = std::max(found.right, x + 1);
                found.bottom = std::max(found.bottom, y + 1);
                ++found.pixels;
            }
        }
    }

    return found;
}

inline double midX(const BLRect &box) {
    return box.x + (box.w / 2.0);
}

inline double midY(const BLRect &box) {
    return box.y + (box.h / 2.0);
}

inline void click(ttk::Root &root, const double x, const double y) {
    root.motion(x, y);
    root.press(ttk::Pointer{.x = x, .y = y});
    root.release(ttk::Pointer{.x = x, .y = y});
}

inline void click(ttk::Root &root, const ttk::Widget *widget) {
    click(root, midX(widget->box()), midY(widget->box()));
}

// A dialog layer alone on a canvas, with each dialog shown on it opened fully and synced.
class Dialogued : public tests::Painted {
protected:
    void SetUp() override {
        tests::Painted::SetUp();

        if (IsSkipped()) {
            return;
        }

        State::get() = State::All{};

        canvas.frame();

        layer = canvas.ui().layer(ttk::Root::DIALOGS)->append(std::make_unique<ttk::DialogLayer>());

        canvas.ui().relayout();
        canvas.frame();
    }

    void TearDown() override {
        State::get() = State::All{};

        tests::Painted::TearDown();
    }

    template <typename Kind>
    Kind *open(std::unique_ptr<Kind> made) {
        Kind *raw = made.get();

        layer->show(std::move(made));
        layer->sync();

        canvas.frame();
        canvas.frameAt(canvas.now() + 1.0);

        return raw;
    }

    // What App does once a turn: the dialog on top reads the state again.
    void turn() {
        layer->sync();

        canvas.frameAt(canvas.now() + 0.1);
    }

    bool key(const int code) { return canvas.ui().key(ttk::Key{.code = code}); }

    tests::Canvas canvas{900, 700};
    ttk::DialogLayer *layer = nullptr;
};

}
