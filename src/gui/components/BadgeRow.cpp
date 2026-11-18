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

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"

#include "gui/components/BadgeRow.h"
#include "gui/components/Tones.h"

using namespace ttk;

namespace {

BLRect enclose(const BLRect &one, const BLRect &two) {
    if (one.w <= 0.0 || one.h <= 0.0) {
        return two;
    }

    if (two.w <= 0.0 || two.h <= 0.0) {
        return one;
    }

    const double left = std::min(one.x, two.x);
    const double top = std::min(one.y, two.y);

    return BLRect{
            left, top, std::max(one.x + one.w, two.x + two.w) - left, std::max(one.y + one.h, two.y + two.h) - top};
}

double counter() {
    return Glyphs::span(1.4F) + 22.0;
}

void pill(const Painter &painter,
          const BLFont &small,
          const BLRect &box,
          const State::BadgeSpec &badge,
          const BLRgba32 ink,
          const BLRgba32 wash,
          const double shown) {
    painter.round(box, box.h / 2.0, Theme::alpha(wash, shown));
    painter.outline(box, box.h / 2.0, 1.0, Theme::alpha(ink, 0.3 * shown));

    double x = box.x + 11.0;

    if (badge.dot) {
        painter.circle(BLPoint{x + 4.0, box.y + (box.h / 2.0)}, 4.0, Theme::alpha(ink, shown));

        x += 14.0;
    }

    painter.label(small,
                  BLRect{x, box.y, box.x + box.w - 11.0 - x, box.h},
                  Align::Start,
                  badge.text,
                  Theme::alpha(ink, shown));
}

BLRgba32 inked(const BLRgba32 tone) {
    return Theme::palette().dark ? tone : Theme::darker(tone, 0.35);
}

}

