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
#include <cmath>
#include <numbers>

#include "gui/components/LibraryCard.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Mark.h"
#include "gui/draw/Paint.h"
#include "gui/draw/Theme.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/StatusIndicator.h"
#include "gui/toolkit/overlays/Menu.h"

namespace {

// The same five tones the engine cards use.
BLRgba32 toneOf(const State::BadgeKind kind) {
    const Theme::Palette &palette = Theme::of();

    switch (kind) {
        case State::BadgeKind::Danger:
            return palette.danger;
        case State::BadgeKind::Success:
            return palette.success;
        case State::BadgeKind::Warning:
            return palette.warning;
        case State::BadgeKind::Muted:
            return palette.muted;
        case State::BadgeKind::None:
            break;
    }

    return palette.accent;
}

BLRgba32 washOf(const State::BadgeKind kind) {
    const Theme::Palette &palette = Theme::of();

    switch (kind) {
        case State::BadgeKind::Danger:
            return palette.dangerSoft;
        case State::BadgeKind::Success:
            return palette.successSoft;
        case State::BadgeKind::Warning:
            return palette.warningSoft;
        case State::BadgeKind::Muted:
            return palette.mutedSoft;
        case State::BadgeKind::None:
            break;
    }

    return palette.accentSoft;
}

constexpr double ART = Theme::cardArt;
constexpr double SLACK = 6.0;

// The card's own shadow, near and thin: a wider one reads as a cloud under the card
// rather than an edge to it.
constexpr double BLUR_REST = 4.0;
constexpr double BLUR_LIT = 10.0;
constexpr double DROP_REST = 2.0;
constexpr double DROP_LIT = 6.0;
constexpr double CAST = 1.0;

// How far a hovered card lifts, and how far a pressed one gives.
constexpr double LIFT = 3.0;
constexpr double PUSH = 1.0;

// How far the card turns to meet the pointer, how far the eye is from it, and
// how finely the turned face is laid down.
constexpr double TILT = 6.0 * std::numbers::pi / 180.0;
constexpr double EYE = 800.0;
constexpr double CELL = 48.0;

// How long the turn takes to close most of the gap to the pointer.
constexpr double FOLLOW = 0.05;

toolkit::StatusIndicator::Status statusOf(const State::RunState status) {
    using Shown = toolkit::StatusIndicator::Status;

    switch (status) {
        case State::RunState::Launching:
            return Shown::Launching;
        case State::RunState::Running:
            return Shown::Running;
        case State::RunState::Stopping:
            return Shown::Stopping;
        case State::RunState::Closed:
            return Shown::Closed;
        case State::RunState::Failed:
            return Shown::Failed;
        case State::RunState::None:
            break;
    }

    return Shown::Empty;
}

}

