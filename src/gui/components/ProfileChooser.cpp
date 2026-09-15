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
#include <functional>
#include <utility>
#include <vector>

#include "gui/components/ProfileChooser.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Paint.h"
#include "gui/draw/Theme.h"
#include "gui/draw/Typeface.h"
#include "gui/model/ProfileBridge.h"
#include "gui/state/State.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/layout/Scroll.h"

namespace components {

using namespace toolkit;

namespace {

// The profile list the chooser drops, with a row per profile and a way to add one.
class Profiles : public Widget {
public:
    static constexpr double ROW = 52.0;
    static constexpr double ADDER = 38.0;

    Profiles(Reach *reach, std::function<void()> chose)
        : _reach(reach), _chose(std::move(chose)) {
        _takesPointer = true;
        cursor = Cursor::Pointer;

        _scroll = append(std::make_unique<Scroll>());
    }

    static double heightOf(const size_t count) {
        return (static_cast<double>(std::min<size_t>(count, 8)) * ROW) + 10.0 + ADDER;
    }

    void arrange(Typeface &type) override {
        _scroll->place(BLRect{_box.x + 5.0, _box.y + 5.0, _box.w - 10.0,
                              _box.h - 10.0 - ADDER},
                       type);

        _scroll->setReach(static_cast<double>(State::get().cfg.profileCards.size()) * ROW);
    }

    void settle() const {
        _scroll->scrollTo((State::get().cfg.profileIndex * ROW) - _scroll->box().h + ROW);
    }

    void paint(const Painter &painter) override {
        const Theme::Palette &palette = Theme::of();
        const std::vector<State::ProfileCard> &cards = State::get().cfg.profileCards;

        painter.round(_box, Theme::radiusSmall, palette.raised);
        painter.outline(_box, Theme::radiusSmall, 1.0, palette.borderStrong);

        const BLRect view = _scroll->box();

        painter.push(view);

        double y = view.y - _scroll->offset();

        for (size_t index = 0; index < cards.size(); ++index) {
            const State::ProfileCard &card = cards[index];
            const BLRect line{view.x, y, view.w - (_scroll->scrollable() ? Theme::lane : 0.0),
                              ROW};

            y += ROW;

            if (!painter.needed(line)) {
                continue;
            }

            const bool current = card.index == State::get().cfg.profileIndex;

            if (current) {
                painter.round(line, Theme::radiusSmall, palette.accentSoft);
            } else if (std::cmp_equal(index, _over)) {
                painter.round(line, Theme::radiusSmall, palette.hover);
            }

            const BLRect chip{line.x + 8.0, line.y + ((line.h - 36.0) / 2.0), 60.0, 36.0};

            painter.round(chip, Theme::radiusSmall - 3.0, palette.artMiddle);

            const BLImage shot = _reach->art.of(card.artKey);

            if (!shot.is_empty()) {
                Paint::cover(painter.context(), chip, shot, Theme::radiusSmall - 3.0);
            }

            const double left = chip.x + chip.w + 10.0;

            painter.label(painter.font(current ? 600 : 400, Theme::fontBody),
                          BLRect{left, line.y + 8.0, line.x + line.w - 10.0 - left, 18.0},
                          Align::Start, card.name, current ? palette.accent : palette.text);

            painter.label(painter.font(400, Theme::fontTiny),
                          BLRect{left, line.y + 27.0, line.x + line.w - 10.0 - left, 16.0},
                          Align::Start,
                          card.port.empty()
                              ? "No source port"
                              : card.port + " · " + (card.iwad.empty() ? "no game" : card.iwad),
                          palette.faint);
        }

        painter.pop();

        _scroll->paint(painter);

        // The row that makes one.
        const BLRect adder{_box.x, _box.y + _box.h - ADDER, _box.w, ADDER};

        painter.fill(BLRect{adder.x + 4.0, adder.y, adder.w - 8.0, 1.0}, palette.border);

        if (_onAdder) {
            painter.round(BLRect{adder.x + 4.0, adder.y + 5.0, adder.w - 8.0, adder.h - 9.0},
                          Theme::radiusSmall, palette.hover);
        }

        const double side = Glyphs::span(1.0F);

        Glyphs::draw(painter.context(), Glyphs::Glyph::Plus,
                     BLPoint{adder.x + 14.0, adder.y + ((adder.h - side) / 2.0)}, 1.0F,
                     _onAdder ? palette.accent : palette.faint);

        painter.label(painter.font(400, Theme::fontBody),
                      BLRect{adder.x + 14.0 + side + 9.0, adder.y, adder.w - 60.0, adder.h},
                      Align::Start, "New profile", _onAdder ? palette.accent : palette.text);
    }

