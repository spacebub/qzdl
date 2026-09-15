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
#include <vector>

#include "gui/components/AddonList.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Theme.h"
#include "gui/draw/Typeface.h"
#include "gui/state/State.h"
#include "gui/util/Format.h"

namespace {

// How long a dropped row takes to walk to its gap.
constexpr double SETTLING = 0.16;

}

namespace components {

using namespace toolkit;

AddonList::AddonList(Reach *reach) : _reach(reach) { _takesPointer = true; }

double AddonList::rowHeight() { return State::get().cfg.showPaths ? 46.0 : 34.0; }

Cursor AddonList::cursorAt(const double x, const double y) const {
    if (rowAt(y) < 0 || overLane(x)) {
        return Cursor::Default;
    }

    if (x < _box.x + 22.0) {
        return Cursor::Resize;
    }

    const bool acts = (x >= _box.x + 24.0 && x < _box.x + 42.0)
        || x >= _box.x + _box.w - 38.0;

    return acts ? Cursor::Pointer : Cursor::Default;
}

void AddonList::arrange(Typeface & /*type*/) {
    setReach(static_cast<double>(State::get().cfg.files.size()) * rowHeight());
}

void AddonList::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const std::vector<State::FileRow> &files = State::get().cfg.files;

    if (files.empty()) {
        const BLFont &heading = painter.font(600, Theme::fontMedium);
        const double wide = _box.w - 48.0;
        double y = _box.y + ((_box.h - 70.0) / 2.0);

        painter.label(heading, BLRect{_box.x + 24.0, y, wide, painter.lineHeight(heading)},
                      Align::Start, "Nothing loaded", palette.muted);

        y += painter.lineHeight(heading) + 6.0;

        painter.paragraph(painter.font(400, Theme::fontSmall),
                          BLRect{_box.x + 24.0, y, wide, 0.0},
                          "WADs, PK3s, DEH and BEX patches, demos and configs go here. They "
                          "are passed to the source port in the order they are listed.",
                          palette.faint);

        return;
    }

    painter.push(_box);

    const double step = rowHeight();
    const double wide = _box.w - (scrollable() ? Theme::lane : 0.0);

    for (size_t index = 0; index < files.size(); ++index) {
        const State::FileRow &row = files[index];
        const double shift = std::cmp_equal(index, _carrying) ? _carryY.value()
                                                                  : shiftOf(index, step);
        const BLRect line{_box.x, _box.y - offset() + (static_cast<double>(index) * step)
                                      + shift,
                          wide, step};

        if (!painter.needed(line)) {
            continue;
        }

        if (std::cmp_equal(index, _carrying)) {
            painter.round(line, Theme::radiusSmall - 2.0, palette.mutedSoft);
        } else if (std::cmp_equal(index, _over)) {
            painter.round(line, Theme::radiusSmall - 2.0, palette.hover);
        }

        const double grip = Glyphs::span(1.0F);

        Glyphs::draw(painter.context(), Glyphs::Glyph::Grip,
                     BLPoint{line.x + ((22.0 - grip) / 2.0), line.y + ((line.h - grip) / 2.0)},
                     1.0F,
                     std::cmp_equal(index, _overGrip) ? palette.muted : palette.border);

        const BLRect check{line.x + 24.0, line.y + ((line.h - 18.0) / 2.0), 18.0, 18.0};

        painter.round(check, 5.0, row.loaded ? palette.accent : palette.field);
        painter.outline(check, 5.0, 1.0, row.loaded ? palette.accent : palette.border);

        if (row.loaded) {
            const double tick = Glyphs::span(0.85F);

            Glyphs::draw(painter.context(), Glyphs::Glyph::Check,
                         BLPoint{check.x + ((check.w - tick) / 2.0),
                                 check.y + ((check.h - tick) / 2.0)},
                         0.85F, palette.accentText);
        }

        const double left = line.x + 52.0;
        const double right = line.x + line.w - 38.0;

        const BLFont &face = painter.font(400, Theme::fontBody);
        const double taken = std::min(painter.width(face, row.name), right - left - 60.0);

        painter.label(face,
                      BLRect{left, State::get().cfg.showPaths ? line.y + 6.0 : line.y,
                             right - left, State::get().cfg.showPaths ? 20.0 : line.h},
                      Align::Start, row.name,
                      row.missing  ? palette.danger
                      : row.loaded ? palette.text
                                   : palette.faint);

        if (!row.loaded) {
            // Blend2D strikes nothing through. The line is drawn.
            const double middle = (State::get().cfg.showPaths ? line.y + 16.0
                                                              : line.y + (line.h / 2.0));

            painter.fill(BLRect{left, middle, taken, 1.0}, palette.faint);
        }

        if (row.missing) {
            painter.label(painter.font(600, Theme::fontTiny),
                          BLRect{left + taken + 7.0,
                                 State::get().cfg.showPaths ? line.y + 6.0 : line.y, 60.0,
                                 State::get().cfg.showPaths ? 20.0 : line.h},
                          Align::Start, "missing", palette.danger);
        }

        if (State::get().cfg.showPaths) {
            const BLFont &mono = painter.font(Typeface::mono, Theme::fontTiny);
            const double unit = painter.width(mono, "M");
            const int room = unit > 0.0 ? static_cast<int>((right - left) / unit) : 0;

            painter.label(mono, BLRect{left, line.y + 25.0, right - left, 16.0}, Align::Start,
                          Format::fitPath(row.directory, room), palette.faint);
        }

        if (std::cmp_equal(index, _over)) {
            const double cross = Glyphs::span(1.2F);

            Glyphs::draw(painter.context(), Glyphs::Glyph::Close,
                         BLPoint{line.x + line.w - 32.0 + ((26.0 - cross) / 2.0),
                                 line.y + ((line.h - cross) / 2.0)},
                         1.2F, _overShut ? palette.danger : palette.muted);
        }
    }

