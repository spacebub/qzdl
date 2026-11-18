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
#include <utility>
#include <vector>

#include "ttk/draw/Anim.h"
#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Painter.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Panel.h"
#include "ttk/util/Format.h"

#include "gui/components/BadgeRow.h"
#include "gui/draw/Cards.h"
#include "gui/state/State.h"

namespace components {

using namespace ttk;

// One port on either shelf of the engines page.
class EngineCard : public ttk::Panel {
public:
    EngineCard() {
        _takesPointer = true;
        hoverable = true;

        _row = append(Box::row());
        _row->spacing(8.0);
    }

    // The installed shelf carries the drag. The browse shelf does not.
    bool draggable = false;

    std::function<void(double, double)> dragStarted;
    std::function<void(double, double)> dragMoved;
    std::function<void()> dragEnded;
    std::function<void()> opened;

    std::string name;
    std::string blurb;
    std::string file;
    bool missing = false;

    // The pills along the top, and what each of them says when rested on.
    std::vector<State::BadgeSpec> tags;
    std::vector<std::string> tagHints;

    // Under the buttons: what is known, or what went wrong.
    std::string told;
    bool trouble = false;

    // Shown as a bar instead, while something is being fetched.
    bool working = false;
    double progress = 0.0;

    [[nodiscard]] Box *buttons() const { return _row; }

    void arrange(ttk::Typeface &type) override {
        _row->place(BLRect{_box.x + 16.0,
                           _box.y + _box.h - 16.0 - ttk::Theme::controlSmall,
                           _box.w - 32.0,
                           ttk::Theme::controlSmall},
                    type);
    }

    void paint(const Painter &painter) override {
        lit = holds_pointer() || _carrying;

        Panel::paint(painter);

        const ttk::Theme::Palette &palette = ttk::Theme::palette();
        const BLFont &face = painter.font(600, ttk::Theme::fontMedium);

        // The fan damage reaches a pixel past it. A clip edge through a button corner shifts its antialiasing.
        const BLRect aboveButtons{_box.x, _box.y, _box.w, _row->box().y - _box.y - 1.0};

        _tagRow.paint(painter,
                      BLRect{_box.x + 32.0 + NAME_ROOM, _box.y + 16.0, _box.w - 48.0 - NAME_ROOM, 22.0},
                      aboveButtons,
                      tags);

        const BLRect drawn = _tagRow.extent();
        const double right = drawn.w > 0.0 ? drawn.x - 8.0 : _box.x + _box.w - 16.0;

        painter.label(face,
                      BLRect{_box.x + 16.0, _box.y + 16.0, right - _box.x - 24.0, 22.0},
                      Align::Start,
                      name,
                      trouble && missing ? palette.danger : palette.text);

        if (!blurb.empty()) {
            painter.paragraph(painter.font(400, ttk::Theme::fontSmall),
                              BLRect{_box.x + 16.0, _box.y + 46.0, _box.w - 32.0, 0.0},
                              blurb,
                              palette.faint);
        }

        if (!file.empty()) {
            const BLFont &mono = painter.font(ttk::Typeface::mono, ttk::Theme::fontTiny);
            const double unit = painter.width(mono, "M");
            const int room = unit > 0.0 ? static_cast<int>((_box.w - 32.0) / unit) : 0;

            painter.label(mono,
                          BLRect{_box.x + 16.0, _box.y + 46.0, _box.w - 32.0, 18.0},
                          Align::Start,
                          ttk::Format::fit_path(file, room),
                          palette.faint);
        }

        const double line = _row->box().y - 26.0;

        if (working) {
            const BLRect track{_box.x + 16.0, line + 5.0, _box.w - 32.0, 6.0};

            painter.round(track, 3.0, palette.sunken);
            painter.round(
                    BLRect{track.x, track.y, track.w * std::clamp(progress, 0.0, 1.0), track.h}, 3.0, palette.accent);
        } else if (!told.empty()) {
            painter.label(painter.font(400, ttk::Theme::fontSmall),
                          BLRect{_box.x + 16.0, line, _box.w - 32.0, 16.0},
                          Align::Start,
                          told,
                          trouble ? palette.danger : palette.faint);
        }

        _tagRow.paintFan(painter, tags);
    }

    bool press(const Pointer &at) override {
        _pressX = at.x;
        _pressY = at.y;
        _carrying = false;
        _armed = true;

        return true;
    }

