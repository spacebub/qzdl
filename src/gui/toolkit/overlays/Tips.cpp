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

#include "gui/draw/Theme.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/overlays/Tips.h"

namespace toolkit {

// --- Tips ----------------------------------------------------------------------

BLRect Tips::measure(const std::string &said) const {
    if (said.empty() || root() == nullptr) {
        return {};
    }

    Typeface &type = root()->type();
    const BLFont &face = type.at(400, Theme::fontSmall);

    const double wide = std::min(ROOM, static_cast<double>(type.width(face, said)));
    const double tall = wrapHeight(type, face, said, wide);

    const double width = wide + 20.0;
    const double height = tall + 20.0;

    // Centred on the pointer, and never off an edge.
    const double x = std::clamp(_x - (width / 2.0), _box.x + 4.0,
                                std::max(_box.x + 4.0, _box.x + _box.w - width - 4.0));

    double y = _over.y - height - 6.0;

    if (y < _box.y + 4.0) {
        y = _over.y + _over.h + 6.0;
    }

    y = std::clamp(y, _box.y + 4.0, std::max(_box.y + 4.0, _box.y + _box.h - height - 4.0));

    return BLRect{x, y, width, height};
}

void Tips::raise() {
    const BLRect was = _frame;

    _shown = _pending;
    _up = true;
    _frame = measure(_shown);

    invalidate(was);
    invalidate(_frame);
}

void Tips::point(const std::string &text, const BLRect &over, const double x, const double y,
                 const double now) {
    // The same words under a pointer that has moved still follow it while nothing
    // is up yet; once one is up it stays where it was raised.
    const bool same = text == _pending;

    if (same && (_up || text.empty())) {
        return;
    }

    _pending = text;
    _over = over;
    _x = x;
    _y = y;

    if (text.empty()) {
        if (_up) {
            _up = false;

            _fade.run(0.0F, now, 0.11, Anim::Curve::CubicOut);
        }

        animate();

        return;
    }

    if (!same) {
        _armed = now;
    }

    // Moving from one control to another shows the next one at once: the wait is
    // for the first tip, not for every one after it.
    if (_up) {
        raise();
    }

    animate();
}

bool Tips::advance(const double now) {
    if (!_up && !_pending.empty() && now - _armed >= DELAY) {
        raise();

        _fade.run(1.0F, now, 0.11, Anim::Curve::CubicOut);
    }

    const bool fading = _fade.live();

    _fade.advance(now);

    // The frame the fade's last step drew is repainted too, not only the live ones.
    if (fading) {
        invalidate(_frame);
    }

    if (!_fade.live() && _fade.value() <= 0.0 && !_shown.empty()) {
        const BLRect was = _frame;

        _shown.clear();
        _frame = BLRect{};

        invalidate(was);
    }

    return _fade.live() || (!_pending.empty() && !_up);
}

void Tips::paint(const Painter &painter) {
    if (_shown.empty() || _fade.value() <= 0.0 || _frame.w <= 0.0) {
        return;
    }

    const Theme::Palette &palette = Theme::of();
    const BLFont &face = painter.font(400, Theme::fontSmall);
    const double fade = _fade.value();

    painter.round(_frame, Theme::radiusSmall, Theme::alpha(palette.raised, fade));
    painter.outline(_frame, Theme::radiusSmall, 1.0, Theme::alpha(palette.borderStrong, fade));
    painter.paragraph(face,
                      BLRect{_frame.x + 10.0, _frame.y + 10.0, _frame.w - 20.0,
                             _frame.h - 20.0},
                      _shown, Theme::alpha(palette.text, fade));
}

}