    painter.pop();

    Scroll::paint(painter);
}

void AddonList::hover(const Pointer &at) {
    const int row = overLane(at.x) ? -1 : rowAt(at.y);
    const bool grip = at.x < _box.x + 22.0;
    const bool shut = row >= 0 && at.x >= _box.x + _box.w - 38.0;

    if (row != _over || grip != (_overGrip == row) || shut != _overShut) {
        _over = row;
        _overGrip = grip ? row : -1;
        _overShut = shut;

        invalidate();
    }
}

void AddonList::leave() {
    Widget::leave();

    _over = -1;
    _overGrip = -1;
    _overShut = false;
}

bool AddonList::press(const Pointer &at) {
    _scrolling = Scroll::press(at);

    if (_scrolling) {
        return true;
    }

    land();

    const int row = rowAt(at.y);

    if (row < 0) {
        return false;
    }

    if (at.x < _box.x + 22.0) {
        _dragging = true;
        _carrying = row;
        _target = row;
        _grabY = at.y;

        _carryY.set(0.0F);
        wake();
    }

    return true;
}

void AddonList::drag(const Pointer &at) {
    Scroll::drag(at);

    if (_carrying < 0) {
        return;
    }

    const double step = rowHeight();
    const int count = static_cast<int>(State::get().cfg.files.size());

    _target = std::clamp(static_cast<int>((at.y - _box.y + offset()) / step), 0,
                         std::max(0, count - 1));

    _carryY.set(static_cast<float>(at.y - _grabY));

    invalidate();
    wake();
}

void AddonList::release(const Pointer &at) {
    Scroll::release(at);

    if (std::exchange(_scrolling, false)) {
        return;
    }

    if (_carrying >= 0) {
        _dragging = false;

        // The row walks to its gap. Only then does the list change.
        _carryY.run(static_cast<float>((_target - _carrying) * rowHeight()), now(), SETTLING,
                    Anim::Curve::CubicOut);

        _landing = now() + SETTLING + 0.02;

        wake();

        return;
    }

    const int row = rowAt(at.y);

    if (row < 0 || std::cmp_greater_equal(row, State::get().cfg.files.size())) {
        return;
    }

    if (at.x >= _box.x + _box.w - 38.0) {
        _reach->config.lists().removeFile(row);

        return;
    }

    if (at.x >= _box.x + 24.0 && at.x < _box.x + 42.0) {
        _reach->config.lists().setFileEnabled(
            row, !State::get().cfg.files[static_cast<size_t>(row)].loaded);
    }
}

bool AddonList::advance(const double now) {
    const bool gliding = Scroll::advance(now);

    _carryY.advance(now);

    invalidate();

    if (_landing > 0.0 && now >= _landing) {
        land();

        return gliding;
    }

    return gliding || _carryY.live() || _landing > 0.0 || _dragging;
}

void AddonList::land() {
    _landing = 0.0;

    if (_carrying < 0) {
        return;
    }

    if (_target != _carrying) {
        _reach->config.lists().moveFile(_carrying, _target);
    }

    _carrying = -1;
    _target = -1;

    _carryY.set(0.0F);

    invalidate();
}

bool AddonList::overLane(const double x) const {
    return scrollable() && x >= _box.x + _box.w - Theme::lane;
}

int AddonList::rowAt(const double y) const {
    const int row = static_cast<int>((y - _box.y + offset()) / rowHeight());

    return row >= 0 && std::cmp_less(row, State::get().cfg.files.size()) ? row : -1;
}

double AddonList::shiftOf(const size_t index, const double step) const {
    const int at = static_cast<int>(index);

    if (_carrying < 0 || at == _carrying) {
        return 0.0;
    }

    if (_carrying < _target && at > _carrying && at <= _target) {
        return -step;
    }

    if (_carrying > _target && at >= _target && at < _carrying) {
        return step;
    }

    return 0.0;
}

}
