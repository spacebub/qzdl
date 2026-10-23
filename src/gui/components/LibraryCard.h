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
#include <optional>
#include <string>
#include <vector>

#include <blend2d/blend2d.h>

#include "gui/draw/Warp.h"
#include "ttk/draw/Anim.h"
#include "ttk/toolkit/Painter.h"
#include "ttk/toolkit/overlays/Menu.h"

#include "gui/state/State.h"

namespace components {

// One card on a library shelf: the art, the play badge, and what is known about
// what it launches.
//
// Every animated property is a tween the card owns, so a card that is not moving
// reports itself still and its pixels are left alone.
class LibraryCard : public ttk::Widget {
public:
    LibraryCard();

    std::string title;
    std::string subtitle;
    std::string caption;

    // The game and its enabled add-ons. What the title screen is read for.
    std::string artKey;

    std::vector<State::BadgeSpec> badges;
    std::vector<ttk::Menu::Row> actions;

    std::string playHint;
    bool playable = true;

    // launching | running | stopping | closed | failed. None is nothing to say.
    State::RunState status = State::RunState::None;
    std::string statusReason;

    void setStatus(State::RunState state, std::string reason);

    // Lets go of the kept image a card at rest is blitted from.
    void dropSheet();

    // What a click anywhere but the buttons does.
    enum class Primary : std::uint8_t {
        Open,
        Play,
    };

    Primary primary = Primary::Open;

    bool draggable = false;

    // Where the card is carried to, off its place in the grid.
    double carryX = 0.0;
    double carryY = 0.0;

    std::function<void()> played;
    std::function<void()> opened;
    std::function<void()> logRequested;
    std::function<void(int)> triggered;

    // Window coordinates.
    std::function<void(double, double)> dragStarted;
    std::function<void(double, double)> dragMoved;
    std::function<void()> dragEnded;

    // The title screen, or an empty image while one is being read.
    std::function<BLImage(const std::string &)> artwork;

    void paint(const ttk::Painter &painter) override;

    [[nodiscard]] BLRect drawn() const override;

    bool press(const ttk::Pointer &at) override;
    void drag(const ttk::Pointer &at) override;
    void release(const ttk::Pointer &at) override;

    void enter() override;
    void leave() override;
    void hover(const ttk::Pointer &at) override;

    [[nodiscard]] ttk::Cursor cursor_at(double x, double y) const override;

    bool advance(double now) override;

    // Where the card is, grown to cover the shadow it casts.
    static BLRect spread(const BLRect &box);

    // True while this one is in hand.
    [[nodiscard]] bool carrying() const { return _dragging; }

    // Starts the walk to the place it has just been given, from where it was.
    void slideFrom(double x, double y, double now);

    // The place in the grid it was last given. A card walks to a new one. It does
    // not walk because the grid scrolled or the window changed size.
    [[nodiscard]] int slot() const { return _slot; }

    void setSlot(const int at) { _slot = at; }

    [[nodiscard]] double slide_x() const { return _slideX.value(); }
    [[nodiscard]] double slide_y() const { return _slideY.value(); }

    [[nodiscard]] bool sliding() const { return _slideX.live() || _slideY.live(); }

    // Flat, unlit, not under the pointer, its pill not beating and nothing on its way.
    [[nodiscard]] bool resting() const;

private:
    // Keeps the art sprites current with the card's size and its title screen.
    void readyArt();

    void paintShadow(const ttk::Painter &painter, const BLRect &card) const;

    // Everything on the face, from its fill to its outline.
    void paintFace(const ttk::Painter &painter, const BLRect &card);

    // A card at rest is one image, shadow and all, blitted where it stands: a
    // scroll moves a shelf of them without painting a run of type or a gradient.
    void paintStill(const ttk::Painter &painter, const BLRect &card);

    // Everything the still image was made from.


    // The face turned to meet the pointer: painted to a sheet and laid back down
    // through the perspective, one image warp for the lot.
    void paintTurned(const ttk::Painter &painter, const BLRect &card);

    // Where a point of the face lands once the card is turned.
    [[nodiscard]] BLPoint turned(BLPoint at, BLPoint middle) const;

    // The same turn as a map, from window pixels to window pixels.
    [[nodiscard]] Warp::Map turning(const BLRect &card) const;

    // How paint() lays the card down: one kept image, the face drawn flat, or the face turned.
    enum class Pose : std::uint8_t {
        Still,
        Flat,
        Turned,
    };

    [[nodiscard]] Pose pose() const;

    // The window pixels a change to `part` of the face can reach, through the turn while there is one.
    [[nodiscard]] BLRect onScreen(const BLRect &part) const;

    void paintArt(const ttk::Painter &painter, const BLRect &box);
    void paintPlay(const ttk::Painter &painter, const BLRect &box);
    void paintBody(const ttk::Painter &painter, const BLRect &card);
    void paintSheen(const ttk::Painter &painter, const BLRect &card);

    // The play badge as drawn: how far it has faded in, how far it has grown out of
    // its middle, and how far the halo around it reaches from there.
    struct Pop {
        double shown = 0.0;
        double grown = 0.0;
        double halo = 0.0;
    };

