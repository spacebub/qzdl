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

#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/layout/Panel.h"
#include "gui/toolkit/overlays/Sheet.h"

namespace toolkit {

Label *Sheet::heading(Box *into, const std::string &text) {
    Label *made = into->append(std::make_unique<Label>(text));

    made->font(Theme::of().headingWeight, Theme::fontLarge)->tone(Theme::of().text)->wrap();

    return made;
}

Label *Sheet::body(Box *into, const std::string &text) {
    Label *made = into->append(std::make_unique<Label>(text));

    made->font(400, Theme::fontBody)->tone(Theme::of().muted)->wrap();

    return made;
}

// --- Sheet ---------------------------------------------------------------------

Sheet::Sheet() {
    _takesPointer = true;

    _card = append(std::make_unique<Panel>());
    _card->rounding = Theme::radius;

    setOpen(true);
}

void Sheet::setOpen(const bool open) {
    if (_open == open) {
        return;
    }

    _open = open;

    setVisible(open);

    if (open) {
        _grown.set(0.95F);
        _grow = true;
    }

    invalidate();
}

void Sheet::arrange(Typeface &type) {
    if (_grow) {
        _grow = false;

        _grown.run(1.0F, now(), 0.14, Anim::Curve::CubicOut);

        animate();
    }

    const double wide = std::min(wanted, _box.w - 48.0);
    const double want = tall > 0.0 ? tall : _card->naturalHeight(type, wide);
    const double high = std::min(want, _box.h - 48.0);

    _card->place(BLRect{_box.x + ((_box.w - wide) / 2.0), _box.y + ((_box.h - high) / 2.0), wide,
                        high},
                 type);
}

void Sheet::paint(const Painter &painter) {
    painter.fill(_box, Theme::of().scrim);

    // The card grows into place; Blend2D can scale, so the transform is a real one.
    const double grown = _grown.value() > 0.0 ? _grown.value() : 1.0;
    const bool growing = grown < 0.999;

    if (growing) {
        const BLRect card = _card->box();

        painter.context().save();
        painter.context().scale(grown, grown);
        painter.context().translate((card.x + (card.w / 2.0)) * ((1.0 / grown) - 1.0),
                                    (card.y + (card.h / 2.0)) * ((1.0 / grown) - 1.0));
    }

    Widget::paint(painter);
    paintOver(painter);

    if (growing) {
        painter.context().restore();
    }
}

Widget *Sheet::at(const double x, const double y) {
    if (!visible()) {
        return nullptr;
    }

    if (Widget *found = Widget::at(x, y); found != nullptr) {
        return found;
    }

    // Anything the card does not want is still the sheet's, so the page under it
    // never answers the pointer.
    return this;
}

bool Sheet::press(const Pointer &at) {
    _onScrim = !_card->holds(at.x, at.y) && (root() == nullptr || !root()->justDismissed());

    return true;
}

void Sheet::release(const Pointer &at) {
    if (_onScrim && !_card->holds(at.x, at.y) && dismissed) {
        dismissed();
    }

    _onScrim = false;
}

bool Sheet::advance(const double now) {
    _grown.advance(now);

    invalidate();

    return _grown.live();
}

}