    bool wheel(const double steps, const Pointer &at) override {
        return _scroll->wheel(steps, at);
    }

    void hover(const Pointer &at) override {
        const bool adder = at.y >= _box.y + _box.h - ADDER;
        const int row = adder ? -1 : rowAt(at.y);

        if (adder != _onAdder || row != _over) {
            _onAdder = adder;
            _over = row;

            invalidate();
        }
    }

    void leave() override {
        Widget::leave();

        _over = -1;
        _onAdder = false;
    }

    // The rows are this widget's, not the scroller's, so it answers the pointer
    // itself. The scroller only wants its lane.
    Widget *at(const double x, const double y) override {
        if (!visible() || !holds(x, y)) {
            return nullptr;
        }

        return _scroll->scrollable() && x >= _box.x + _box.w - 5.0 - Theme::lane
            ? static_cast<Widget *>(_scroll)
            : this;
    }

    bool press(const Pointer &at) override { return holds(at.x, at.y); }

    void release(const Pointer &at) override {
        // Read out first: closing the list frees this widget.
        Reach *reach = _reach;
        const std::function<void()> close = _chose;

        if (at.y >= _box.y + _box.h - ADDER) {
            if (close) {
                close();
            }

            reach->prompt("New profile", "Name", "New profile", "Create",
                          [reach](const std::string &named) {
                              reach->config.profile().addProfile(named);
                          });

            return;
        }

        const int row = rowAt(at.y);

        if (row < 0 || std::cmp_greater_equal(row, State::get().cfg.profileCards.size())) {
            return;
        }

        const int index = State::get().cfg.profileCards[static_cast<size_t>(row)].index;

        if (close) {
            close();
        }

        reach->config.profile().setProfileIndex(index);
    }

private:
    [[nodiscard]] int rowAt(const double y) const {
        const int row = static_cast<int>((y - _scroll->box().y + _scroll->offset()) / ROW);

        return row >= 0 ? row : -1;
    }

    Reach *_reach;

    std::function<void()> _chose;

    Scroll *_scroll = nullptr;

    int _over = -1;
    bool _onAdder = false;
};

}

ProfileChooser::ProfileChooser(Reach *reach) : _reach(reach) {
    _takesPointer = true;
    cursor = Cursor::Pointer;

    _status = append(std::make_unique<StatusIndicator>());
    _status->onClick([this] { _reach->runs.show(State::get().cfg.profileKey); },
                     "Click to see what it printed.");
}

void ProfileChooser::arrange(Typeface &type) {
    const double wide = _status->wantedWidth(type);

    _status->setVisible(_status->status() != StatusIndicator::Status::Empty);
    _status->place(BLRect{_box.x + _box.w - MARGIN - wide,
                          _box.y + ((_box.h - StatusIndicator::HEIGHT) / 2.0), wide,
                          StatusIndicator::HEIGHT},
                   type);
}

void ProfileChooser::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const State::Cfg &cfg = State::get().cfg;

    if (holdsPointer() || _open) {
        painter.round(_box, Theme::radius, _open ? palette.mutedSoft : palette.hover);
    }

