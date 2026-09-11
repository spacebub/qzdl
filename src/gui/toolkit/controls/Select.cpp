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

#include "gui/draw/Glyphs.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/layout/Spacer.h"

namespace {

constexpr double ROW = 32.0;
constexpr int SHOWN = 9;

}

namespace toolkit {

// --- Select --------------------------------------------------------------------

Select::Select(std::string label, std::function<void(int)> selected)
    : Box(Flow::Column), _selected(std::move(selected)) {
    spacing(6.0);

    minWidth = 200.0;

    _takesPointer = true;
    cursor = Cursor::Pointer;

    _caption = append(std::make_unique<Label>(std::move(label)));
    _caption->section();
    _caption->setVisible(!_caption->text().empty());

    append(std::make_unique<Spacer>(0.0))->fixedHeight = Theme::control;
}

namespace {

// The rows of an open Select, on the popup layer.
class Options : public Widget {
public:
    Options(std::vector<std::string> options, std::vector<std::string> badges,
            std::string placeholder, const int current, const bool clearable,
            std::function<void(int)> chose)
        : _options(std::move(options)), _badges(std::move(badges)),
          _placeholder(std::move(placeholder)), _current(current), _clearable(clearable),
          _chose(std::move(chose)) {
        _takesPointer = true;
        cursor = Cursor::Pointer;

        _scroll = append(std::make_unique<Scroll>());
    }

    // Opens on the picked option.
    void settle() {
        const int shift = _clearable ? 1 : 0;

        _scroll->scrollTo(((_current + shift) * ROW) - _scroll->box().h + ROW);
    }

    void arrange(Typeface &type) override {
        _scroll->place(BLRect{_box.x + 5.0, _box.y + 5.0, _box.w - 10.0, _box.h - 10.0}, type);
        _scroll->setReach(static_cast<double>(_options.size() + (_clearable ? 1 : 0)) * ROW);
    }

    void paint(const Painter &painter) override {
        const Theme::Palette &palette = Theme::of();

        painter.round(_box, Theme::radiusSmall, palette.raised);
        painter.outline(_box, Theme::radiusSmall, 1.0, palette.borderStrong);

        painter.push(BLRect{_box.x + 5.0, _box.y + 5.0, _box.w - 10.0, _box.h - 10.0});

        const int shift = _clearable ? 1 : 0;
        const double top = _box.y + 5.0 - _scroll->offset();

        for (size_t row = 0; row < _options.size() + shift; ++row) {
            const BLRect line{_box.x + 5.0, top + (static_cast<double>(row) * ROW),
                              _box.w - 10.0 - (_scroll->scrollable() ? Theme::lane : 0.0), ROW};

            if (!painter.needed(line)) {
                continue;
            }

            const int at = static_cast<int>(row) - shift;
            const bool picked = at == _current;
            const std::string &option = at < 0 ? _placeholder
                                               : _options[static_cast<size_t>(at)];

            if (picked) {
                painter.round(line, Theme::radiusSmall, palette.accentSoft);
            } else if (std::cmp_equal(row, _over)) {
                painter.round(line, Theme::radiusSmall, palette.hover);
            }

            const std::string badge = at >= 0 && std::cmp_less(at, _badges.size())
                ? _badges[static_cast<size_t>(at)]
                : std::string();

            double room = line.w - 20.0;

            if (!badge.empty()) {
                const BLFont &small = painter.font(600, Theme::fontTiny);
                const double taken = painter.width(small, badge);

                painter.label(small, BLRect{line.x + line.w - 10.0 - taken, line.y, taken + 2.0,
                                            line.h},
                              Align::Start, badge, picked ? palette.accent : palette.faint);

                room -= taken + 10.0;
            }

            painter.label(painter.font(picked ? 600 : 400, Theme::fontBody),
                          BLRect{line.x + 10.0, line.y, room, line.h}, Align::Start, option,
                          picked      ? palette.accent
                          : at < 0    ? palette.faint
                                      : palette.text);
        }

        painter.pop();

        // The bar, which the scroller draws over its own box.
        _scroll->paint(painter);
    }