    [[nodiscard]] Pop popped(const BLRect &box) const;

    // The square the play badge and its halo cover. Empty while it is hidden.
    [[nodiscard]] BLRect popBox() const;

    // Where the run pill's top left sits.
    [[nodiscard]] BLPoint pillAt() const;

    void paintState(const ttk::Painter &painter);
    void paintMeta(const ttk::Painter &painter, const BLRect &box) const;
    void paintBadges(const ttk::Painter &painter, const BLRect &row);

    // The art as two finished sprites, built once per size and cross-faded: at rest
    // and lit. Gradient, sheen, artwork, scrim, ember and dim are all fixed in them,
    // so a hover is two blits rather than a composite, a gradient and a mask per frame.
    void ground(int wide, int tall);

    void showMenu();

    // A badge in the row, worked out at paint time and reused by the hit test.
    struct Slot {
        double x = 0.0;
        double span = 0.0;
        bool shown = false;
    };

    void slots(const ttk::Painter &painter, const BLRect &row, std::vector<Slot> &out, int &buried) const;

    // Where the first of the buried badges lands once the fan is all the way out.
    [[nodiscard]] double fanLeft() const;

    // A buried badge `span` wide on its way to `x`, with the fan `open` of the way out of the mark.
    [[nodiscard]] BLRect fanned(double x, double span, double open) const;

    // Everything the fan covers at `open`, from the row and mark last painted.
    [[nodiscard]] BLRect fanBox(double open) const;

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

    // The sheet after the turn, kept so a frame allocates nothing.
    BLImage _warped;

    // The turned card's body, without the glow that follows the pointer.
    BLImage _base;

    // Refilled in place each paint, so drawing the face allocates nothing.
    BLPath _shape;
    BLPattern _restFill;
    BLPattern _litFill;
    BLGradient _sweep;
    double _sweepShown = -1.0;

    std::optional<ttk::Painter> _sheetPainter;

    // What the still sheet was drawn from. Compared field by field rather than
    // rolled into a key: a card is painted every frame the shelf is, and building
    // a string for it was a tenth of what a resting card costs.
    struct Still {
        std::string title;
        std::string subtitle;
        std::string caption;
        std::string statusReason;
        Primary primary{};
        std::vector<State::BadgeSpec> badges;
        State::RunState status = State::RunState::None;

        // The build of the art sprites it was drawn over: a rebuild at the size the
        // card settled on, or for new art, leaves everything else here standing.
        int ground = 0;

        int wide = 0;
        int tall = 0;
        bool playable = false;
        bool dim = false;
        bool drawn = false;
        bool menu = false;
        bool dark = false;
    };

    [[nodiscard]] Still stillOf(const BLRectI &sheet) const;

    // stillOf(sheet) == mark, without copying the strings to find out.
    [[nodiscard]] bool stillMatches(const Still &mark, const BLRectI &sheet) const;

    // What the kept body was drawn from. Only used while nothing is in flight.
    struct Face {
        Still still;
        double x = 0.0;
        double y = 0.0;
    };

    // The one painter every offscreen sheet is drawn with, aimed at `into`.
    const ttk::Painter &sheetPainter(const ttk::Painter &painter, BLContext &into, const BLRectI &sheet);

    // Paints the sheet as a kept body with the sheen over it.
    void keepBody(const ttk::Painter &painter, const BLRect &card, const BLRectI &sheet);

    Face _faceMark;

    BLImage _still;
    Still _stillMark;
    BLPoint _stillAt{};

    // What the two art sprites were built from, and how many times they have been.
    int _grounds = 0;
    std::string _groundArt;

    // The title screen itself, not its size: every Doom title is 320x200, and the
    // game's own stands in while an add-on's is read.
    BLImage _groundShot;
    int _groundWide = 0;
    int _groundTall = 0;
    bool _groundPlayable = false;
    bool _groundCaptioned = false;
    bool _drawn = false;

    // A resize hands the card a new size every frame, and rebuilding the sprites
    // for each of them is most of what a resize costs. The old ones are stretched
    // until the size has held still for a moment.
    int _wantedWide = 0;
    int _wantedTall = 0;
    double _resized = 0.0;
    bool _holding = false;

    ttk::Anim::Tween _rise;  // 0 at rest, 1 hovered
    ttk::Anim::Tween _press; // 0 up, 1 down
    ttk::Anim::Tween _play;  // 0 hidden, 1 the play badge fully up
    ttk::Anim::Tween _badge; // 0 away, 1 over the play badge
    ttk::Anim::Tween _spill; // the buried badges fanning out

    // Where the pointer is across the face, -1 to 1 each way, and where the turn
    // has got to on its way there. Not a tween: a pointer reports a hundred times
    // a frame, and a run restarted on each report never gets past its first step.
    double _aimX = 0.0;
    double _aimY = 0.0;
    double _tiltX = 0.0;
    double _tiltY = 0.0;
    double _tilted = 0.0;

    // Where it is coming from, while the cards shuffle around a carried one.
    ttk::Anim::Tween _slideX;
    ttk::Anim::Tween _slideY;

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