    const BLRect thumb{_box.x + 8.0, _box.y + ((_box.h - THUMB_TALL) / 2.0), THUMB_WIDE,
                       THUMB_TALL};

    painter.round(thumb, Theme::radiusSmall, palette.artMiddle);

    if (!_thumb.is_empty()) {
        painter.context().blit_image(BLPoint{thumb.x, thumb.y}, _thumb);
    }

    const double left = thumb.x + thumb.w + 14.0;
    const BLFont &name = painter.font(palette.headingWeight, Theme::fontDisplay);

    painter.label(name, BLRect{left, _box.y + 12.0, _box.w - left + _box.x, 30.0},
                  Align::Start, cfg.profileName, palette.text);

    painter.label(painter.font(400, Theme::fontSmall),
                  BLRect{left, _box.y + 44.0, _box.w - left + _box.x, 18.0}, Align::Start,
                  _said, _ready ? palette.faint : palette.warning);

    Widget::paint(painter);
}

void ProfileChooser::setSaid(std::string said, const bool ready) {
    const std::string &name = State::get().cfg.profileName;
    const int art = State::get().sys.artRev;
    std::string key = ProfileBridge::artKey();

    // sync() runs on every touch of the state tree. The head carries a scaled
    // title screen, which is far too dear to look up and resample for a log line
    // arriving. A title landing moves artRev. Swapping the add-ons one is picked
    // from moves the key, which the summary only counts.
    if (said == _said && ready == _ready && name == _name && art == _artRev
        && key == _artKey) {
        return;
    }

    _said = std::move(said);
    _ready = ready;
    _name = name;
    _artRev = art;
    _artKey = std::move(key);

    if (const BLImage shot = _reach->art.of(_artKey); !shot.equals(_shot)) {
        _shot = shot;

        cutThumb();
    }

    invalidate();
}

void ProfileChooser::setStatus(const StatusIndicator::Status status, std::string reason) {
    // The pill's width comes from the word on it, so a new word moves it.
    const bool shifts = status != _status->status();

    _status->set(status, std::move(reason));

    if (shifts && root() != nullptr) {
        place(_box, root()->type());
        invalidate();
    }
}

void ProfileChooser::within(const bool inside) {
    Widget::within(inside);

    invalidate();
}

bool ProfileChooser::press(const Pointer & /*at*/) { return true; }

void ProfileChooser::release(const Pointer &at) {
    if (holds(at.x, at.y)) {
        show();
    }
}

void ProfileChooser::show() {
    if (_open || root() == nullptr) {
        return;
    }

    const double tall = Profiles::heightOf(State::get().cfg.profileCards.size());
    const double wide = std::clamp(_box.w, 280.0, 460.0);

    auto made = std::make_unique<Profiles>(_reach, [this] { root()->dismiss(); });
    const Profiles *raw = made.get();

    _list = root()->layer(Root::POPUPS)->add(std::move(made));
    _open = true;

    _list->place(BLRect{_box.x, _box.y + _box.h + 6.0, wide, tall}, root()->type());

    raw->settle();

    root()->setDismiss([this] {
        if (_list != nullptr) {
            const BLRect was = _list->box();

            root()->layer(Root::POPUPS)->erase(_list);

            _list = nullptr;
            _open = false;

            root()->damage(was);

            // The chooser draws itself differently while the list is down.
            invalidate();
        }
    }, this);

    _list->invalidate();
    invalidate();
}

void ProfileChooser::cutThumb() {
    _thumb.reset();

    if (_shot.is_empty()
        || _thumb.create(THUMB_WIDE, THUMB_TALL, BL_FORMAT_PRGB32) != BL_SUCCESS) {
        return;
    }

    BLContext into(_thumb);

    into.clear_all();
    Paint::cover(into, BLRect{0.0, 0.0, THUMB_WIDE, THUMB_TALL}, _shot, Theme::radiusSmall);
}

}