    bool wheel(const double steps, const Pointer &at) override {
        return _scroll->wheel(steps, at);
    }

    void hover(const Pointer &at) override {
        const int over = rowAt(at.y);

        if (over != _over) {
            _over = over;

            invalidate();
        }
    }

    void leave() override {
        Widget::leave();

        _over = -1;
    }

    // The rows are drawn by this, not by the scroller, so the list answers the
    // pointer itself; the scroller only wants its lane.
    Widget *at(const double x, const double y) override {
        if (!visible() || !holds(x, y)) {
            return nullptr;
        }

        return _scroll->scrollable() && x >= _box.x + _box.w - 5.0 - Theme::lane
            ? static_cast<Widget *>(_scroll)
            : this;
    }

    bool press(const Pointer &at) override {
        return holds(at.x, at.y) && at.x < _box.x + _box.w - Theme::lane;
    }

    void release(const Pointer &at) override {
        const int row = rowAt(at.y);

        if (row < 0 || !_chose) {
            return;
        }

        // Copied out: choosing closes the list, which frees the callable.
        const std::function<void(int)> fire = _chose;

        fire(row - (_clearable ? 1 : 0));
    }

    // How tall the whole list wants to be.
    static double heightOf(const size_t options, const bool clearable) {
        const int rows = std::min(static_cast<int>(options) + (clearable ? 1 : 0), SHOWN);

        return std::max(44.0, (rows * ROW) + 10.0);
    }

private:
    [[nodiscard]] int rowAt(const double y) const {
        const double top = _box.y + 5.0 - _scroll->offset();
        const int row = static_cast<int>((y - top) / ROW);

        return row >= 0 && std::cmp_less(row, _options.size() + (_clearable ? 1 : 0)) ? row : -1;
    }

    std::vector<std::string> _options;
    std::vector<std::string> _badges;
    std::string _placeholder;

    int _current = -1;
    int _over = -1;
    bool _clearable = false;

    std::function<void(int)> _chose;