namespace components {

using namespace toolkit;

LibraryCard::LibraryCard() {
    _takesPointer = true;
    cursor = Cursor::Pointer;
}

Cursor LibraryCard::cursorAt(double /*x*/, double /*y*/) const {
    return _dragging ? Cursor::Grabbing : Cursor::Pointer;
}

BLRect LibraryCard::spread(const BLRect &box) {
    // The widest the card ever gets: the rise is a scale, and a spread that shrank
    // with it would stop covering the shadow the frame before drew.
    const double out = Paint::bleed(BLUR_LIT) + (box.w * 0.03);

    return BLRect{box.x - out, box.y - out, box.w + (out * 2.0), box.h + (out * 2.0) + DROP_LIT};
}

BLRect LibraryCard::drawn() const {
    return spread(BLRect{_box.x + carryX + _slideX.value(), _box.y + carryY + _slideY.value(),
                         _box.w, _box.h});
}

BLRect LibraryCard::face() const {
    // The rise is a lift, not a scale, and it lands on whole pixels: a scaled or
    // fractionally placed face has everything under it resampled, which is most
    // of what a hover would cost.
    const double lift = std::round((_rise.value() * LIFT) - (_press.value() * PUSH));

    return BLRect{std::round(_box.x + carryX + _slideX.value()),
                  std::round(_box.y + carryY + _slideY.value()) - lift, _box.w, _box.h};
}

void LibraryCard::slideFrom(const double x, const double y, const double now) {
    _slideX.set(static_cast<float>(x));
    _slideY.set(static_cast<float>(y));

    _slideX.run(0.0F, now, 0.19, Anim::Curve::CubicOut);
    _slideY.run(0.0F, now, 0.19, Anim::Curve::CubicOut);

    animate();
}

BLRect LibraryCard::artBox() const {
    const BLRect card = face();

    return BLRect{card.x, card.y, card.w, ART * (card.h / _box.h)};
}

BLRect LibraryCard::playBox() const {
    const BLRect art = artBox();

    return BLRect{art.x + ((art.w - 48.0) / 2.0), art.y + ((art.h - 48.0) / 2.0), 48.0, 48.0};
}

BLRect LibraryCard::moreBox() const {
    const BLRect card = face();

    return BLRect{card.x + card.w - 36.0, card.y + card.h - 36.0, 26.0, 26.0};
}

BLRect LibraryCard::metaBox() const {
    const BLRect card = face();
    const BLRect art = artBox();

    return BLRect{card.x + 14.0, art.y + art.h + 12.0, card.w - 28.0, 44.0};
}

BLRect LibraryCard::badgeRow() const {
    const BLRect card = face();

    return BLRect{card.x + 14.0, card.y + card.h - 34.0, card.w - 14.0 - 44.0, 22.0};
}

// --- the art -------------------------------------------------------------------

void LibraryCard::ground(const int wide, const int tall) {
    const BLImage shot = artwork ? artwork(artKey) : BLImage();
    const std::string key = artKey + '\n' + std::to_string(shot.width()) + 'x'
        + std::to_string(shot.height()) + (playable ? "\np" : "");

    if (_groundWide == wide && _groundTall == tall && _groundKey == key) {
        return;
    }

    _groundWide = wide;
    _groundTall = tall;
    _groundKey = key;
    _drawn = !shot.is_empty();

    if (_rest.create(wide, tall, BL_FORMAT_PRGB32) != BL_SUCCESS
        || _lit.create(wide, tall, BL_FORMAT_PRGB32) != BL_SUCCESS) {
        return;
    }

    const Theme::Palette &palette = Theme::of();
    const BLRect whole{0.0, 0.0, static_cast<double>(wide), static_cast<double>(tall)};

    // The layers both sprites share.
    BLImage common;

    if (common.create(wide, tall, BL_FORMAT_PRGB32) != BL_SUCCESS) {
        return;
    }

    {
        BLContext into(common);

        into.set_comp_op(BL_COMP_OP_SRC_COPY);
        into.fill_all(BLRgba32(0x00000000));
        into.set_comp_op(BL_COMP_OP_SRC_OVER);

        BLGradient down = Paint::down(whole);

        down.add_stop(0.0, palette.artTop);
        down.add_stop(0.55, palette.artMiddle);
        down.add_stop(1.0, palette.artBottom);

        into.fill_rect(whole, down);

        // The sheen off the left edge.
        BLGradient sheen(BLLinearGradientValues{0.0, 0.0, whole.w, 0.0});

        sheen.add_stop(0.0, BLRgba32(0x0effffff));
        sheen.add_stop(0.7, BLRgba32(0x00ffffff));

        into.fill_rect(whole, sheen);

        if (_drawn) {
            Paint::cover(into, whole, shot);
        } else {
            // The mark, where there is no title screen.
            const BLImage &mark = Mark::of(256);

            const double side = std::min(whole.h * 0.44, 64.0);
            const double lift = caption.empty() ? 0.0 : -9.0;

            if (!mark.is_empty()) {
                into.set_global_alpha(0.72);
                into.blit_image(BLRect{(whole.w - side) / 2.0, ((whole.h - side) / 2.0) + lift,
                                       side, side},
                                mark);
                into.set_global_alpha(1.0);
            }
        }

        if (!caption.empty()) {
            if (_drawn) {
                const BLRect scrim{0.0, whole.h - 46.0, whole.w, 46.0};
                BLGradient veil = Paint::down(scrim);

                veil.add_stop(0.0, BLRgba32(0x00000000));
                veil.add_stop(1.0, BLRgba32(0xb3000000));

                into.fill_rect(scrim, veil);
            }
        }

        // The hairline along the top.
        into.fill_rect(BLRect{Theme::radius, 0.0, whole.w - (Theme::radius * 2.0), 1.0},
                       BLRgba32(0x14ffffff));

        into.end();
    }

    // The two states, which differ only in the ember's strength and the dim.
    const BLRectI slice{0, 0, wide, tall};

    for (int state = 0; state < 2; ++state) {
        BLImage &into_image = state == 0 ? _rest : _lit;
        const double strength = state == 0 ? 0.0 : 1.0;

        BLContext into(into_image);

        into.set_comp_op(BL_COMP_OP_SRC_COPY);
        into.blit_image(BLPoint{0.0, 0.0}, common, slice);
        into.set_comp_op(BL_COMP_OP_SRC_OVER);

        const double reach = std::max(whole.w, whole.h) * 0.95;
        const BLPoint middle{whole.w * 0.3, whole.h * 1.05};

        if (strength > 0.0) {
            BLGradient ember(
                BLRadialGradientValues{middle.x, middle.y, middle.x, middle.y, reach});

            ember.add_stop(0.0, Theme::alpha(palette.ember, 0.38 * strength));
            ember.add_stop(0.45, Theme::alpha(palette.ember, 0.12 * strength));
            ember.add_stop(1.0, Theme::alpha(palette.ember, 0.0));

            into.fill_rect(whole, ember);
        }

        // The dim under the play badge, which only the lit sprite carries.
        if (state == 1 && playable) {
            into.fill_rect(whole, Theme::alpha(palette.artBottom, 0.35));
        }

        into.end();
    }
}

void LibraryCard::readyArt() {
    // Built at the size the card has at rest: the rise is a 2% stretch of the
    // sprites rather than a rebuild, which would hold the art blurred for a beat.
    const int wide = static_cast<int>(std::lround(_box.w));
    const int tall = static_cast<int>(std::lround(ART));

    if (wide <= 0 || tall <= 0) {
        return;
    }

    if (wide != _wantedWide || tall != _wantedTall) {
        _wantedWide = wide;
        _wantedTall = tall;
        _resized = now();
    }

    // A resize hands over a new size every frame, and rebuilding the sprites for
    // each of them is most of what a resize costs, so the old ones are stretched
    // until the size has held still. A title screen arriving is not a resize: the
    // size has not moved, and it is drawn the moment it lands.
    _holding = !_rest.is_empty() && (wide != _groundWide || tall != _groundTall)
        && now() - _resized <= 0.12;

    ground(_holding ? _groundWide : wide, _holding ? _groundTall : tall);

    // Whatever was stretched is rebuilt at the right size a moment later.
    if (_holding) {
        animate();
    }
}

void LibraryCard::paintArt(const Painter &painter, const BLRect &box) {
    if (_rest.is_empty() || box.w <= 0.0 || box.h <= 0.0) {
        return;
    }

    const double lit = _rise.value();

    // The art is drawn as a pattern under the face's own outline rather than cut
    // out of a sprite: a mask leaves whatever the face painted showing through the
    // corner it chipped away, and it has to be the ground behind the card there.
    BLPath shape;

    shape.move_to(box.x, box.y + box.h);
    shape.line_to(box.x, box.y + Theme::radius);
    shape.arc_quadrant_to(box.x, box.y, box.x + Theme::radius, box.y);
    shape.line_to(box.x + box.w - Theme::radius, box.y);
    shape.arc_quadrant_to(box.x + box.w, box.y, box.x + box.w, box.y + Theme::radius);
    shape.line_to(box.x + box.w, box.y + box.h);
    shape.close();

    BLMatrix2D at = BLMatrix2D::make_translation(box.x, box.y);

    at.scale(box.w / _rest.width(), box.h / _rest.height());

    painter.context().fill_path(shape, BLPattern(_rest, BL_EXTEND_MODE_PAD, at));

    if (lit > 0.0) {
        painter.context().set_global_alpha(lit);
        painter.context().fill_path(shape, BLPattern(_lit, BL_EXTEND_MODE_PAD, at));
        painter.context().set_global_alpha(1.0);
    }

    // Type stays sharp over a stretched sprite, and the art is dark in both shades.
    // Hung off the card's middle, which the rise scales about, so it holds still.
    if (!caption.empty()) {
        const Theme::Palette &palette = Theme::of();
        const BLFont &small = painter.font(600, Theme::fontTiny);
        const std::string said = painter.type().elide(small, caption,
                                                      static_cast<float>(box.w - 24.0), 1.4F);
        const double taken = painter.type().widthTracked(small, said, 1.4F);
        const double line = painter.lineHeight(small);
        const double middle = face().y + (face().h / 2.0);

        painter.tracked(small,
                        BLPoint{box.x + ((box.w - taken) / 2.0),
                                middle - (_box.h / 2.0) + ART - line - 14.0},
                        said,
                        Theme::alpha(_drawn ? palette.artText : palette.steel,
                                     _drawn ? 0.95 : 0.75),
                        1.4);
    }
}

void LibraryCard::paintPlay(const Painter &painter, const BLRect &box) {
    const double shown = _play.value() * (playable ? 1.0 : 0.0);

    if (shown <= 0.01) {
        return;
    }

    const Theme::Palette &palette = Theme::of();

    // Grows out of its middle as it fades in, and goes back the same way.
    const double pop = shown * (1.0 - (_press.value() * 0.06));

    const BLPoint middle{box.x + (box.w / 2.0), box.y + (box.h / 2.0)};
    const double halo = box.w * 1.7 * pop / 2.0;

    painter.circle(middle, halo,
                   Theme::alpha(palette.ember, (_badge.value() > 0.0 ? 0.3 : 0.18) * shown));

    const double side = box.w * pop;

    BLGradient sweep(BLLinearGradientValues{middle.x, middle.y + (side / 2.0), middle.x,
                                            middle.y - (side / 2.0)});

    sweep.add_stop(0.0, Theme::alpha(palette.ember, shown));
    sweep.add_stop(1.0, Theme::alpha(palette.emberHigh, shown));

    painter.context().fill_circle(middle.x, middle.y, side / 2.0, sweep);
    painter.context().set_stroke_width(1.0);
    painter.context().stroke_circle(middle.x, middle.y, (side / 2.0) - 0.5,
                                    Theme::alpha(BLRgba32(0xffffffff), 0.28 * shown));

    const auto weight = static_cast<float>(1.5 * pop);
    const double glyph = Glyphs::span(weight);

    Glyphs::draw(painter.context(), Glyphs::Glyph::Play,
                 BLPoint{middle.x - (glyph / 2.0) + (2.0 * pop), middle.y - (glyph / 2.0)}, weight,
                 Theme::alpha(palette.artEdge, shown));
}

void LibraryCard::paintState(const Painter &painter, const BLRect &box) {
    if (status == State::RunState::None) {
        return;
    }

    const StatusIndicator::Status shown = statusOf(status);

    _statePill = StatusIndicator::render(painter, BLPoint{box.x + 10.0, box.y + 10.0}, shown, _dim);

    hint = StatusIndicator::sayOf(shown, statusReason) + " Click to see what it printed.";
}

void LibraryCard::paintMeta(const Painter &painter, const BLRect &box) const {
    const Theme::Palette &palette = Theme::of();
    const BLFont &heading = painter.font(palette.headingWeight, Theme::fontMedium);

    double y = box.y;

    painter.label(heading, BLRect{box.x, y, box.w - 26.0, painter.lineHeight(heading)},
                  Align::Start, title, palette.text);

    y += painter.lineHeight(heading) + 2.0;

    if (!subtitle.empty()) {
        const BLFont &small = painter.font(400, Theme::fontSmall);

        painter.label(small, BLRect{box.x, y, box.w, painter.lineHeight(small)}, Align::Start,
                      subtitle, palette.faint);
    }
}

std::vector<LibraryCard::Slot> LibraryCard::slots(const Painter &painter, const BLRect &row,
                                                  int &buried) const {
    const BLFont &small = painter.font(600, Theme::fontSmall);
    const size_t shown = std::min<size_t>(badges.size(), 5);

    std::vector<Slot> out;
    double total = 0.0;

    out.reserve(shown);

    for (size_t at = 0; at < shown; ++at) {
        Slot slot;

        slot.span = painter.width(small, badges[at].text) + (badges[at].dot ? 14.0 : 0.0) + 22.0;
        total += slot.span;

        out.push_back(slot);
    }

    const double gaps = shown > 1 ? static_cast<double>(shown - 1) * 6.0 : 0.0;

    // Room for the mark only when a badge is left out.
    const double counter = Glyphs::span(1.4F) + 22.0;
    const bool crowded = total + gaps > row.w || badges.size() > 5;
    const double room = crowded ? row.w - counter - 6.0 : row.w;

    double x = 0.0;
    bool following = true;

    for (auto & at : out) {
        at.x = x;
        at.shown = following && x + at.span <= room;
        following = at.shown;

        if (at.shown) {
            x += at.span + 6.0;
        }
    }

    buried = static_cast<int>(badges.size()) - static_cast<int>(shown);

    for (const Slot &slot : out) {
        if (!slot.shown) {
            ++buried;
        }
    }

    return out;
}

void LibraryCard::paintBadges(const Painter &painter, const BLRect &row) {
    int buried = 0;
    const std::vector<Slot> made = slots(painter, row, buried);

    _slots = made;
    _buried = buried;
    _mark = BLRect{};

    const Theme::Palette &palette = Theme::of();
    const BLFont &small = painter.font(600, Theme::fontSmall);

    double next = 0.0;

    for (size_t at = 0; at < made.size(); ++at) {
        if (!made[at].shown) {
            continue;
        }

        const State::BadgeSpec &badge = badges[at];
        const BLRect pill{row.x + made[at].x, row.y, made[at].span, row.h};
        const BLRgba32 tone = toneOf(badge.kind);
        const BLRgba32 wash = washOf(badge.kind);
        const BLRgba32 ink = palette.dark ? tone : Theme::darker(tone, 0.35);

        painter.round(pill, pill.h / 2.0, wash);
        painter.outline(pill, pill.h / 2.0, 1.0, Theme::alpha(ink, 0.3));

        double x = pill.x + 11.0;

        if (badge.dot) {
            painter.circle(BLPoint{x + 4.0, pill.y + (pill.h / 2.0)}, 4.0, ink);

            x += 14.0;
        }

        painter.label(small, BLRect{x, pill.y, pill.x + pill.w - 11.0 - x, pill.h}, Align::Start,
                      badge.text, ink);

        next = made[at].x + made[at].span + 6.0;
    }

    if (buried <= 0) {
        return;
    }

    const double counter = Glyphs::span(1.4F) + 22.0;

    _mark = BLRect{row.x + next, row.y, counter, row.h};

    painter.round(_mark, _mark.h / 2.0, palette.mutedSoft);
    painter.outline(_mark, _mark.h / 2.0, 1.0,
                    Theme::alpha(palette.dark ? palette.muted
                                              : Theme::darker(palette.muted, 0.35),
                                 0.3));

    Glyphs::draw(painter.context(), Glyphs::Glyph::Dots,
                 BLPoint{_mark.x + ((_mark.w - Glyphs::span(1.4F)) / 2.0),
                         _mark.y + ((_mark.h - Glyphs::span(1.4F)) / 2.0)},
                 1.4F, palette.dark ? palette.muted : Theme::darker(palette.muted, 0.35));

    // The ones left out, fanned above the mark.
    const double open = _spill.value();

    if (open <= 0.0) {
        return;
    }

    double fan = 0.0;

    for (const auto & at : made) {
        if (!at.shown) {
            fan += at.span + 6.0;
        }
    }

    fan = std::max(0.0, fan - 6.0);

    const BLRect card = face();
    const double left = std::clamp(_mark.x + (counter / 2.0) - (fan / 2.0), card.x + 8.0,
                                   std::max(card.x + 8.0, card.x + card.w - fan - 8.0));

    double x = left;

    for (size_t at = 0; at < made.size(); ++at) {
        if (made[at].shown) {
            continue;
        }

        const State::BadgeSpec &badge = badges[at];
        const double span = made[at].span;

        const double fromX = _mark.x + ((counter - span) / 2.0);
        const double fromY = row.y - 2.0;
        const double toY = row.y - 34.0;

        const double grown = 0.4 + (open * 0.6);
        const BLRect pill{fromX + ((x - fromX) * open), fromY + ((toY - fromY) * open), span * grown,
                          26.0 * grown};

        const BLRgba32 ink = palette.dark ? palette.muted : Theme::darker(palette.muted, 0.35);

        painter.round(pill, pill.h / 2.0, Theme::alpha(palette.mutedSoft, open));
        painter.outline(pill, pill.h / 2.0, 1.0, Theme::alpha(ink, 0.3 * open));

        double inner = pill.x + 11.0;

        if (badge.dot) {
            painter.circle(BLPoint{inner + 4.0, pill.y + (pill.h / 2.0)}, 4.0,
                           Theme::alpha(ink, open));

            inner += 14.0;
        }

        painter.label(small, BLRect{inner, pill.y, pill.x + pill.w - 11.0 - inner, pill.h},
                      Align::Start, badge.text, Theme::alpha(ink, open));

        x += span + 6.0;
    }
}

void LibraryCard::paint(const Painter &painter) {
    readyArt();

    const BLRect card = face();
    const bool turning = std::abs(_tiltX) >= 0.01 || std::abs(_tiltY) >= 0.01;
    const bool moving = turning || _rise.value() > 0.0 || _press.value() > 0.0
        || _play.value() > 0.0 || _spill.value() > 0.0;

    if (!moving) {
        paintStill(painter, card);

        return;
    }

    paintShadow(painter, card);

    if (turning) {
        paintTurned(painter, card);
    } else {
        paintFace(painter, card);
    }
}

void LibraryCard::paintShadow(const Painter &painter, const BLRect &card) const {
    // The shadow, as a sprite: the middle of it is about to be covered by an
    // opaque face, so it is blitted as a frame rather than a filled rectangle.
    const double lit = _rise.value();
    const double blur = std::round((BLUR_REST + ((BLUR_LIT - BLUR_REST) * lit)) / 2.0) * 2.0;
    const double drop = DROP_REST + ((DROP_LIT - DROP_REST) * lit);

    const BLImage &cast = Paint::shadow(static_cast<int>(std::lround(_box.w)),
                                        static_cast<int>(std::lround(_box.h)), Theme::radius,
                                        blur, Theme::alpha(Theme::of().shadow, CAST));

    if (cast.is_empty()) {
        return;
    }

    const double out = Paint::bleed(blur);

    painter.context().blit_image(BLPoint{card.x - out, card.y - out + std::round(drop)}, cast);
}

std::string LibraryCard::stillKey(const BLRectI &sheet) const {
    std::string key = title + '\n' + subtitle + '\n' + caption + '\n' + _groundKey + '\n'
        + std::to_string(static_cast<int>(status)) + '\n' + statusReason + '\n'
        + primary + '\n';

    key += playable ? 'p' : '-';
    key += _dim ? 'd' : '-';
    key += _drawn ? 'a' : '-';
    key += actions.empty() ? '-' : 'm';
    key += Theme::dark() ? 'D' : 'L';

    for (const State::BadgeSpec &badge : badges) {
        key += '\n' + badge.text + '\n'
            + std::to_string(static_cast<int>(badge.kind)) + (badge.dot ? '.' : '-');
    }

    key += '\n' + std::to_string(sheet.w) + 'x' + std::to_string(sheet.h);

    return key;
}

void LibraryCard::paintStill(const Painter &painter, const BLRect &card) {
    const BLRect area = spread(card);
    const BLRectI sheet{static_cast<int>(std::floor(area.x)), static_cast<int>(std::floor(area.y)),
                        static_cast<int>(std::ceil(area.w)) + 1,
                        static_cast<int>(std::ceil(area.h)) + 1};

    if (const std::string key = stillKey(sheet); key != _stillKey || _still.is_empty()) {
        if ((_still.width() != sheet.w || _still.height() != sheet.h)
            && _still.create(sheet.w, sheet.h, BL_FORMAT_PRGB32) != BL_SUCCESS) {
            paintShadow(painter, card);
            paintFace(painter, card);

            return;
        }

        BLContext into(_still);

        into.clear_all();
        into.translate(-sheet.x, -sheet.y);

        const Painter flat(into, painter.type(), sheet);

        paintShadow(flat, card);
        paintFace(flat, card);

        into.end();

        _stillKey = key;
        _stillAt = BLPoint{static_cast<double>(sheet.x), static_cast<double>(sheet.y)};
    } else if (_stillAt.x != sheet.x || _stillAt.y != sheet.y) {
        // The pill and the badge mark were placed when the image was made; a
        // scroll since has moved the card under them.
        const double dx = sheet.x - _stillAt.x;
        const double dy = sheet.y - _stillAt.y;

        _statePill.x += dx;
        _statePill.y += dy;
        _mark.x += dx;
        _mark.y += dy;

        _stillAt = BLPoint{static_cast<double>(sheet.x), static_cast<double>(sheet.y)};
    }

    painter.context().blit_image(BLPoint{static_cast<double>(sheet.x),
                                         static_cast<double>(sheet.y)},
                                 _still);
}

void LibraryCard::paintFace(const Painter &painter, const BLRect &card) {
    const Theme::Palette &palette = Theme::of();
    const double lit = _rise.value();

    painter.round(card, Theme::radius, palette.surface);

    paintArt(painter, artBox());
    paintState(painter, artBox());
    paintPlay(painter, playBox());
    paintMeta(painter, metaBox());
    paintBadges(painter, badgeRow());

    // A light that follows the pointer, over everything but the badges.
    if (lit > 0.0) {
        const double reach = card.w * 0.62;

        BLGradient glow(BLRadialGradientValues{_glowX, _glowY, _glowX, _glowY, reach});

        glow.add_stop(0.0, Theme::alpha(BLRgba32(0xffffffff), 0.11 * lit));
        glow.add_stop(0.4, Theme::alpha(BLRgba32(0xffffffff), 0.04 * lit));
        glow.add_stop(0.72, BLRgba32(0x00ffffff));

        painter.push(card);
        painter.context().fill_rect(card, glow);
        painter.pop();
    }

    painter.outline(card, Theme::radius, 1.0,
                    Theme::mix(palette.border, palette.borderStrong, lit));

    if (!actions.empty() && lit > 0.0) {
        const BLRect more = moreBox();

        if (_overMore) {
            painter.round(more, Theme::radiusSmall, Theme::alpha(palette.hover, lit));
        }

        const double side = Glyphs::span(1.2F);

        Glyphs::draw(painter.context(), Glyphs::Glyph::Dots,
                     BLPoint{more.x + ((more.w - side) / 2.0), more.y + ((more.h - side) / 2.0)},
                     1.2F, Theme::alpha(_overMore ? palette.text : palette.muted, lit));
    }
}

BLPoint LibraryCard::turned(const BLPoint at, const BLPoint middle) const {
    // The edge under the pointer comes forward: about the vertical axis for the
    // pointer's x, about the horizontal one for its y.
    const double beta = _tiltX * TILT;
    const double alpha = -_tiltY * TILT;

    const double u = at.x - middle.x;
    const double v = at.y - middle.y;

    const double x = u * std::cos(beta);
    const double z = -u * std::sin(beta);
    const double y = (v * std::cos(alpha)) - (z * std::sin(alpha));
    const double depth = (v * std::sin(alpha)) + (z * std::cos(alpha));
    const double near = EYE / (EYE + depth);

    return BLPoint{middle.x + (x * near), middle.y + (y * near)};
}

void LibraryCard::paintTurned(const Painter &painter, const BLRect &card) {
    const BLRectI sheet{static_cast<int>(std::floor(card.x)) - 1,
                        static_cast<int>(std::floor(card.y)) - 1,
                        static_cast<int>(std::ceil(card.w)) + 3,
                        static_cast<int>(std::ceil(card.h)) + 3};

    if (_sheet.width() != sheet.w || _sheet.height() != sheet.h) {
        if (_sheet.create(sheet.w, sheet.h, BL_FORMAT_PRGB32) != BL_SUCCESS) {
            paintFace(painter, card);

            return;
        }
    }

    {
        BLContext into(_sheet);

        into.clear_all();
        into.translate(-sheet.x, -sheet.y);

        const Painter flat(into, painter.type(), sheet);

        paintFace(flat, card);

        into.end();
    }

    const BLPoint middle{card.x + (card.w / 2.0), card.y + (card.h / 2.0)};
    const BLPattern skin(_sheet, BL_EXTEND_MODE_PAD,
                         BLMatrix2D::make_translation(sheet.x, sheet.y));
    BLContext &context = painter.context();

    // Resampling is the dear part; a card on its way back down gets the cheap kind.
    context.set_pattern_quality(hovered() ? BL_PATTERN_QUALITY_BILINEAR
                                          : BL_PATTERN_QUALITY_NEAREST);

    for (double y = sheet.y; y < sheet.y + sheet.h; y += CELL) {
        const double tall = std::min(CELL, sheet.y + sheet.h - y);

        for (double x = sheet.x; x < sheet.x + sheet.w; x += CELL) {
            const double wide = std::min(CELL, sheet.x + sheet.w - x);

            const BLPoint corner = turned(BLPoint{x, y}, middle);
            const BLPoint right = turned(BLPoint{x + wide, y}, middle);
            const BLPoint below = turned(BLPoint{x, y + tall}, middle);

            const double m00 = (right.x - corner.x) / wide;
            const double m01 = (right.y - corner.y) / wide;
            const double m10 = (below.x - corner.x) / tall;
            const double m11 = (below.y - corner.y) / tall;

            const BLMatrix2D at(m00, m01, m10, m11, corner.x - (m00 * x) - (m10 * y),
                                corner.y - (m01 * x) - (m11 * y));

            context.save();
            context.apply_transform(at);

            // Half a pixel over each edge, so the seams between cells close.
            context.fill_rect(BLRect{x - 0.5, y - 0.5, wide + 1.0, tall + 1.0}, skin);

            context.restore();
        }
    }

    context.set_pattern_quality(BL_PATTERN_QUALITY_BILINEAR);
}

// --- the pointer ---------------------------------------------------------------

bool LibraryCard::press(const Pointer &at) {
    _armed = true;
    _dragging = false;
    _pressX = at.x;
    _pressY = at.y;

    _press.run(1.0F, now(), 0.17, Anim::Curve::CubicOut);
    animate();

    if (pressedDown) {
        pressedDown();
    }

    return true;
}

void LibraryCard::drag(const Pointer &at) {
    _glowX = at.x;
    _glowY = at.y;

    if (!draggable || !_armed) {
        return;
    }

    if (!_dragging) {
        if (std::abs(at.x - _pressX) < SLACK && std::abs(at.y - _pressY) < SLACK) {
            return;
        }

        _dragging = true;

        if (dragStarted) {
            dragStarted(_pressX, _pressY);
        }
    }

    if (dragMoved) {
        dragMoved(at.x, at.y);
    }
}

void LibraryCard::release(const Pointer &at) {
    _press.run(0.0F, now(), 0.17, Anim::Curve::CubicOut);
    animate();

    const bool carried = _dragging;

    _armed = false;
    _dragging = false;

    if (carried) {
        if (dragEnded) {
            dragEnded();
        }

        return;
    }

    if (!holds(at.x, at.y)) {
        return;
    }

    if (!actions.empty() && moreBox().x <= at.x && at.x < moreBox().x + moreBox().w
        && at.y >= moreBox().y && at.y < moreBox().y + moreBox().h) {
        showMenu();

        return;
    }

    if (status != State::RunState::None && _statePill.w > 0.0 && at.x >= _statePill.x
        && at.x < _statePill.x + _statePill.w && at.y >= _statePill.y
        && at.y < _statePill.y + _statePill.h && logRequested) {
        logRequested();

        return;
    }

    if (playable && _rise.value() > 0.0) {
        const BLRect play = playBox();

        if (at.x >= play.x && at.x < play.x + play.w && at.y >= play.y
            && at.y < play.y + play.h) {
            if (played) {
                played();
            }

            return;
        }
    }

    if (primary == "play") {
        if (played) {
            played();
        }
    } else if (opened) {
        opened();
    }
}

void LibraryCard::enter() {
    Widget::enter();

    _rise.toward(1.0F, now(), 0.17, Anim::Curve::CubicOut);
    _play.toward(1.0F, now(), 0.2, Anim::Curve::CubicOut);
    animate();
}

void LibraryCard::leave() {
    Widget::leave();

    _rise.toward(0.0F, now(), 0.17, Anim::Curve::CubicOut);
    _play.toward(0.0F, now(), 0.2, Anim::Curve::CubicOut);
    _spill.toward(0.0F, now(), 0.23, Anim::Curve::BackOut);
    _aimX = 0.0;
    _aimY = 0.0;
    _overPlay = false;
    _overMore = false;
    _overSpill = false;

    animate();
}

void LibraryCard::hover(const Pointer &at) {
    _glowX = at.x;
    _glowY = at.y;

    const BLRect card = face();
    _aimX = std::clamp((at.x - card.x - (card.w / 2.0)) / (card.w / 2.0), -1.0, 1.0);
    _aimY = std::clamp((at.y - card.y - (card.h / 2.0)) / (card.h / 2.0), -1.0, 1.0);
    animate();

    const BLRect play = playBox();
    const BLRect more = moreBox();

    const bool onPlay = playable && at.x >= play.x && at.x < play.x + play.w && at.y >= play.y
        && at.y < play.y + play.h;
    const bool onMore = !actions.empty() && at.x >= more.x && at.x < more.x + more.w
        && at.y >= more.y && at.y < more.y + more.h;
    const bool onSpill = _buried > 0 && _mark.w > 0.0 && at.x >= _mark.x
        && at.x < _mark.x + _mark.w && at.y >= _mark.y && at.y < _mark.y + _mark.h;

    if (onPlay != _overPlay) {
        _overPlay = onPlay;

        _badge.toward(onPlay ? 1.0F : 0.0F, now(), 0.14, Anim::Curve::CubicOut);
        animate();
    }

    if (onMore != _overMore) {
        _overMore = onMore;

        invalidate();
    }

    if (onSpill != _overSpill) {
        _overSpill = onSpill;

        _spill.toward(onSpill ? 1.0F : 0.0F, now(), 0.23, Anim::Curve::BackOut);
        animate();
    }

    // The pointer light moves with it.
    invalidate();
}

bool LibraryCard::advance(const double now) {
    // Where it was before the tweens moved: the shelf damages this too, but the
    // live list is not ordered, so the card may run first and leave a trail.
    const BLRect was = drawn();

    _rise.advance(now);
    _press.advance(now);
    _play.advance(now);
    _badge.advance(now);
    _spill.advance(now);
    // Closes a fixed share of the gap per unit time, whatever the frame rate; a
    // card left behind comes flat faster, since every frame turned is paid for.
    const double follow = hovered() ? FOLLOW : FOLLOW * 0.5;
    const double step = 1.0 - std::exp(-std::clamp(now - _tilted, 0.0, 0.05) / follow);

    _tilted = now;
    _tiltX += (_aimX - _tiltX) * step;
    _tiltY += (_aimY - _tiltY) * step;

    // Close enough is there: a tail under a hundredth would keep the card in the
    // turned path, which is the dear one, long after it has come to rest.
    if (std::abs(_aimX - _tiltX) < 0.01 && std::abs(_aimY - _tiltY) < 0.01) {
        _tiltX = _aimX;
        _tiltY = _aimY;
    }

    const bool tilting = _tiltX != _aimX || _tiltY != _aimY;
    _slideX.advance(now);
    _slideY.advance(now);

    const bool moving = _rise.live() || _press.live() || _play.live() || _badge.live()
        || _spill.live() || tilting || _slideX.live()
        || _slideY.live();

    // The size has held still long enough: the stretched sprites are rebuilt at it.
    if (_holding && now - _resized > 0.12) {
        _holding = false;

        invalidate();
    }

    if (moving) {
        invalidate(was);
        invalidate();
    }

    // The run pill's dot beats on its own clock; repainting the card for it every
    // frame is what a status used to cost.
    const bool beating = StatusIndicator::beats(statusOf(status));

    if (beating && now - _blinked >= 0.62) {
        _blinked = now;
        _dim = !_dim;

        invalidate(_statePill);
    }

    return moving || beating || _holding;
}

void LibraryCard::showMenu() {
    if (root() == nullptr || actions.empty()) {
        return;
    }

    const double tall = Menu::heightOf(actions);
    const BLRect more = moreBox();

    auto made = std::make_unique<Menu>(actions, [this](const int action) {
        root()->dismiss();

        if (triggered) {
            triggered(action);
        }
    });

    _menu = root()->layer(Root::POPUPS)->add(std::move(made));

    _menu->place(BLRect{more.x + more.w - Menu::WIDTH, more.y - tall - 4.0, Menu::WIDTH, tall},
                 root()->type());

    root()->setDismiss([this] {
        if (_menu != nullptr) {
            const BLRect was = _menu->box();

            root()->layer(Root::POPUPS)->erase(_menu);

            _menu = nullptr;

            root()->damage(was);
        }
    }, this);

    _menu->invalidate();
}

}
