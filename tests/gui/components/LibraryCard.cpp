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

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <blend2d/blend2d.h>
#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/system/Process.h"
#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/StatusIndicator.h"
#include "ttk/toolkit/overlays/Menu.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/LibraryCard.h"
#include "gui/draw/Cards.h"
#include "gui/model/ConfigBridge.h"
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Rig.h"

namespace {

using Card = components::LibraryCard;

constexpr BLRect BOX{60.0, 60.0, Cards::width, Cards::rowHeight};

constexpr BLPoint MIDDLE{BOX.x + (BOX.w / 2.0), BOX.y + (BOX.h / 2.0)};

// The play badge sits in the middle of the art.
constexpr BLPoint PLAY{BOX.x + (BOX.w / 2.0), BOX.y + (Cards::art / 2.0)};

constexpr BLPoint MORE{BOX.x + BOX.w - 36.0 + 13.0, BOX.y + BOX.h - 36.0 + 13.0};

constexpr BLPoint PILL{BOX.x + 10.0 + 12.0, BOX.y + 10.0 + 12.0};

constexpr BLRectI ART{static_cast<int>(BOX.x) + 4, static_cast<int>(BOX.y) + 4, 236, 130};

constexpr BLRectI ROW{static_cast<int>(BOX.x) + 14, static_cast<int>(BOX.y + BOX.h) - 34, 186, 22};

enum class Action : std::uint8_t {
    First,
    Second,
};

class LibraryCardTest : public tests::Painted {
protected:
    void SetUp() override {
        tests::Painted::SetUp();

        if (IsSkipped()) {
            return;
        }

        ttk::Theme::set_mode(ttk::Theme::Mode::Light);
    }

    tests::Canvas canvas{400, 380};

    Card *card = nullptr;

    int opened = 0;
    int played = 0;
    int logged = 0;
    int started = 0;
    int moved = 0;
    int ended = 0;
    std::vector<int> triggered;

    BLPoint startedAt{};
    BLPoint movedTo{};

    BLImage shot;

    std::unique_ptr<Card> prepared() {
        auto made = std::make_unique<Card>();

        made->title = "Eviternity";
        made->subtitle = "GZDoom";
        made->caption = "DOOM II";
        made->artKey = "doom2.wad";
        made->playHint = "Launch Eviternity";
        made->badges = {{.text = "3 files", .kind = State::BadgeKind::Muted, .dot = true}};

        made->played = [this] { ++played; };
        made->opened = [this] { ++opened; };
        made->logRequested = [this] { ++logged; };
        made->triggered = [this](const int action) { triggered.push_back(action); };
        made->dragStarted = [this](const double x, const double y) {
            ++started;
            startedAt = BLPoint{x, y};
        };
        made->dragMoved = [this](const double x, const double y) {
            ++moved;
            movedTo = BLPoint{x, y};
        };
        made->dragEnded = [this] { ++ended; };
        made->artwork = [this](const std::string &) { return shot; };

        return made;
    }

    Card *make(std::unique_ptr<Card> made, const BLRect &box = BOX) {
        card = parts::place(canvas, std::move(made), box);
        canvas.full();

        return card;
    }

    Card *make() { return make(prepared()); }

    ttk::Root &ui() { return canvas.ui(); }

    void hover(const BLPoint at, const int frames = 30) {
        ui().motion(at.x, at.y);
        canvas.run(frames);
    }

    void away(const int frames = 60) {
        ui().motion(390.0, 370.0);
        canvas.run(frames);
    }

    void click(const BLPoint at) { parts::click(ui(), at.x, at.y); }

    // The first row down the middle of the card that is the dark art, under the shadow and the outline.
    int artTop() {
        const auto x = static_cast<int>(MIDDLE.x);

        for (int y = 0; y < canvas.height(); ++y) {
            if (parts::distance(canvas.at(x, y), ttk::Theme::palette().background.value) > 400) {
                return y;
            }
        }

        return -1;
    }

    int ember(const BLRectI &area) { return parts::count_near(canvas, area, Cards::ember, 120); }

    static BLRectI around(const BLPoint at, const int half) {
        return BLRectI{static_cast<int>(at.x) - half, static_cast<int>(at.y) - half, half * 2, half * 2};
    }

    void expectFull() {
        const parts::Ink off = parts::mismatch(canvas);

        EXPECT_EQ(off.pixels, 0U) << off;
    }

    static BLImage solid(const int wide, const int tall, const BLRgba32 colour) {
        BLImage made(wide, tall, BL_FORMAT_PRGB32);
        BLContext into(made);

        into.fill_all(colour);
        into.end();

        return made;
    }