    void drag(const Pointer &at) override {
        if (!draggable || !_armed) {
            return;
        }

        if (!_carrying) {
            if (std::abs(at.x - _pressX) < Cards::dragSlack && std::abs(at.y - _pressY) < Cards::dragSlack) {
                return;
            }

            _carrying = true;

            if (dragStarted) {
                dragStarted(_pressX, _pressY);
            }
        }

        if (dragMoved) {
            dragMoved(at.x, at.y);
        }
    }

    void release(const Pointer &at) override {
        const bool carried = _carrying;

        _armed = false;
        _carrying = false;

        if (carried) {
            if (dragEnded) {
                dragEnded();
            }

            return;
        }

        if (holds(at.x, at.y) && opened && at.y < _row->box().y) {
            opened();
        }
    }

    // Nothing on a card is a link. An installed one is only carried.
    [[nodiscard]] Cursor cursor_at(double /*x*/, double /*y*/) const override {
        return _carrying ? Cursor::Grabbing : Cursor::Default;
    }

    void hover(const Pointer &at) override {
        if (_tagRow.hover(at.x, at.y, now())) {
            wake();
        }

        const int which = _tagRow.at(at.x, at.y);

        if (which >= 0 && std::cmp_less(which, tagHints.size())) {
            hint = tagHints[static_cast<size_t>(which)];
        } else {
            hint.clear();
        }
    }

    void leave() override {
        Panel::leave();

        _tagRow.leave(now());

        if (_tagRow.live()) {
            wake();
        }

        hint.clear();
    }

    // The buttons are on the card: the light stays while the pointer is on them.
    void within(bool /*inside*/) override { invalidate(); }

    // The place in the grid it was last given. A card walks to a new one. It does
    // not walk because the grid scrolled or the window changed size.
    [[nodiscard]] int slot() const { return _slot; }

    void setSlot(const int at) { _slot = at; }

    // The walk to a new place, while the cards shuffle around a carried one.
    void slideFrom(const double x, const double y, const double now) {
        _slideX.set(static_cast<float>(x));
        _slideY.set(static_cast<float>(y));

        _slideX.run(0.0F, now, Cards::settling, ttk::Anim::Curve::CubicOut);
        _slideY.run(0.0F, now, Cards::settling, ttk::Anim::Curve::CubicOut);

        wake();
    }

    [[nodiscard]] double slide_x() const { return _slideX.value(); }
    [[nodiscard]] double slide_y() const { return _slideY.value(); }

    [[nodiscard]] bool sliding() const { return _slideX.live() || _slideY.live(); }

    bool advance(const double now) override {
        // The slide is in the box here, so the shelf lays it out again. What it was
        // has to be damaged before that, since the live list is not ordered.
        const BLRect was = _box;
        const float open = _tagRow.open();
        const BLRect fanWas = _tagRow.fanBox(open);

        _slideX.advance(now);
        _slideY.advance(now);
        _tagRow.advance(now);

        if (sliding()) {
            invalidate(was);
            invalidate();
        } else if (_tagRow.open() != open) {
            invalidate(grown(fanWas));
            invalidate(grown(_tagRow.fanBox(_tagRow.open())));
        }

        return sliding() || _tagRow.live();
    }

    // The tags the row shows and the ones folded behind its mark.
    [[nodiscard]] const BadgeRow &tagRow() const { return _tagRow; }

private:
    // The name keeps this much beside the tags, about six letters. Tags past it fold behind the mark.
    static constexpr double NAME_ROOM = 64.0;

    // Antialiasing reaches a pixel past a shape's edge.
    static BLRect grown(const BLRect &part) {
        return part.w > 0.0 ? BLRect{part.x - 1.0, part.y - 1.0, part.w + 2.0, part.h + 2.0} : BLRect{};
    }

    Box *_row = nullptr;

    double _pressX = 0.0;
    double _pressY = 0.0;

    bool _armed = false;
    bool _carrying = false;

    int _slot = -1;

    BadgeRow _tagRow{BadgeRow::Look{
            .from = BadgeRow::From::End,
            .fan = BadgeRow::Fan::Down,
            .tone = BadgeRow::Tone::Clear,
            .gap = 8.0,
            .keepFirst = true,
    }};

    ttk::Anim::Tween _slideX;
    ttk::Anim::Tween _slideY;
};

}