    Scroll *_scroll = nullptr;
};

}

void Select::setOptions(std::vector<std::string> options) {
    _options = std::move(options);

    invalidate();
}

void Select::setBadges(std::vector<std::string> badges) {
    _badges = std::move(badges);

    invalidate();
}

void Select::setCurrent(const int index) {
    if (_current == index) {
        return;
    }

    _current = index;

    invalidate();
}

Select *Select::placeholder(std::string text) {
    _placeholder = std::move(text);

    return this;
}

Select *Select::clearable(const bool value) {
    _clearable = value;

    return this;
}

Select *Select::tip(std::string text) {
    hint = std::move(text);

    return this;
}

double Select::naturalWidth(Typeface &type) {
    return fixedWidth >= 0.0 ? fixedWidth : std::max(200.0, Box::naturalWidth(type));
}

void Select::arrange(Typeface &type) {
    Box::arrange(type);

    _frame = children().back()->box();
    _frame.h = Theme::control;
}

void Select::show() {
    if (_list != nullptr || root() == nullptr || _options.empty()) {
        return;
    }

    const double tall = Options::heightOf(_options.size(), _clearable);
    const bool below = _frame.y + _frame.h + 4.0 + tall <= root()->height();

    auto made = std::make_unique<Options>(_options, _badges, _placeholder, _current, _clearable,
                                          [this](const int index) {
                                              close();

                                              if (_selected) {
                                                  _selected(index);
                                              }
                                          });

    Options *raw = made.get();

    _list = root()->layer(Root::POPUPS)->add(std::move(made));

    _list->place(BLRect{_frame.x, below ? _frame.y + _frame.h + 4.0 : _frame.y - tall - 4.0,
                        _frame.w, tall},
                 root()->type());

    raw->settle();

    root()->setDismiss([this] { close(); }, this);

    _turn.run(180.0F, now(), 0.16, Anim::Curve::CubicOut);

    animate();
    invalidate();
    _list->invalidate();
}

void Select::close() {
    if (_list == nullptr || root() == nullptr) {
        return;
    }

    const BLRect was = _list->box();

    root()->layer(Root::POPUPS)->erase(_list);

    _list = nullptr;

    root()->setDismiss(nullptr);
    root()->damage(was);

    _turn.run(0.0F, now(), 0.16, Anim::Curve::CubicOut);

    animate();
    invalidate();
}

Widget *Select::at(const double x, const double y) {
    return visible() && enabled() && x >= _frame.x && x < _frame.x + _frame.w && y >= _frame.y
            && y < _frame.y + _frame.h
        ? this
        : nullptr;
}

bool Select::press(const Pointer & /*at*/) {
    return enabled();
}

void Select::release(const Pointer &at) {
    if (!holds(at.x, at.y)) {
        return;
    }

    if (_list == nullptr) {
        show();
    } else {
        close();
    }
}

void Select::enter() {
    Widget::enter();

    _lit.toward(1.0F, now(), 0.11, Anim::Curve::CubicOut);
    animate();
}

void Select::leave() {
    Widget::leave();

    _lit.toward(0.0F, now(), 0.11, Anim::Curve::CubicOut);
    animate();
}

bool Select::key(const Key &pressed) {
    if (pressed.code == Code::Return || pressed.text == " ") {
        release(Pointer{.x = _frame.x + 1.0, .y = _frame.y + 1.0});

        return true;
    }

    if (pressed.code == Code::Down && _current + 1 < static_cast<int>(_options.size())) {
        if (_selected) {
            _selected(_current + 1);
        }

        return true;
    }

    if (pressed.code == Code::Up && _current > (_clearable ? -1 : 0)) {
        if (_selected) {
            _selected(_current - 1);
        }

        return true;
    }

    return false;
}

bool Select::advance(const double now) {
    _lit.advance(now);
    _turn.advance(now);

    invalidate();

    return _lit.live() || _turn.live();
}

void Select::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const double lit = _lit.value();
    const double dim = enabled() ? 1.0 : 0.45;

    painter.round(_frame, Theme::radiusSmall,
                  Theme::alpha(pressed() ? palette.sunken : palette.field, dim));

    if (lit > 0.0 && !pressed()) {
        painter.round(_frame, Theme::radiusSmall, Theme::alpha(palette.hover, lit * dim));
    }

    painter.outline(_frame, Theme::radiusSmall, 1.0,
                    Theme::alpha(open() ? palette.accent
                                        : Theme::mix(palette.border, palette.borderStrong, lit),
                                 dim));

    const std::string shown = _current >= 0 && std::cmp_less(_current, _options.size())
        ? _options[static_cast<size_t>(_current)]
        : std::string();
    const std::string badge = _current >= 0 && std::cmp_less(_current, _badges.size())
        ? _badges[static_cast<size_t>(_current)]
        : std::string();

    double room = _frame.w - 12.0 - 32.0;

    if (!badge.empty()) {
        const BLFont &small = painter.font(600, Theme::fontTiny);
        const double taken = painter.width(small, badge);

        painter.label(small,
                      BLRect{_frame.x + _frame.w - 32.0 - taken - 8.0, _frame.y, taken + 2.0,
                             _frame.h},
                      Align::Start, badge, Theme::alpha(palette.faint, dim));

        room -= taken + 8.0;
    }

    painter.label(painter.font(400, Theme::fontBody),
                  BLRect{_frame.x + 12.0, _frame.y, room, _frame.h}, Align::Start,
                  shown.empty() ? _placeholder : shown,
                  Theme::alpha(shown.empty() ? palette.faint : palette.text, dim));

    constexpr float weight = 1.0F;
    const double side = Glyphs::span(weight);

    Glyphs::draw(painter.context(), "down",
                 BLPoint{_frame.x + _frame.w - 12.0 - side, _frame.y + ((_frame.h - side) / 2.0)},
                 weight, Theme::alpha(open() ? palette.accent : palette.faint, dim),
                 _turn.value());

    Box::paint(painter);
}

}