    static std::vector<ttk::Menu::Row> actions() {
        return {ttk::Menu::item(Action::First, "First", ttk::Glyphs::Glyph::Play),
                ttk::Menu::item(Action::Second, "Second", ttk::Glyphs::Glyph::Edit)};
    }
};

TEST(LibraryCard, StartsStillAndUnplaced) {
    const Card card;

    EXPECT_EQ(card.slot(), -1);
    EXPECT_FALSE(card.carrying());
    EXPECT_FALSE(card.sliding());
    EXPECT_DOUBLE_EQ(card.slide_x(), 0.0);
    EXPECT_EQ(card.status, State::RunState::None);
    EXPECT_TRUE(card.playable);
    EXPECT_EQ(card.primary, Card::Primary::Open);
    EXPECT_TRUE(card.hint.empty());
}

TEST(LibraryCard, ShowsThePointingHand) {
    const Card card;

    EXPECT_EQ(card.cursor, ttk::Cursor::Pointer);
    EXPECT_EQ(card.cursor_at(10.0, 10.0), ttk::Cursor::Pointer);
}

TEST(LibraryCard, RemembersItsSlot) {
    Card card;

    card.setSlot(4);

    EXPECT_EQ(card.slot(), 4);
}

TEST(LibraryCard, SpreadReachesFurtherBelowForTheDrop) {
    const BLRect spread = Card::spread(BOX);
    const double left = BOX.x - spread.x;
    const double below = (spread.y + spread.h) - (BOX.y + BOX.h);

    EXPECT_GT(left, 0.0);
    EXPECT_DOUBLE_EQ(BOX.y - spread.y, left);
    EXPECT_GT(below, left);
}

TEST(LibraryCard, SlideFromStartsWhereItWasAndWalksHome) {
    Card card;

    card.slideFrom(40.0, -20.0, 1.0);

    EXPECT_TRUE(card.sliding());
    EXPECT_DOUBLE_EQ(card.slide_x(), 40.0);
    EXPECT_DOUBLE_EQ(card.slide_y(), -20.0);

    card.advance(1.0 + Cards::settling + 0.01);

    EXPECT_FALSE(card.sliding());
    EXPECT_DOUBLE_EQ(card.slide_x(), 0.0);
    EXPECT_DOUBLE_EQ(card.slide_y(), 0.0);
}

TEST(LibraryCard, ANoneStatusSaysNothing) {
    Card card;

    card.setStatus(State::RunState::Failed, "Exited with code 1");
    card.setStatus(State::RunState::None, "");

    EXPECT_TRUE(card.hint.empty());
}

TEST(LibraryCard, AStatusSaysWhatItIsAndThatTheLogIsAClickAway) {
    Card card;

    card.setStatus(State::RunState::Failed, "Exited with code 1");

    EXPECT_EQ(card.hint,
              ttk::StatusIndicator::say_of(ttk::StatusIndicator::Status::Failed, "Exited with code 1")
                      + " Click to see what it printed.");
    EXPECT_EQ(card.statusReason, "Exited with code 1");
}

TEST_F(LibraryCardTest, DrawnCoversTheSpreadAndFollowsTheCarry) {
    make();

    const BLRect spread = Card::spread(BOX);
    const BLRect drawn = card->drawn();

    EXPECT_LE(drawn.x, spread.x);
    EXPECT_LE(drawn.y, spread.y);
    EXPECT_GE(drawn.x + drawn.w, spread.x + spread.w);
    EXPECT_GE(drawn.y + drawn.h, spread.y + spread.h);

    card->carryX = 30.0;

    EXPECT_DOUBLE_EQ(card->drawn().x, drawn.x + 30.0);
}

TEST_F(LibraryCardTest, PaintsNothingOutsideWhatItDraws) {
    make();

    const BLRect drawn = card->drawn();
    const std::uint32_t ground = ttk::Theme::palette().background.value;

    EXPECT_EQ(parts::ink(canvas, BLRectI{0, 0, 400, static_cast<int>(drawn.y) - 1}, ground).pixels, 0U);
    EXPECT_EQ(parts::ink(canvas, BLRectI{0, 0, static_cast<int>(drawn.x) - 1, 380}, ground).pixels, 0U);
}

TEST_F(LibraryCardTest, AClickOpensAnOpenCard) {
    make();
    click(MIDDLE);

    EXPECT_EQ(opened, 1);
    EXPECT_EQ(played, 0);
}

TEST_F(LibraryCardTest, AClickPlaysAPlayCard) {
    auto made = prepared();

    made->primary = Card::Primary::Play;
    make(std::move(made));
    click(MIDDLE);

    EXPECT_EQ(played, 1);
    EXPECT_EQ(opened, 0);
}

TEST_F(LibraryCardTest, ThePlayBadgeOfAHoveredCardPlays) {
    make();
    hover(MIDDLE);
    click(PLAY);

    EXPECT_EQ(played, 1);
    EXPECT_EQ(opened, 0);
}

TEST_F(LibraryCardTest, AnUnplayableCardOpensFromWhereTheBadgeWouldBe) {
    auto made = prepared();

    made->playable = false;
    make(std::move(made));
    hover(MIDDLE);
    click(PLAY);

    EXPECT_EQ(played, 0);
    EXPECT_EQ(opened, 1);
}

TEST_F(LibraryCardTest, ReturnOnAFocusedCardDoesWhatAClickDoes) {
    make();
    ui().focus(card);
    ui().key(parts::key(ttk::Code::Return));

    EXPECT_EQ(opened, 1);
}

TEST_F(LibraryCardTest, ReleasingOffTheCardDoesNothing) {
    make();
    parts::sweep(ui(), MIDDLE.x, MIDDLE.y, 390.0, 370.0);

    EXPECT_EQ(opened, 0);
    EXPECT_EQ(started, 0);
}

TEST_F(LibraryCardTest, TheCornerDropsTheActions) {
    auto made = prepared();

    made->actions = actions();
    make(std::move(made));
    click(MORE);

    ASSERT_EQ(ui().layer(ttk::Root::POPUPS)->children().size(), 1U);
    EXPECT_EQ(opened, 0);

    const BLRect &menu = ui().layer(ttk::Root::POPUPS)->children().front()->box();

    EXPECT_LT(menu.y + menu.h, MORE.y);
}

TEST_F(LibraryCardTest, PickingAnActionTriggersIt) {
    auto made = prepared();

    made->actions = actions();
    make(std::move(made));
    click(MORE);

    ASSERT_EQ(ui().layer(ttk::Root::POPUPS)->children().size(), 1U);

    const BLRect &menu = ui().layer(ttk::Root::POPUPS)->children().front()->box();

    parts::click(ui(), menu.x + 60.0, menu.y + 5.0 + 32.0 + 16.0);

    EXPECT_EQ(triggered, (std::vector<int>{static_cast<int>(Action::Second)}));
    EXPECT_TRUE(ui().layer(ttk::Root::POPUPS)->children().empty());
}

TEST_F(LibraryCardTest, WithNoActionsTheCornerIsPartOfTheCard) {
    make();
    click(MORE);

    EXPECT_TRUE(ui().layer(ttk::Root::POPUPS)->children().empty());
    EXPECT_EQ(opened, 1);
}

TEST_F(LibraryCardTest, ThePillAsksForTheLog) {
    make();
    card->setStatus(State::RunState::Failed, "Exited with code 1");
    canvas.frame();
    click(PILL);

    EXPECT_EQ(logged, 1);
    EXPECT_EQ(opened, 0);
}

TEST_F(LibraryCardTest, WithNoStatusWhereThePillWouldBeOpens) {
    make();
    click(PILL);

    EXPECT_EQ(logged, 0);
    EXPECT_EQ(opened, 1);
}

TEST_F(LibraryCardTest, CarryingADraggableCardReportsTheDrag) {
    auto made = prepared();

    made->draggable = true;
    make(std::move(made));
    parts::sweep(ui(), MIDDLE.x, MIDDLE.y, MIDDLE.x + 60.0, MIDDLE.y + 10.0);

    EXPECT_EQ(started, 1);
    EXPECT_DOUBLE_EQ(startedAt.x, MIDDLE.x);
    EXPECT_DOUBLE_EQ(startedAt.y, MIDDLE.y);
    EXPECT_EQ(moved, 1);
    EXPECT_DOUBLE_EQ(movedTo.x, MIDDLE.x + 60.0);
    EXPECT_EQ(ended, 1);
    EXPECT_EQ(opened, 0);
}

TEST_F(LibraryCardTest, AMoveInsideTheSlackIsStillAClick) {
    auto made = prepared();

    made->draggable = true;
    make(std::move(made));
    parts::sweep(ui(), MIDDLE.x, MIDDLE.y, MIDDLE.x + Cards::dragSlack - 1.0, MIDDLE.y);

    EXPECT_EQ(started, 0);
    EXPECT_EQ(opened, 1);
}

TEST_F(LibraryCardTest, ACardThatIsNotDraggableIsNotCarried) {
    make();
    ui().motion(MIDDLE.x, MIDDLE.y);
    ui().press(ttk::Pointer{.x = MIDDLE.x, .y = MIDDLE.y});
    ui().motion(MIDDLE.x + 60.0, MIDDLE.y);

    EXPECT_FALSE(card->carrying());

    ui().release(ttk::Pointer{.x = MIDDLE.x + 60.0, .y = MIDDLE.y});

    EXPECT_EQ(started, 0);
}

TEST_F(LibraryCardTest, ACarriedCardShowsTheGrabbingHand) {
    auto made = prepared();

    made->draggable = true;
    make(std::move(made));
    ui().motion(MIDDLE.x, MIDDLE.y);
    ui().press(ttk::Pointer{.x = MIDDLE.x, .y = MIDDLE.y});
    ui().motion(MIDDLE.x + 60.0, MIDDLE.y);

    EXPECT_TRUE(card->carrying());
    EXPECT_EQ(card->cursor_at(MIDDLE.x, MIDDLE.y), ttk::Cursor::Grabbing);

    ui().release(ttk::Pointer{.x = MIDDLE.x + 60.0, .y = MIDDLE.y});

    EXPECT_FALSE(card->carrying());
}

TEST_F(LibraryCardTest, HoverLiftsTheCard) {
    make();

    const int resting = artTop();

    hover(MIDDLE);

    EXPECT_EQ(artTop(), resting - 3);
}

TEST_F(LibraryCardTest, APressSinksAHoveredCardALittle) {
    make();

    const int resting = artTop();

    hover(MIDDLE);
    ui().press(ttk::Pointer{.x = MIDDLE.x, .y = MIDDLE.y});
    canvas.run(30);

    EXPECT_EQ(artTop(), resting - 2);

    ui().release(ttk::Pointer{.x = MIDDLE.x, .y = MIDDLE.y});
}

TEST_F(LibraryCardTest, HoverRaisesThePlayBadge) {
    shot = solid(320, 200, BLRgba32(0xff808080));
    make();

    EXPECT_EQ(ember(around(PLAY, 16)), 0);

    hover(MIDDLE);

    EXPECT_GT(ember(around(PLAY, 16)), 0);
}

TEST_F(LibraryCardTest, AnUnplayableCardRaisesNoBadge) {
    shot = solid(320, 200, BLRgba32(0xff808080));

    auto made = prepared();

    made->playable = false;
    make(std::move(made));
    hover(MIDDLE);

    EXPECT_EQ(ember(around(PLAY, 16)), 0);
}

TEST_F(LibraryCardTest, TheActionsCornerShowsOnlyOnHover) {
    auto made = prepared();

    made->actions = actions();
    make(std::move(made));

    const std::vector<std::uint32_t> resting = parts::pixels_in(canvas, around(MORE, 8));

    hover(MIDDLE);

    const std::uint32_t lifted = canvas.at(static_cast<int>(MORE.x), static_cast<int>(MORE.y) - 3);

    EXPECT_GT(parts::changed(parts::pixels_in(canvas, around(BLPoint{MORE.x, MORE.y - 3.0}, 8)), resting), 0);
    EXPECT_NE(lifted, 0U);
}

TEST_F(LibraryCardTest, ACardHoveredAndLeftLooksAsItDidBefore) {
    make();

    const std::vector<std::uint32_t> resting = canvas.pixels();

    hover(MIDDLE);
    away();
    canvas.full();

    EXPECT_EQ(canvas.pixels(), resting);
}

TEST_F(LibraryCardTest, RisingRepaintsLikeAFullFrame) {
    make();
    hover(MIDDLE, 3);
    expectFull();
}

TEST_F(LibraryCardTest, SettlingBackRepaintsLikeAFullFrame) {
    make();
    hover(MIDDLE);
    ui().motion(390.0, 370.0);
    canvas.run(4);
    expectFull();
}

TEST_F(LibraryCardTest, ComingToRestRepaintsLikeAFullFrame) {
    make();
    hover(MIDDLE);
    away();
    expectFull();
}

TEST_F(LibraryCardTest, TurningToThePointerRepaintsLikeAFullFrame) {
    make();
    hover(BLPoint{BOX.x + 20.0, BOX.y + 20.0}, 4);
    expectFull();
}

TEST_F(LibraryCardTest, ATurnedCardRepaintsLikeAFullFrame) {
    make();
    hover(BLPoint{BOX.x + 20.0, BOX.y + 20.0}, 40);
    expectFull();
}

TEST_F(LibraryCardTest, MovingOntoThePlayBadgeRepaintsLikeAFullFrame) {
    make();
    hover(MIDDLE);
    hover(PLAY, 4);
    expectFull();
}

TEST_F(LibraryCardTest, PressingRepaintsLikeAFullFrame) {
    make();
    hover(MIDDLE);
    ui().press(ttk::Pointer{.x = MIDDLE.x, .y = MIDDLE.y});
    canvas.run(3);
    expectFull();
    ui().release(ttk::Pointer{.x = MIDDLE.x, .y = MIDDLE.y});
}

TEST_F(LibraryCardTest, SlidingRepaintsLikeAFullFrame) {
    make();
    card->slideFrom(60.0, 20.0, canvas.now());
    canvas.run(4);
    expectFull();
}

TEST_F(LibraryCardTest, ASlideEndsWhereItStarted) {
    make();

    const std::vector<std::uint32_t> resting = canvas.pixels();

    card->slideFrom(60.0, 20.0, canvas.now());
    canvas.run(30);

    EXPECT_EQ(canvas.pixels(), resting);
}

TEST_F(LibraryCardTest, AStatusPillIsDrawnOverTheArt) {
    make();

    const std::vector<std::uint32_t> bare = parts::pixels_in(canvas, around(PILL, 10));

    card->setStatus(State::RunState::Failed, "Exited with code 1");
    canvas.frame();

    EXPECT_GT(parts::changed(parts::pixels_in(canvas, around(PILL, 10)), bare), 0);
}

TEST_F(LibraryCardTest, ANewStatusRepaintsLikeAFullFrame) {
    make();
    card->setStatus(State::RunState::Launching, "");
    canvas.frame();
    card->setStatus(State::RunState::Failed, "Exited with code 1");
    canvas.frame();
    expectFull();
}

TEST_F(LibraryCardTest, AStatusClearedRepaintsLikeAFullFrame) {
    make();
    card->setStatus(State::RunState::Running, "");
    canvas.frame();
    card->setStatus(State::RunState::None, "");
    canvas.frame();
    expectFull();
}

TEST_F(LibraryCardTest, ALaunchingPillBeats) {
    make();
    card->setStatus(State::RunState::Launching, "");
    canvas.frame();

    const std::vector<std::uint32_t> first = parts::pixels_in(canvas, around(PILL, 12));

    canvas.run(static_cast<int>(ttk::StatusIndicator::BEAT * 60.0) + 4);

    EXPECT_GT(parts::changed(parts::pixels_in(canvas, around(PILL, 12)), first), 0);
    expectFull();
}

TEST_F(LibraryCardTest, ARunningPillHoldsStill) {
    make();
    card->setStatus(State::RunState::Running, "");
    canvas.frame();

    const std::vector<std::uint32_t> first = parts::pixels_in(canvas, around(PILL, 12));

    canvas.run(60);

    EXPECT_EQ(parts::pixels_in(canvas, around(PILL, 12)), first);
}

TEST_F(LibraryCardTest, BadgesAreDrawnAlongTheBottom) {
    auto made = prepared();

    made->badges.clear();
    make(std::move(made));

    const std::uint32_t ground = ttk::Theme::palette().surface.value;
    const size_t bare = parts::ink(canvas, ROW, ground).pixels;

    make();

    const parts::Ink drawn = parts::ink(canvas, ROW, ground);

    EXPECT_GT(drawn.pixels, bare);
    EXPECT_EQ(drawn.left, ROW.x);
}

TEST_F(LibraryCardTest, ADangerBadgeIsDrawnInDanger) {
    auto made = prepared();

    made->badges = {{.text = "Missing", .kind = State::BadgeKind::Danger, .dot = true}};
    make(std::move(made));

    EXPECT_GT(parts::count_near(canvas, ROW, ttk::Theme::palette().dangerSoft, 12), 0);
}

TEST_F(LibraryCardTest, ABadgeTooLongForTheRowIsBuriedAndFansOutOverIt) {
    auto made = prepared();

    made->badges = {{.text = std::string(60, 'W'), .kind = State::BadgeKind::Muted, .dot = false}};
    make(std::move(made));
    hover(MIDDLE);

    const BLRectI above{ROW.x - 10, ROW.y - 40, ROW.w + 20, 36};
    const std::vector<std::uint32_t> shut = parts::pixels_in(canvas, above);

    hover(BLPoint{static_cast<double>(ROW.x) + 12.0, static_cast<double>(ROW.y) + 8.0});

    EXPECT_GT(parts::changed(parts::pixels_in(canvas, above), shut), 0);
}

TEST_F(LibraryCardTest, TheFanRepaintsLikeAFullFrame) {
    auto made = prepared();

    made->badges = {{.text = std::string(60, 'W'), .kind = State::BadgeKind::Muted, .dot = false}};
    make(std::move(made));
    hover(MIDDLE);
    hover(BLPoint{static_cast<double>(ROW.x) + 12.0, static_cast<double>(ROW.y) + 8.0}, 4);
    expectFull();
}

TEST_F(LibraryCardTest, TheFanFoldsAwayWhenThePointerLeavesTheMark) {
    auto made = prepared();

    made->badges = {{.text = std::string(60, 'W'), .kind = State::BadgeKind::Muted, .dot = false}};
    make(std::move(made));
    hover(MIDDLE);

    // Moved once the card has risen, so both times the pointer is aimed from the same lifted face.
    const BLPoint rest{MIDDLE.x + 1.0, MIDDLE.y};

    hover(rest);

    const BLRectI above{ROW.x - 10, ROW.y - 40, ROW.w + 20, 36};
    const std::vector<std::uint32_t> shut = parts::pixels_in(canvas, above);

    hover(BLPoint{static_cast<double>(ROW.x) + 12.0, static_cast<double>(ROW.y) + 8.0});
    hover(rest);

    EXPECT_EQ(parts::pixels_in(canvas, above), shut);
    expectFull();
}

TEST_F(LibraryCardTest, SixBadgesBuryTheSixth) {
    auto made = prepared();

    made->badges.clear();

    for (int at = 0; at < 6; ++at) {
        made->badges.push_back({.text = "B", .kind = State::BadgeKind::Muted, .dot = false});
    }

    make(std::move(made));
    hover(MIDDLE);

    const BLRectI above{ROW.x - 10, ROW.y - 40, ROW.w + 20, 36};
    const std::vector<std::uint32_t> shut = parts::pixels_in(canvas, above);

    // The mark follows the five, each a letter and its pad wide with 6 between.
    for (int x = ROW.x + 150; x < ROW.x + ROW.w; x += 4) {
        ui().motion(x, ROW.y + 11.0);
    }

    canvas.run(30);

    EXPECT_GT(parts::changed(parts::pixels_in(canvas, above), shut), 0);
}

TEST_F(LibraryCardTest, TheTitleIsWrittenUnderTheArt) {
    auto made = prepared();

    made->title.clear();
    made->subtitle.clear();
    make(std::move(made));

    const BLRectI meta{static_cast<int>(BOX.x) + 14, static_cast<int>(BOX.y + Cards::art) + 12, 200, 20};
    const size_t bare = parts::ink(canvas, meta, ttk::Theme::palette().surface.value).pixels;

    auto titled = prepared();

    titled->subtitle.clear();
    make(std::move(titled));

    EXPECT_GT(parts::ink(canvas, meta, ttk::Theme::palette().surface.value).pixels, bare);
}

TEST_F(LibraryCardTest, ASubtitleIsWrittenUnderTheTitle) {
    auto made = prepared();

    made->subtitle.clear();
    make(std::move(made));

    const BLRectI under{static_cast<int>(BOX.x) + 14, static_cast<int>(BOX.y + Cards::art) + 36, 200, 18};
    const size_t bare = parts::ink(canvas, under, ttk::Theme::palette().surface.value).pixels;

    make();

    EXPECT_GT(parts::ink(canvas, under, ttk::Theme::palette().surface.value).pixels, bare);
}

TEST_F(LibraryCardTest, TheCaptionIsWrittenAcrossTheFootOfTheArt) {
    auto made = prepared();

    made->caption.clear();
    make(std::move(made));

    const BLRectI foot{ART.x, ART.y + ART.h - 40, ART.w, 34};
    const std::vector<std::uint32_t> bare = parts::pixels_in(canvas, foot);

    make();

    EXPECT_GT(parts::changed(parts::pixels_in(canvas, foot), bare), 0);
}

TEST_F(LibraryCardTest, TheTitleScreenIsDrawnOnceItArrives) {
    make();

    const BLRgba32 red(0xffd02020);

    EXPECT_EQ(parts::count_near(canvas, ART, red, 40), 0);

    shot = solid(320, 200, red);
    card->invalidate();
    canvas.frame();

    EXPECT_GT(parts::count_near(canvas, ART, red, 60), ART.w * ART.h / 2);
}

// A shelf rebuilt over a card it keeps hands it what a new one would be made from.
TEST_F(LibraryCardTest, ACardMadeOverPaintsAsANewOne) {
    const std::vector<std::function<void(Card &)>> changes = {
            [this](Card &into) {
                into.artKey = "plutonia.wad";
                shot = solid(320, 200, BLRgba32(0xff2040d0));
            },
            [](Card &into) { into.caption.clear(); },
            [](Card &into) { into.playable = false; },
    };

    for (size_t at = 0; at < changes.size(); ++at) {
        shot = solid(320, 200, BLRgba32(0xffd02020));
        make();
        changes[at](*card);
        canvas.full();

        const std::vector<std::uint32_t> over = canvas.pixels();

        auto fresh = prepared();

        changes[at](*fresh);
        make(std::move(fresh));

        EXPECT_EQ(canvas.pixels(), over) << "change " << at;
    }
}

TEST_F(LibraryCardTest, WithoutATitleScreenTheMarkStandsInTheArt) {
    make();

    const parts::Ink drawn = parts::ink(canvas, ART, Cards::artMiddle.value);

    EXPECT_GT(drawn.pixels, 0U);
}

TEST_F(LibraryCardTest, AResizedCardSettlesToTheSamePixelsAsAFreshOne) {
    make();
    hover(MIDDLE);
    away();

    const BLRect wide{BOX.x, BOX.y, 300.0, BOX.h};

    static_cast<parts::Holder *>(card->parent())->spot = wide;
    ui().relayout();
    canvas.run(30);
    canvas.full();

    const std::vector<std::uint32_t> resized = canvas.pixels();

    make(prepared(), wide);

    EXPECT_EQ(canvas.pixels(), resized);
}

TEST_F(LibraryCardTest, ADroppedSheetIsRebuiltTheSame) {
    make();

    const std::vector<std::uint32_t> resting = canvas.pixels();

    card->dropSheet();
    canvas.full();

    EXPECT_EQ(canvas.pixels(), resting);
}

TEST_F(LibraryCardTest, ADarkShadeRepaintsTheCardDark) {
    make();

    const std::vector<std::uint32_t> light = canvas.pixels();

    ttk::Theme::set_mode(ttk::Theme::Mode::Dark);
    canvas.full();

    const std::vector<std::uint32_t> dark = canvas.pixels();

    EXPECT_GT(parts::changed(dark, light), 0);

    make();

    EXPECT_EQ(canvas.pixels(), dark);
}

// A resting card copies its face into the window, which only an opaque face survives.
TEST_F(LibraryCardTest, ACardAtRestLeavesTheWindowOpaque) {
    const auto opaque = [this] {
        for (const std::uint32_t pixel : canvas.pixels()) {
            if ((pixel >> 24U) != 0xffU) {
                return false;
            }
        }

        return true;
    };

    for (const ttk::Theme::Mode mode : {ttk::Theme::Mode::Light, ttk::Theme::Mode::Dark}) {
        ttk::Theme::set_mode(mode);

        shot = BLImage();
        make();
        EXPECT_TRUE(opaque());

        shot = solid(320, 200, BLRgba32(0x80d02020));

        auto unplayable = prepared();

        unplayable->playable = false;
        make(std::move(unplayable));
        EXPECT_TRUE(opaque());

        card->setStatus(State::RunState::Launching, "");
        canvas.run(static_cast<int>(ttk::StatusIndicator::BEAT * 60.0) + 4);
        EXPECT_TRUE(opaque());
    }
}

TEST_F(LibraryCardTest, ACardAtRestPaintsNothingMore) {
    make();

    EXPECT_EQ(canvas.frame(), 0U);
}


class LibraryCardOnTheShelf : public parts::Settled {
protected:
    void open(Config what, const State::Shelf shelf = State::Shelf::Profiles) {
        State::get().nav.shelf = shelf;
        parts::install(rig(), std::move(what));
        rig().turn();
    }

