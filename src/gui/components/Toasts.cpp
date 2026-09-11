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

#include "gui/components/Toasts.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/GlyphButton.h"

namespace components {

using namespace toolkit;

// --- Toasts --------------------------------------------------------------------

Toasts::Toasts(std::function<void(int)> dismissed) : _dismissed(std::move(dismissed)) {}

// --- Toast ---------------------------------------------------------------------

Toast::Toast(State::Buzz message, std::function<void()> close)
    : _message(std::move(message)), _close(std::move(close)), _left(_message.duration) {
    _takesPointer = true;

    

    _shut = append(std::make_unique<GlyphButton>("cross", [this] { this->close(); }));
    _shut->size(24.0)->tone(Theme::of().faint, Theme::of().text);
    _shut->fixedWidth = 24.0;
    _shut->fixedHeight = 24.0;
}

void Toasts::setMessages(const std::vector<State::Buzz> &messages) {
    std::vector<int> wanted;

    wanted.reserve(messages.size());

    for (const State::Buzz &message : messages) {
        wanted.push_back(message.id);
    }

    if (wanted == _shown) {
        return;
    }

    _shown = wanted;

    clear();

    for (const State::Buzz &message : messages) {
        const int id = message.id;

        append(std::make_unique<Toast>(message, [this, id] {
            if (_dismissed) {
                _dismissed(id);
            }
        }));
    }

    if (root() != nullptr) {
        root()->relayout();
    }
}

void Toasts::arrange(Typeface &type) {
    // Inset from the window's corner, under the title bar.
    const double right = _box.x + _box.w - 18.0;

    double y = _box.y + Theme::barHeight + 18.0;

    for (const Ptr &child : children()) {
        const double tall = child->naturalHeight(type, 392.0);

        child->place(BLRect{right - 392.0, y, 392.0, tall}, type);

        y += tall + 10.0;
    }
}

bool Toasts::advance(double /*now*/) {
    return false;
}

void Toast::moved() {
    animate();
}

BLRgba32 Toast::tone() const {
    const Theme::Palette &palette = Theme::of();

    switch (_message.severity) {
        case 3:
            return palette.danger;

        case 2:
            return palette.warning;

        case 1:
            return palette.success;

        default:
            return palette.accent;
    }
}

BLRgba32 Toast::wash() const {
    const Theme::Palette &palette = Theme::of();

    switch (_message.severity) {
        case 3:
            return palette.dangerSoft;

        case 2:
            return palette.warningSoft;

        case 1:
            return palette.successSoft;

        default:
            return palette.accentSoft;
    }
}

double Toast::naturalHeight(Typeface &type, double /*width*/) {
    const BLFont &face = type.at(400, Theme::fontSmall);
    const double room = WIDTH - 56.0;

    double tall = wrapHeight(type, face, _message.body, room);

    if (!_message.title.empty()) {
        tall += wrapHeight(type, type.at(Typeface::bold, Theme::fontSmall), _message.title, room)
            + 3.0;
    }

    return tall + 26.0;
}

void Toast::arrange(Typeface &type) {
    (void) type;

    _shut->place(BLRect{_box.x + _box.w - 32.0, _box.y + 9.0, 24.0, 24.0}, type);
}

void Toast::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const double here = _here.value();

    if (here <= 0.0) {
        return;
    }

    painter.round(_box, Theme::radius, wash());
    painter.outline(_box, Theme::radius, 1.0, Theme::alpha(tone(), 0.42));

    const double room = _box.w - 56.0;
    double y = _box.y + 13.0;

    if (!_message.title.empty()) {
        const BLFont &face = painter.font(Typeface::bold, Theme::fontSmall);

        painter.circle(BLPoint{_box.x + 20.0, y + (painter.lineHeight(face) / 2.0)}, 4.0, tone());

        y += painter.paragraph(face, BLRect{_box.x + 31.0, y, room - 15.0, 0.0}, _message.title,
                               tone())
            + 3.0;

        painter.paragraph(painter.font(400, Theme::fontSmall),
                          BLRect{_box.x + 31.0, y, room - 15.0, 0.0}, _message.body,
                          palette.text);
    } else {
        const BLFont &face = painter.font(400, Theme::fontSmall);

        painter.circle(BLPoint{_box.x + 20.0, y + (painter.lineHeight(face) / 2.0)}, 4.0, tone());
        painter.paragraph(face, BLRect{_box.x + 31.0, y, room - 15.0, 0.0}, _message.body,
                          palette.text);
    }

    if (_message.duration > 0) {
        const double left = std::clamp(_left / _message.duration, 0.0, 1.0);

        painter.fill(BLRect{_box.x + 1.0, _box.y + _box.h - 4.0, (_box.w - 2.0) * left, 3.0},
                     tone());
    }

    Widget::paint(painter);
}

void Toast::enter() {
    Widget::enter();
}

void Toast::leave() {
    Widget::leave();
}

void Toast::close() {
    if (_going) {
        return;
    }

    _going = true;

    _here.run(0.0F, now(), 0.16, Anim::Curve::CubicOut);
    animate();
}

bool Toast::advance(const double now) {
    if (!_started) {
        _started = true;

        _here.run(1.0F, now, 0.16, Anim::Curve::CubicOut);
    }

    _here.advance(now);

    // Anything but an error counts down; hovering holds it.
    if (_message.duration > 0 && !_going && !hovered()) {
        if (_ticked > 0.0) {
            _left -= (now - _ticked) * 1000.0;
        }

        _ticked = now;

        invalidate();

        if (_left <= 0.0) {
            close();
        }
    } else {
        _ticked = now;
    }

    if (_going && !_here.live() && _close) {
        const std::function<void()> going = _close;

        _close = nullptr;

        going();

        return false;
    }

    invalidate();

    return true;
}

}
