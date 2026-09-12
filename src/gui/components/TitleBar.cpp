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
#include <utility>

#include "gui/components/TitleBar.h"
#include "gui/draw/Mark.h"
#include "gui/draw/Typeface.h"
#include "gui/model/State.h"
#include "gui/toolkit/controls/GlyphButton.h"

namespace {

std::string shadeHint() {
    const std::string &mode = Theme::mode();

    if (mode == "system") {
        return "Following the desktop. Click for the light theme";
    }

    return mode == "light" ? "Light theme. Click for the dark one"
                           : "Dark theme. Click to follow the desktop again";
}

}

namespace components {

using namespace toolkit;

TitleBar::TitleBar(Reach *reach) : _reach(reach) {
    _takesPointer = true;

    _tabs.emplace_back("library", "Library");
    _tabs.emplace_back("profile", "Profile");
    _tabs.emplace_back("engines", "Engines");
    _tabs.emplace_back("settings", "Settings");

    _mark = Mark::of(128);

    _shade = append(std::make_unique<GlyphButton>(Theme::mode(), [this] {
        _reach->cycleShade();
    }));

    _minimize = append(std::make_unique<GlyphButton>("minimize", [this] {
        _reach->shell.minimize();
    }));

    _maximize = append(std::make_unique<GlyphButton>("maximize", [this] {
        _reach->shell.toggleMaximize();
    }));

    _close = append(std::make_unique<GlyphButton>("close", [this] {
        _reach->shell.stop();
    }));

    _close->tone(Theme::of().muted, BLRgba32(0xffffffff));
}

void TitleBar::sync() {
    const std::string &page = State::get().sys.page;

    for (Tab &tab : _tabs) {
        const bool active = tab.key == page;

        if ((tab.on.value() > 0.5) != active) {
            tab.on.toward(active ? 1.0F : 0.0F, now(), 0.14, Anim::Curve::CubicOut);

            animate();
        }
    }

    _tabs[2].badge = State::get().cfg.ports.empty();

    // The shade button cycles system, light and dark, and says which it is on.
    _shade->glyph(Theme::mode());
    _shade->tip(shadeHint());

    _maximize->glyph(_reach->shell.maximized() ? "restore" : "maximize");

    invalidate();
}

void TitleBar::arrange(Typeface &type) {
    const BLFont &face = type.at(400, Theme::fontBody);
    const bool compact = _box.w < 860.0;

    // The brand.
    _brandEnd = 16.0 + 22.0 + (compact ? 0.0 : 9.0 + type.width(face, "ZDL4"));

    // The controls, at the far edge.
    constexpr double side = Theme::controlSmall;
    double x = _box.x + _box.w - 8.0 - side;

    _close->place(BLRect{x, _box.y + ((Theme::barHeight - side) / 2.0), side, side}, type);

    x -= side + 2.0;
    _maximize->place(BLRect{x, _box.y + ((Theme::barHeight - side) / 2.0), side, side}, type);

    x -= side + 2.0;
    _minimize->place(BLRect{x, _box.y + ((Theme::barHeight - side) / 2.0), side, side}, type);

    x -= side + 8.0;
    _shade->place(BLRect{x, _box.y + ((Theme::barHeight - side) / 2.0), side, side}, type);

    // The tabs, over the middle or beside the name when the window is narrow.
    double total = 0.0;

    for (Tab &tab : _tabs) {
        tab.width = type.width(face, tab.label);
        total += tab.width + 30.0 + 2.0;
    }

    total = std::max(0.0, total - 2.0);

    double at = std::max(_box.x + _brandEnd + 22.0,
                         std::min(_box.x + ((_box.w - total) / 2.0), x - total - 16.0));

    for (Tab &tab : _tabs) {
        tab.box = BLRect{at, _box.y, tab.width + 30.0, Theme::barHeight};

        at += tab.box.w + 2.0;
    }
}

bool TitleBar::draggable(const double x, const double y) const {
    if (y >= _box.y + Theme::barHeight) {
        return false;
    }

    for (const Tab &tab : _tabs) {
        if (x >= tab.box.x && x < tab.box.x + tab.box.w) {
            return false;
        }
    }

    return x < _shade->box().x;
}

void TitleBar::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const BLRect bar{_box.x, _box.y, _box.w, Theme::barHeight};

    painter.fill(bar, palette.background);
    painter.fill(BLRect{bar.x, bar.y + bar.h - 1.0, bar.w, 1.0}, palette.border);

    if (!_mark.is_empty()) {
        painter.context().blit_image(BLRect{bar.x + 16.0, bar.y + ((bar.h - 22.0) / 2.0), 22.0,
                                            22.0},
                                     _mark);
    }

    if (_box.w >= 860.0) {
        painter.label(painter.font(Typeface::semibold, Theme::fontBody),
                      BLRect{bar.x + 16.0 + 22.0 + 9.0, bar.y, 80.0, bar.h}, Align::Start,
                      "ZDL4", palette.text);
    }

    for (size_t index = 0; index < _tabs.size(); ++index) {
        const Tab &tab = _tabs[index];
        const double on = tab.on.value();
        const bool lit = std::cmp_equal(index, _over);

        painter.label(painter.font(on > 0.5 ? 600 : 400, Theme::fontBody), tab.box, Align::Centre,
                      tab.label, on > 0.5 || lit ? palette.text : palette.faint);

        if (on > 0.0) {
            const double wide = tab.width + 12.0;

            painter.round(BLRect{tab.box.x + ((tab.box.w - wide) / 2.0),
                                 tab.box.y + tab.box.h - 2.0, wide, 2.0},
                          1.0, Theme::alpha(palette.accent, on));
        }

        if (tab.badge && on <= 0.5) {
            painter.circle(BLPoint{tab.box.x + tab.box.w - 11.0, tab.box.y + 16.0}, 3.0,
                           palette.accent);
        }
    }

    Widget::paint(painter);
}

bool TitleBar::press(const Pointer &at) {
    return at.y < _box.y + Theme::barHeight && !draggable(at.x, at.y);
}

void TitleBar::release(const Pointer &at) {
    for (const Tab &tab : _tabs) {
        if (at.x >= tab.box.x && at.x < tab.box.x + tab.box.w && at.y >= tab.box.y
            && at.y < tab.box.y + tab.box.h) {
            _reach->go(tab.key);

            return;
        }
    }
}

void TitleBar::hover(const Pointer &at) {
    int over = -1;

    for (size_t index = 0; index < _tabs.size(); ++index) {
        const BLRect &box = _tabs[index].box;

        if (at.x >= box.x && at.x < box.x + box.w && at.y >= box.y && at.y < box.y + box.h) {
            over = static_cast<int>(index);
        }
    }

    if (over != _over) {
        _over = over;

        invalidate();
    }
}

void TitleBar::leave() {
    Widget::leave();

    _over = -1;
}

bool TitleBar::advance(const double now) {
    bool live = false;

    for (Tab &tab : _tabs) {
        tab.on.advance(now);
        tab.lit.advance(now);

        live = live || tab.on.live() || tab.lit.live();
    }

    invalidate(BLRect{_box.x, _box.y, _box.w, Theme::barHeight});

    return live;
}

}