    std::vector<Card *> cards() { return parts::all<Card>(&rig().library()); }

    static std::vector<std::string> texts(const Card &card) {
        std::vector<std::string> out;

        for (const State::BadgeSpec &badge : card.badges) {
            out.push_back(badge.text);
        }

        return out;
    }

    // The first profile on a port that is no DOSBox one.
    static Config native() {
        Config made = tests::Fixtures::config(3, 2);

        made.ports.front().dosbox = false;

        return made;
    }
};

TEST_F(LibraryCardOnTheShelf, AProfileCardIsMadeFromItsProfile) {
    open(native());

    const std::vector<Card *> made = cards();

    ASSERT_EQ(made.size(), 3U);
    EXPECT_EQ(made[0]->title, Session::get().config().profiles[0].name);
    EXPECT_EQ(made[0]->subtitle, "Port 0");
    EXPECT_EQ(made[0]->caption, "GAME 0");
    EXPECT_EQ(made[0]->primary, Card::Primary::Open);
}

TEST_F(LibraryCardOnTheShelf, AReadyProfileIsPlayableAndCountsWhatItLoads) {
    open(native());

    const Card &first = *cards()[0];

    EXPECT_TRUE(first.playable);
    EXPECT_EQ(texts(first), (std::vector<std::string>{"1 of 2 loaded"}));
}

TEST_F(LibraryCardOnTheShelf, EveryFileLoadedIsCountedPlainly) {
    Config what = native();

    for (FileEntry &entry : what.profiles.front().files) {
        entry.enabled = true;
    }

    open(std::move(what));

    EXPECT_EQ(texts(*cards()[0]), (std::vector<std::string>{"2 files"}));
}

TEST_F(LibraryCardOnTheShelf, AProfileWithNoPortIsNotPlayable) {
    Config what = native();

    what.profiles.front().port.clear();
    open(std::move(what));

    const Card &first = *cards()[0];

    EXPECT_FALSE(first.playable);
    EXPECT_EQ(first.subtitle, "No source port");
    ASSERT_FALSE(first.badges.empty());
    EXPECT_EQ(first.badges.front().text, "No port");
    EXPECT_EQ(first.badges.front().kind, State::BadgeKind::Warning);
}

TEST_F(LibraryCardOnTheShelf, AProfileOnADosPortSaysSoFirst) {
    open(tests::Fixtures::config(3, 2));

    const Card &first = *cards()[0];

    ASSERT_FALSE(first.badges.empty());
    EXPECT_EQ(first.badges.front().text, "DOS");
    EXPECT_EQ(first.badges.front().kind, State::BadgeKind::Muted);
}

TEST_F(LibraryCardOnTheShelf, AHostingAndRecordingProfileSaysBoth) {
    open(native());

    EXPECT_EQ(texts(*cards()[1]), (std::vector<std::string>{"1 of 2 loaded", "Hosting", "Recording"}));
}

TEST_F(LibraryCardOnTheShelf, AJoiningAndReplayingProfileSaysBoth) {
    Config what = native();

    what.profiles[2].multiplayer.players = 0;
    what.profiles[2].multiplayer.host = "10.0.0.2";
    open(std::move(what));

    EXPECT_EQ(texts(*cards()[2]), (std::vector<std::string>{"1 of 2 loaded", "Multiplayer", "Replay"}));
}

TEST_F(LibraryCardOnTheShelf, ANetgameThePortCannotPlayIsNotBadged) {
    Config what = native();

    what.ports[1].file = "/ports/dsda-doom";
    open(std::move(what));

    EXPECT_EQ(texts(*cards()[1]), (std::vector<std::string>{"1 of 2 loaded", "Recording"}));
}

TEST_F(LibraryCardOnTheShelf, AGameCardPlaysOnAClick) {
    open(tests::Fixtures::grounded(1, 0), State::Shelf::Games);

    const std::vector<Card *> made = cards();

    ASSERT_EQ(made.size(), 12U);
    EXPECT_EQ(made[0]->title, "Test Doom");
    EXPECT_EQ(made[0]->primary, Card::Primary::Play);
    EXPECT_TRUE(made[0]->playable);
    EXPECT_TRUE(made[0]->badges.empty());
}

TEST_F(LibraryCardOnTheShelf, AGameWhoseFileIsGoneIsMarkedMissing) {
    open(tests::Fixtures::grounded(1, 0), State::Shelf::Games);

    const Card &gone = *cards()[2];

    EXPECT_FALSE(gone.playable);
    ASSERT_EQ(gone.badges.size(), 1U);
    EXPECT_EQ(gone.badges.front().text, "Missing");
    EXPECT_EQ(gone.badges.front().kind, State::BadgeKind::Danger);
    EXPECT_EQ(gone.playHint, "This file is not where the library says it is");
}

TEST_F(LibraryCardOnTheShelf, TheTitleScreenOfAGameOnDiskArrives) {
    open(tests::Fixtures::grounded(1, 0), State::Shelf::Games);
    rig().ready();

    Card &first = *cards()[0];

    ASSERT_TRUE(first.artwork);
    EXPECT_FALSE(first.artwork(first.artKey).is_empty());
}

TEST_F(LibraryCardOnTheShelf, AGameThatIsGoneHasNoArtToWaitFor) {
    open(tests::Fixtures::grounded(1, 0), State::Shelf::Games);

    EXPECT_TRUE(cards()[2]->artKey.empty());
}

TEST_F(LibraryCardOnTheShelf, ALaunchThatIsRefusedMarksTheCardFailed) {
    open(native());

    Card &first = *cards()[0];

    ASSERT_TRUE(first.played);

    first.played();
    rig().turn();

    EXPECT_EQ(first.status, State::RunState::Failed);
    EXPECT_FALSE(first.hint.empty());
    EXPECT_EQ(rig().notify().messages().back().title, "Nothing was launched");
}

TEST_F(LibraryCardOnTheShelf, ARefusedLaunchLeavesTheShelfStanding) {
    open(native());

    const Card *first = cards()[0];

    first->played();
    rig().turn();

    EXPECT_EQ(cards()[0], first);
}

TEST_F(LibraryCardOnTheShelf, RenameAndDeleteAreOffWhileTheProfileRuns) {
    open(native());
    rig().runs().began(ConfigBridge::profileKey("profile-0"), "Run", "gzdoom", 987654321, ttk::Process::NOTHING);
    rig().turn();

    const Card &first = *cards()[0];
    int off = 0;

    for (const ttk::Menu::Row &row : first.actions) {
        off += row.disabled && !row.separator ? 1 : 0;
    }

    EXPECT_EQ(off, 2);
    EXPECT_EQ(first.status, State::RunState::Launching);
}

TEST_F(LibraryCardOnTheShelf, ClickingAProfileCardOpensItsPage) {
    open(native());
    canvas().full();

    const Card &second = *cards()[1];

    parts::click(rig().ui(), second.box().x + 60.0, second.box().y + Cards::art + 30.0);

    EXPECT_EQ(Session::get().config().activeProfileId, "profile-1");
    EXPECT_EQ(rig().visited, (std::vector<State::Page>{State::Page::Profile}));
}

TEST_F(LibraryCardOnTheShelf, HoveringAcrossTheShelfRepaintsLikeAFullFrame) {
    open(native());
    canvas().full();

    for (const Card *card : cards()) {
        rig().ui().motion(card->box().x + 40.0, card->box().y + 40.0);
        canvas().run(3);
    }

    const parts::Ink off = parts::mismatch(canvas());

    EXPECT_EQ(off.pixels, 0U) << off;
}

TEST_F(LibraryCardTest, RestingOnACardWithAStatusOffersWhatItSays) {
    make();
    card->setStatus(State::RunState::Failed, "Exited with code 1");
    ui().motion(MIDDLE.x, MIDDLE.y);

    EXPECT_EQ(ui().hovered(), card);
    EXPECT_EQ(ui().hovered()->hint, card->hint);
    EXPECT_FALSE(card->hint.empty());
}

}
