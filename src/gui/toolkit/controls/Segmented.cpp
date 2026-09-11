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

#include <utility>

#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Segmented.h"

namespace toolkit {

Segmented::Segmented(std::function<void(const std::string &)> selected)
    : _selected(std::move(selected)) {
    _takesPointer = true;
    cursor = Cursor::Pointer;
}

void Segmented::setOptions(std::vector<Choice> options) {
    _options = std::move(options);
    _marked = false;

    invalidate();

    if (root() != nullptr) {
        root()->relayout();
    }
}

void Segmented::setCurrent(std::string key) {
    if (_current == key) {
        return;
    }

    _current = std::move(key);

    invalidate();
    animate();
}

std::vector<BLRect> Segmented::lanes(Typeface &type) const {
    std::vector<BLRect> out;
    const BLFont &face = type.at(400, Theme::fontBody);

    double x = _box.x + 4.0;

    for (const Choice &option : _options) {
        const double wide = type.width(face, option.label) + 34.0;

        out.emplace_back(x, _box.y + 4.0, wide, _box.h - 8.0);

        x += wide;
    }

    return out;
}

double Segmented::naturalWidth(Typeface &type) {
    if (fixedWidth >= 0.0) {
        return fixedWidth;
    }

    const BLFont &face = type.at(400, Theme::fontBody);
    double total = 8.0;

    for (const Choice &option : _options) {
        total += type.width(face, option.label) + 34.0;
    }

    return total;
}

// The marker is placed against the lanes every time they move, so a resize does
// not leave it where the control used to be.
void Segmented::arrange(Typeface &type) {
    const std::vector<BLRect> boxes = lanes(type);

    for (size_t at = 0; at < boxes.size(); ++at) {
        if (_options[at].key != _current) {
            continue;
        }

        if (!_marked || !_markX.live()) {
            _marked = true;

            _markX.set(static_cast<float>(boxes[at].x));
            _markWidth.set(static_cast<float>(boxes[at].w));
        }

        return;
    }

    // Nothing is current, so nothing is marked.
    _markWidth.set(0.0F);
}

void Segmented::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const double radius = _box.h / 2.0;

    painter.round(_box, radius, palette.sunken);
    painter.outline(_box, radius, 1.0, palette.border);

    const std::vector<BLRect> boxes = lanes(painter.type());

    if (_markWidth.value() > 0.0) {
        const BLRect mark{_markX.value(), _box.y + 4.0, _markWidth.value(), _box.h - 8.0};

        painter.round(mark, mark.h / 2.0, palette.accentSoft);
        painter.outline(mark, mark.h / 2.0, 1.0, Theme::alpha(palette.accent, 0.45));
    }

    for (size_t at = 0; at < boxes.size(); ++at) {
        const bool active = _options[at].key == _current;
        const BLRgba32 ink = active ? palette.accent
                           : std::cmp_equal(at, _over) ? palette.text
                                                           : palette.muted;

        painter.label(painter.font(active ? 600 : 400, Theme::fontBody), boxes[at], Align::Centre,
                      _options[at].label, ink);

        if (_options[at].badge) {
            painter.circle(BLPoint{boxes[at].x + boxes[at].w - 11.0, boxes[at].y + 10.0}, 3.0,
                           palette.accent);
        }
    }
}

bool Segmented::press(const Pointer & /*at*/) {
    return enabled();
}

void Segmented::release(const Pointer &at) {
    if (!holds(at.x, at.y) || root() == nullptr) {
        return;
    }

    const std::vector<BLRect> boxes = lanes(root()->type());

    for (size_t index = 0; index < boxes.size(); ++index) {
        if (at.x >= boxes[index].x && at.x < boxes[index].x + boxes[index].w && _selected) {
            _selected(_options[index].key);

            return;
        }
    }
}

void Segmented::hover(const Pointer &at) {
    if (root() == nullptr) {
        return;
    }

    const std::vector<BLRect> boxes = lanes(root()->type());
    int over = -1;

    for (size_t index = 0; index < boxes.size(); ++index) {
        if (at.x >= boxes[index].x && at.x < boxes[index].x + boxes[index].w) {
            over = static_cast<int>(index);
        }
    }

    if (over != _over) {
        _over = over;

        invalidate();
    }
}

void Segmented::leave() {
    Widget::leave();

    _over = -1;
}

bool Segmented::advance(const double now) {
    if (root() != nullptr) {
        const std::vector<BLRect> boxes = lanes(root()->type());

        for (size_t at = 0; at < boxes.size(); ++at) {
            if (_options[at].key != _current) {
                continue;
            }

            if (!_marked) {
                _marked = true;
                _markX.set(static_cast<float>(boxes[at].x));
                _markWidth.set(static_cast<float>(boxes[at].w));
            } else {
                _markX.toward(static_cast<float>(boxes[at].x), now, 0.18, Anim::Curve::CubicOut);
                _markWidth.toward(static_cast<float>(boxes[at].w), now, 0.18,
                                  Anim::Curve::CubicOut);
            }
        }
    }

    _markX.advance(now);
    _markWidth.advance(now);

    invalidate();

    return _markX.live() || _markWidth.live();
}

}
