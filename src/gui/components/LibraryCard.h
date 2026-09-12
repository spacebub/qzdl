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
#include <vector>

#include <blend2d/blend2d.h>

#include "gui/draw/Anim.h"
#include "gui/model/State.h"
#include "gui/toolkit/overlays/Menu.h"

namespace components {

// One card on a library shelf: the art, the play badge, and what is known about
// what it launches.
//
// Every animated property is a tween the card owns, so a card that is not moving
// reports itself still and its pixels are left alone.
class LibraryCard : public toolkit::Widget {
public:
    LibraryCard();

    std::string title;
    std::string subtitle;
    std::string caption;

    // The game and its enabled add-ons; what the title screen is read for.
    std::string artKey;

    std::vector<State::BadgeSpec> badges;
    std::vector<toolkit::Menu::Row> actions;

    std::string playHint;
    bool playable = true;

    // launching | running | stopping | closed | failed. Empty is nothing to say.
    State::RunState status = State::RunState::None;
    std::string statusReason;

    // play | open: what a click anywhere but the buttons does.
    std::string primary = "open";

    bool draggable = false;

    // Where the card is carried to, off its place in the grid.
    double carryX = 0.0;
    double carryY = 0.0;

    std::function<void()> played;
    std::function<void()> opened;
    std::function<void()> logRequested;
    std::function<void(const std::string &)> triggered;

    // Window coordinates.
    std::function<void()> pressedDown;
    std::function<void(double, double)> dragStarted;
    std::function<void(double, double)> dragMoved;
    std::function<void()> dragEnded;

    // The title screen, or an empty image while one is being read.
    std::function<BLImage(const std::string &)> artwork;

    void paint(const toolkit::Painter &painter) override;

    [[nodiscard]] BLRect drawn() const override;

    bool press(const toolkit::Pointer &at) override;
    void drag(const toolkit::Pointer &at) override;
    void release(const toolkit::Pointer &at) override;

    void enter() override;
    void leave() override;
    void hover(const toolkit::Pointer &at) override;

    [[nodiscard]] toolkit::Cursor cursorAt(double x, double y) const override;

    bool advance(double now) override;

    // Where the card is, grown to cover the shadow it casts.
    static BLRect spread(const BLRect &box);

    // True while this one is in hand.
    [[nodiscard]] bool carrying() const { return _dragging; }

    // Starts the walk to the place it has just been given, from where it was.
    void slideFrom(double x, double y, double now);

    // The place in the grid it was last given. A card walks to a new one; it does
    // not walk because the grid scrolled or the window changed size.
    [[nodiscard]] int slot() const { return _slot; }

    void setSlot(const int at) { _slot = at; }

    [[nodiscard]] double slideX() const { return _slideX.value(); }
    [[nodiscard]] double slideY() const { return _slideY.value(); }

private:
    // Keeps the art sprites current with the card's size and its title screen.
    void readyArt();

    void paintShadow(const toolkit::Painter &painter, const BLRect &card) const;

    // Everything on the face, from its fill to its outline.
    void paintFace(const toolkit::Painter &painter, const BLRect &card);

    // A card at rest is one image, shadow and all, blitted where it stands: a
    // scroll moves a shelf of them without painting a run of type or a gradient.
    void paintStill(const toolkit::Painter &painter, const BLRect &card);

    // Everything the still image was made from.
    [[nodiscard]] std::string stillKey(const BLRectI &sheet) const;

    // The face turned to meet the pointer: painted to a sheet and laid back down
    // in cells, each under the affine that fits the perspective there.
    void paintTurned(const toolkit::Painter &painter, const BLRect &card);

    // Where a point of the face lands once the card is turned.
    [[nodiscard]] BLPoint turned(BLPoint at, BLPoint middle) const;

    void paintArt(const toolkit::Painter &painter, const BLRect &box);
    void paintPlay(const toolkit::Painter &painter, const BLRect &box);
    void paintState(const toolkit::Painter &painter, const BLRect &box);
    void paintMeta(const toolkit::Painter &painter, const BLRect &box) const;
    void paintBadges(const toolkit::Painter &painter, const BLRect &row);

    // The art as two finished sprites, built once per size and cross-faded: at rest
    // and lit. Everything in them is fixed -- the gradient, the sheen, the artwork,
    // the scrim, the ember and the dim -- so a hover is two blits rather than a
    // composite, a gradient and a mask per frame.
    void ground(int wide, int tall);

    void showMenu();

    // A badge in the row, worked out at paint time and reused by the hit test.
    struct Slot {
        double x = 0.0;
        double span = 0.0;
        bool shown = false;
    };

    std::vector<Slot> slots(const toolkit::Painter &painter, const BLRect &row, int &buried) const;

    // Where the card's face is, with the carry and the rise applied.
    [[nodiscard]] BLRect face() const;

    [[nodiscard]] BLRect artBox() const;
    [[nodiscard]] BLRect playBox() const;
    [[nodiscard]] BLRect moreBox() const;
    [[nodiscard]] BLRect metaBox() const;
    [[nodiscard]] BLRect badgeRow() const;

    BLImage _rest;
    BLImage _lit;
    BLImage _sheet;

    BLImage _still;
    std::string _stillKey;
    BLPoint _stillAt{};

    // What they were built from.
    std::string _groundKey;
    int _groundWide = 0;
    int _groundTall = 0;
    bool _drawn = false;

    // A resize hands the card a new size every frame, and rebuilding the sprites
    // for each of them is most of what a resize costs. The old ones are stretched
    // until the size has held still for a moment.
    int _wantedWide = 0;
    int _wantedTall = 0;
    double _resized = 0.0;
    bool _holding = false;

    Anim::Tween _rise;    // 0 at rest, 1 hovered
    Anim::Tween _press;   // 0 up, 1 down
    Anim::Tween _play;    // 0 hidden, 1 the play badge fully up
    Anim::Tween _badge;   // 0 away, 1 over the play badge
    Anim::Tween _spill;   // the buried badges fanning out

    // Where the pointer is across the face, -1 to 1 each way, and where the turn
    // has got to on its way there. Not a tween: a pointer reports a hundred times
    // a frame, and a run restarted on each report never gets past its first step.
    double _aimX = 0.0;
    double _aimY = 0.0;
    double _tiltX = 0.0;
    double _tiltY = 0.0;
    double _tilted = 0.0;

    // Where it is coming from, while the cards shuffle around a carried one.
    Anim::Tween _slideX;
    Anim::Tween _slideY;

    // The run pill's dot, while a launch is still settling either way.
    double _blinked = 0.0;
    bool _dim = false;

    // Where the pointer is, for the light that follows it.
    double _glowX = 0.0;
    double _glowY = 0.0;

    int _slot = -1;

    bool _dragging = false;
    bool _armed = false;
    bool _overPlay = false;
    bool _overMore = false;
    bool _overSpill = false;

    double _pressX = 0.0;
    double _pressY = 0.0;

    // Where the run pill was last drawn, so a press on it opens the log.
    BLRect _statePill{};

    // The last badge row measured, so the pointer can be tested against it.
    std::vector<Slot> _slots;
    int _buried = 0;
    BLRect _mark{};

    Widget *_menu = nullptr;
};

}