namespace components {

void BadgeRow::layOut(const Painter &painter, const BLRect &row, const std::span<const State::BadgeSpec> badges) {
    const BLFont &small = painter.font(600, Theme::fontSmall);
    const size_t count = badges.size();
    const size_t kept = std::min(count, MOST);
    const bool fromEnd = _look.from == From::End;

    const auto slotAt = [&](const size_t order) -> Slot & { return _slots[fromEnd ? count - 1 - order : order]; };

    _row = row;
    _lineHeight = painter.line_height(small);
    _slots.assign(count, Slot{});

    double total = 0.0;

    for (size_t at = 0; at < count; ++at) {
        _slots[at].span = painter.width(small, badges[at].text) + (badges[at].dot ? 14.0 : 0.0) + 22.0;
    }

    for (size_t order = 0; order < kept; ++order) {
        total += slotAt(order).span;
    }

    const double gaps = kept > 1 ? static_cast<double>(kept - 1) * _look.gap : 0.0;

    // Room for the mark only when a badge is left out.
    const bool crowded = total + gaps > row.w || count > MOST;
    const double room = crowded ? row.w - counter() - _look.gap : row.w;

    double x = 0.0;
    bool following = true;

    for (size_t order = 0; order < kept; ++order) {
        Slot &slot = slotAt(order);

        slot.shown = following && (x + slot.span <= room || (_look.keepFirst && order == 0));
        slot.x = fromEnd ? row.x + row.w - x - slot.span : row.x + x;
        following = slot.shown;

        if (slot.shown) {
            x += slot.span + _look.gap;
        }
    }

    _buried = static_cast<int>(std::ranges::count(_slots, false, &Slot::shown));
    _mark = _buried > 0 ? BLRect{fromEnd ? row.x + row.w - x - counter() : row.x + x, row.y, counter(), row.h}
                        : BLRect{};
}

void BadgeRow::paint(const Painter &painter,
                     const BLRect &row,
                     const BLRect &bounds,
                     const std::span<const State::BadgeSpec> badges) {
    layOut(painter, row, badges);

    _bounds = bounds;

    const Theme::Palette &palette = Theme::palette();
    const BLFont &small = painter.font(600, Theme::fontSmall);

    for (size_t at = 0; at < _slots.size(); ++at) {
        const Slot &slot = _slots[at];

        if (slot.shown) {
            const State::BadgeSpec &badge = badges[at];

            pill(painter,
                 small,
                 BLRect{slot.x, row.y, slot.span, row.h},
                 badge,
                 inked(toneOf(badge.kind)),
                 washOf(badge.kind),
                 1.0);
        }
    }

    if (_buried <= 0) {
        return;
    }

    const bool clear = _look.tone == Tone::Clear;
    const BLRgba32 ink = clear ? palette.text : inked(palette.muted);

    painter.round(_mark, _mark.h / 2.0, clear ? palette.borderStrong : palette.mutedSoft);
    painter.outline(_mark, _mark.h / 2.0, 1.0, Theme::alpha(ink, 0.3));

    Glyphs::draw(
            painter.context(),
            Glyphs::Glyph::Dots,
            BLPoint{_mark.x + ((_mark.w - Glyphs::span(1.4F)) / 2.0), _mark.y + ((_mark.h - Glyphs::span(1.4F)) / 2.0)},
            1.4F,
            ink);
}

template <typename Visit>
void BadgeRow::eachFanned(Visit &&visit) const {
    const double limit = _bounds.w - 16.0;
    size_t at = 0;
    int line = 0;

    while (at < _slots.size()) {
        if (_slots[at].shown) {
            ++at;

            continue;
        }

        size_t end = at;
        double wide = 0.0;

        for (size_t next = at; next < _slots.size(); ++next) {
            if (_slots[next].shown) {
                continue;
            }

            const double grown = end == at ? _slots[next].span : wide + _look.gap + _slots[next].span;

            if (end != at && grown > limit) {
                break;
            }

            wide = grown;
            end = next + 1;
        }

        double x = std::clamp(_mark.x + (counter() / 2.0) - (wide / 2.0),
                              _bounds.x + 8.0,
                              std::max(_bounds.x + 8.0, _bounds.x + _bounds.w - wide - 8.0));

        for (; at < end; ++at) {
            if (!_slots[at].shown) {
                visit(at, x, line);

                x += _slots[at].span + _look.gap;
            }
        }

        ++line;
    }
}

void BadgeRow::paintFan(const Painter &painter, const std::span<const State::BadgeSpec> badges) const {
    const double open = _spill.value();

    if (open <= 0.0 || _buried <= 0 || badges.size() != _slots.size()) {
        return;
    }

    const Theme::Palette &palette = Theme::palette();
    const bool clear = _look.tone == Tone::Clear;
    const BLFont &small = painter.font(600, Theme::fontSmall);

    eachFanned([&](const size_t at, const double x, const int line) {
        const State::BadgeSpec &badge = badges[at];

        pill(painter,
             small,
             fanned(x, _slots[at].span, open, line),
             badge,
             inked(clear ? toneOf(badge.kind) : palette.muted),
             clear ? washOf(badge.kind) : palette.mutedSoft,
             open);
    });
}

bool BadgeRow::hover(const double x, const double y, const double now) {
    const bool onMark = _buried > 0 && _mark.w > 0.0 && x >= _mark.x && x < _mark.x + _mark.w && y >= _mark.y
                        && y < _mark.y + _mark.h;

    if (onMark == _over) {
        return false;
    }

    _over = onMark;
    _spill.toward(onMark ? 1.0F : 0.0F, now, 0.23, Anim::Curve::BackOut);

    return true;
}

void BadgeRow::leave(const double now) {
    _spill.toward(0.0F, now, 0.23, Anim::Curve::BackOut);
    _over = false;
}

BLRect BadgeRow::fanned(const double x, const double span, const double open, const int line) const {
    const double fromX = _mark.x + ((counter() - span) / 2.0);
    const double fromY = _mark.y - 2.0;
    const double toY = _mark.y - 34.0 - (32.0 * line);

    const double grown = 0.4 + (open * 0.6);
    const double tall = 26.0 * grown;
    const double y = fromY + ((toY - fromY) * open);

    // Fanned down, the same path is mirrored across the middle of the mark. The curve overshoots the end, and the far
    // edge still stops at the bounds.
    const double up = std::max(y, _bounds.y);
    const double down = std::min((2.0 * _mark.y) + _mark.h - y - tall, _bounds.y + _bounds.h - tall);
    const double placed = _look.fan == Fan::Up ? up : down;

    return BLRect{fromX + ((x - fromX) * open), placed, span * grown, tall};
}

BLRect BadgeRow::fanBox(const double open) const {
    if (open <= 0.0 || _buried <= 0 || _mark.w <= 0.0) {
        return {};
    }

    BLRect box{};

    eachFanned([&](const size_t at, const double x, const int line) {
        // A small pill still has its text at full size, centred on it.
        const BLRect pill = fanned(x, _slots[at].span, open, line);
        const double reach = std::max(0.0, (_lineHeight - pill.h) / 2.0);

        box = enclose(box, BLRect{pill.x, pill.y - reach, pill.w, pill.h + (2.0 * reach)});
    });

    return box;
}

void BadgeRow::shift(const double dx, const double dy) {
    _row.x += dx;
    _row.y += dy;
    _bounds.x += dx;
    _bounds.y += dy;
    _mark.x += dx;
    _mark.y += dy;

    for (Slot &slot : _slots) {
        slot.x += dx;
    }
}

int BadgeRow::at(const double x, const double y) const {
    if (y < _row.y || y >= _row.y + _row.h) {
        return -1;
    }

    for (size_t which = 0; which < _slots.size(); ++which) {
        const Slot &slot = _slots[which];

        if (slot.shown && x >= slot.x && x < slot.x + slot.span) {
            return static_cast<int>(which);
        }
    }

    return -1;
}

BLRect BadgeRow::extent() const {
    BLRect box = _mark;

    for (const Slot &slot : _slots) {
        if (slot.shown) {
            box = enclose(box, BLRect{slot.x, _row.y, slot.span, _row.h});
        }
    }

    return box;
}

}
