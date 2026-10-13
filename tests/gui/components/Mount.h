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
#include <cstdlib>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <blend2d/blend2d.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/Widget.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"

#include "core/config/Config.h"
#include "support/Canvas.h"
#include "support/Fixtures.h"
#include "support/Rig.h"

namespace parts {

// Holds its child at a fixed box on the page, so a component is hit, focused and painted through the root.
class Holder final : public ttk::Widget {
public:
    BLRect spot{};

    void arrange(ttk::Typeface &type) override {
        for (const Ptr &child : children()) {
            child->place(spot, type);
        }
    }
};

template <typename Kind>
Kind *place(tests::Canvas &canvas, std::unique_ptr<Kind> widget, const BLRect &spot) {
    canvas.bare();

    auto holder = std::make_unique<Holder>();

    holder->spot = spot;

    Kind *raw = holder->append(std::move(widget));

    canvas.ui().content()->add(std::move(holder));
    canvas.ui().relayout();
    canvas.frame();

    return raw;
}

inline BLPoint middle(const BLRect &box) {
    return BLPoint{box.x + (box.w / 2.0), box.y + (box.h / 2.0)};
}

inline void click(ttk::Root &root, const double x, const double y, const bool shift = false) {
    root.motion(x, y);
    root.press(ttk::Pointer{.x = x, .y = y, .shift = shift});
    root.release(ttk::Pointer{.x = x, .y = y});
}

inline void click(ttk::Root &root, const ttk::Widget &widget) {
    const BLPoint at = middle(widget.box());

    click(root, at.x, at.y);
}

// On the switch at its left end: a toggle laid across a row answers only as far as its words reach.
inline void flip(ttk::Root &root, const ttk::Widget &toggle) {
    click(root, toggle.box().x + 19.0, toggle.box().y + (toggle.box().h / 2.0));
}

inline void sweep(ttk::Root &root, const double fromX, const double fromY, const double toX, const double toY) {
    root.motion(fromX, fromY);
    root.press(ttk::Pointer{.x = fromX, .y = fromY});
    root.motion(toX, toY);
    root.release(ttk::Pointer{.x = toX, .y = toY});
}

inline ttk::Key key(const int code, const bool ctrl = false, const bool shift = false) {
    return ttk::Key{.code = code, .ctrl = ctrl, .shift = shift};
}

inline int channel(const std::uint32_t pixel, const unsigned shift) {
    return static_cast<int>((pixel >> shift) & 0xffU);
}

inline int distance(const std::uint32_t one, const std::uint32_t two) {
    int total = 0;

    for (unsigned shift = 0; shift < 24; shift += 8) {
        total += std::abs(channel(one, shift) - channel(two, shift));
    }

    return total;
}

// Whether `pixel` is closer to `ink` than to `ground`, for edges that antialiasing blends.
inline bool leans(const std::uint32_t pixel, const BLRgba32 ink, const BLRgba32 ground) {
    return distance(pixel, ink.value) < distance(pixel, ground.value);
}

inline int count_near(const tests::Canvas &canvas, const BLRectI &area, const BLRgba32 ink, const int slack = 24) {
    int found = 0;

    for (int y = area.y; y < area.y + area.h; ++y) {
        for (int x = area.x; x < area.x + area.w; ++x) {
            if (distance(canvas.at(x, y), ink.value) <= slack) {
                ++found;
            }
        }
    }

    return found;
}

inline std::vector<std::uint32_t> pixels_in(const tests::Canvas &canvas, const BLRectI &area) {
    std::vector<std::uint32_t> out;

    for (int y = area.y; y < area.y + area.h; ++y) {
        for (int x = area.x; x < area.x + area.w; ++x) {
            out.push_back(canvas.at(x, y));
        }
    }

    return out;
}

// How many of two runs of pixels stand more than `slack` apart. A glyph drawn for the first time is
// stroked and after that laid from a mask, a level or two apart at its edges, which this lets pass.
inline int changed(const std::vector<std::uint32_t> &one, const std::vector<std::uint32_t> &two, const int slack = 24) {
    int found = 0;

    for (std::size_t at = 0; at < one.size() && at < two.size(); ++at) {
        found += distance(one[at], two[at]) > slack ? 1 : 0;
    }

    return found;
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
            if (distance(canvas.at(x, y), ground) > 6) {
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

// The pixel of `region` that stands farthest from `ground`, the fullest ink of thin text.
inline std::uint32_t peak(const tests::Canvas &canvas, const BLRectI &region, const std::uint32_t ground) {
    std::uint32_t found = ground;

    for (int y = region.y; y < region.y + region.h; ++y) {
        for (int x = region.x; x < region.x + region.w; ++x) {
            if (distance(canvas.at(x, y), ground) > distance(found, ground)) {
                found = canvas.at(x, y);
            }
        }
    }

    return found;
}

// Where the pixels standing in `region` differ from a fresh repaint of the whole window, which this paints.
inline Ink mismatch(tests::Canvas &canvas, const BLRectI &region) {
    const std::vector<std::uint32_t> partial = canvas.pixels();

    canvas.full();

    const std::vector<std::uint32_t> whole = canvas.pixels();
    Ink found{.left = canvas.width(), .top = canvas.height(), .right = 0, .bottom = 0};

    for (int y = std::max(0, region.y); y < std::min(canvas.height(), region.y + region.h); ++y) {
        for (int x = std::max(0, region.x); x < std::min(canvas.width(), region.x + region.w); ++x) {
            const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(canvas.width()))
                                   + static_cast<std::size_t>(x);

            if (partial[at] != whole[at]) {
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

inline Ink mismatch(tests::Canvas &canvas) {
    return mismatch(canvas, BLRectI{0, 0, canvas.width(), canvas.height()});
}

inline std::ostream &operator<<(std::ostream &out, const Ink &found) {
    return out << found.pixels << " pixels in [" << found.left << ", " << found.top << ") to [" << found.right << ", "
               << found.bottom << ")";
}

inline BLRectI pixels_of(const BLRect &box) {
    return BLRectI{static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(box.w), static_cast<int>(box.h)};
}

template <typename Kind>
Kind *find(ttk::Widget *from, const std::function<bool(const Kind &)> &wanted = {}) {
    for (const ttk::Widget::Ptr &child : from->children()) {
        if (auto *made = dynamic_cast<Kind *>(child.get()); made != nullptr && (!wanted || wanted(*made))) {
            return made;
        }

        if (Kind *deeper = find<Kind>(child.get(), wanted); deeper != nullptr) {
            return deeper;
        }
    }

    return nullptr;
}

template <typename Kind>
std::vector<Kind *> all(ttk::Widget *from) {
    std::vector<Kind *> out;

    for (const ttk::Widget::Ptr &child : from->children()) {
        if (auto *made = dynamic_cast<Kind *>(child.get()); made != nullptr) {
            out.push_back(made);
        }

        for (Kind *deeper : all<Kind>(child.get())) {
            out.push_back(deeper);
        }
    }

    return out;
}

inline ttk::Label *labelled(ttk::Widget *from, const std::string &text) {
    return find<ttk::Label>(from, [&text](const ttk::Label &label) { return label.text() == text; });
}

template <typename Kind>
Kind *captioned(ttk::Widget *from, const std::string &caption) {
    return find<Kind>(from,
                      [&caption](const Kind &made) { return labelled(const_cast<Kind *>(&made), caption) != nullptr; });
}

template <typename Kind>
Kind *hinted(ttk::Widget *from, const std::string &hint) {
    return find<Kind>(from, [&hint](const Kind &made) { return made.hint == hint; });
}

inline bool visibleToTheRoot(const ttk::Widget *widget) {
    for (const ttk::Widget *up = widget; up != nullptr; up = up->parent()) {
        if (!up->visible()) {
            return false;
        }
    }

    return true;
}

// Three profiles, the first of them on `program` and loading `files` add-ons.
inline Config onPort(const std::string &program, const int files = 3, const bool dos = false) {
    Config made = tests::Fixtures::config(3, files);

    made.general.showPaths = false;
    made.ports.front().file = "/ports/" + program;
    made.ports.front().dosbox = dos;

    return made;
}

inline void install(tests::Rig &rig, Config what) {
    tests::Fixtures::install(std::move(what));

    rig.config().reload();
    rig.shell().advance(0.0);
}

// The rig with what start-up posted and scheduled run, so nothing left over syncs the tree in the middle of a case.
// Attached before the first sync, which is how every sync after the first finds the tree.
class Settled : public tests::Rigged {
protected:
    void SetUp() override {
        tests::Rigged::SetUp();

        if (IsSkipped()) {
            return;
        }

        ttk::Theme::set_mode(ttk::Theme::Mode::Light);
        canvas().settle();
        rig().ready();

        // The command preview the bridge scheduled on construction is due at once, and only a turn runs timers.
        rig().turn();
    }
};

// A component alone on a screen of its own, reaching for the rig's services.
class Screened : public Settled {
protected:
    void SetUp() override {
        Settled::SetUp();

        if (IsSkipped()) {
            return;
        }

        _screen = std::make_unique<tests::Canvas>(wide, tall);
    }

    // Before the rig, which the component holds a reach into.
    void TearDown() override {
        _screen.reset();

        Settled::TearDown();
    }

    tests::Canvas &screen() { return *_screen; }

    ttk::Root &ui() { return _screen->ui(); }

    // A fixture that wants another size sets these in its constructor.
    int wide = 1100;
    int tall = 900;

    // Over whatever the field held, as a select-all and a paste would.
    void type(ttk::Field &field, const std::string &text) {
        ui().focus(field.input());
        ui().key(key('a', true));
        ui().wrote(text);
    }

private:
    std::unique_ptr<tests::Canvas> _screen;
};

}
