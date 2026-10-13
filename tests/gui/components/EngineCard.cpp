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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Event.h"
#include "ttk/toolkit/controls/Button.h"

#include "Mount.h"
#include "gui/components/EngineCard.h"
#include "gui/draw/Cards.h"
#include "gui/state/State.h"
#include "support/Canvas.h"
#include "support/Rig.h"

namespace {

using Card = components::EngineCard;

constexpr BLRect BOX{20.0, 20.0, 360.0, 220.0};

constexpr BLPoint MIDDLE{BOX.x + 120.0, BOX.y + 90.0};

constexpr BLRectI TOLD{
        static_cast<int>(BOX.x) + 16, static_cast<int>(BOX.y + BOX.h - 16.0 - ttk::Theme::controlSmall) - 26, 328, 16};

class EngineCardTest : public tests::Painted {
protected:
    void SetUp() override {
        tests::Painted::SetUp();

        if (IsSkipped()) {
            return;
        }

        ttk::Theme::set_mode(ttk::Theme::Mode::Light);
    }

    tests::Canvas canvas{420, 280};

    Card *card = nullptr;

    int opened = 0;
    int started = 0;
    int moved = 0;
    int ended = 0;
    int pressed = 0;

    std::unique_ptr<Card> prepared() {
        auto made = std::make_unique<Card>();

        made->name = "GZDoom";
        made->blurb = "The OpenGL descendant of ZDoom.";
        made->told = "4.14.2 here";
        made->tags = {{.text = "Installed", .kind = State::BadgeKind::Success, .dot = false},
                      {.text = "DOS", .kind = State::BadgeKind::Muted, .dot = false}};
        made->tagHints = {"Downloaded and updated by ZDL4", ""};

        made->opened = [this] { ++opened; };
        made->dragStarted = [this](double, double) { ++started; };
        made->dragMoved = [this](double, double) { ++moved; };
        made->dragEnded = [this] { ++ended; };

        made->buttons()->append(std::make_unique<ttk::Button>("Install", [this] { ++pressed; }));

        return made;
    }

    Card *make(std::unique_ptr<Card> made) {
        card = parts::place(canvas, std::move(made), BOX);
        canvas.full();

        return card;
    }

    Card *make() { return make(prepared()); }

    ttk::Root &ui() { return canvas.ui(); }

    // The rightmost tag, its right end 16 in and its top 16 down.
    static BLPoint lastTag() { return BLPoint{BOX.x + BOX.w - 16.0 - 12.0, BOX.y + 16.0 + 11.0}; }

    std::uint32_t told() { return parts::peak(canvas, TOLD, ttk::Theme::palette().surface.value); }

    void expectFull() {
        const parts::Ink off = parts::mismatch(canvas);

        EXPECT_EQ(off.pixels, 0U) << off;
    }
};

TEST(EngineCard, StartsUnplacedAndStill) {
    const Card card;

    EXPECT_FALSE(card.draggable);
    EXPECT_EQ(card.slot(), -1);
    EXPECT_FALSE(card.sliding());
    EXPECT_TRUE(card.hoverable);
    EXPECT_TRUE(card.tags.empty());
    EXPECT_FALSE(card.working);
    EXPECT_FALSE(card.trouble);
}

TEST(EngineCard, TheButtonRowStartsEmptyAndIsTheCards) {
    const Card card;

    ASSERT_NE(card.buttons(), nullptr);
    EXPECT_TRUE(card.buttons()->children().empty());
    EXPECT_EQ(card.buttons()->parent(), &card);
}

TEST(EngineCard, RemembersItsSlot) {
    Card card;

    card.setSlot(3);

    EXPECT_EQ(card.slot(), 3);
}

TEST(EngineCard, ASlideStartsWhereItWasAndWalksHome) {
    Card card;

    card.slideFrom(-30.0, 15.0, 2.0);

    EXPECT_TRUE(card.sliding());
    EXPECT_DOUBLE_EQ(card.slide_x(), -30.0);
    EXPECT_DOUBLE_EQ(card.slide_y(), 15.0);

    EXPECT_FALSE(card.advance(2.0 + Cards::settling + 0.01));
    EXPECT_DOUBLE_EQ(card.slide_x(), 0.0);
}

TEST_F(EngineCardTest, TheButtonsRunAlongTheBottom) {
    make();

    const BLRect &row = card->buttons()->box();

    EXPECT_DOUBLE_EQ(row.x, BOX.x + 16.0);
    EXPECT_DOUBLE_EQ(row.y, BOX.y + BOX.h - 16.0 - ttk::Theme::controlSmall);
    EXPECT_DOUBLE_EQ(row.w, BOX.w - 32.0);
    EXPECT_DOUBLE_EQ(row.h, ttk::Theme::controlSmall);
}

TEST_F(EngineCardTest, AClickAboveTheButtonsOpensIt) {
    make();
    parts::click(ui(), MIDDLE.x, MIDDLE.y);

    EXPECT_EQ(opened, 1);
}

TEST_F(EngineCardTest, AClickOnTheButtonRowDoesNotOpenIt) {
    make();

    const BLRect &row = card->buttons()->box();

    parts::click(ui(), row.x + row.w - 10.0, row.y + (row.h / 2.0));

    EXPECT_EQ(opened, 0);
}

TEST_F(EngineCardTest, AButtonOnTheCardIsItsOwn) {
    make();
    parts::click(ui(), *card->buttons()->children().front());

    EXPECT_EQ(pressed, 1);
    EXPECT_EQ(opened, 0);
}

TEST_F(EngineCardTest, ReleasingOffTheCardDoesNothing) {
    make();
    parts::sweep(ui(), MIDDLE.x, MIDDLE.y, 410.0, 270.0);

    EXPECT_EQ(opened, 0);
}

TEST_F(EngineCardTest, ADraggableCardReportsTheCarry) {
    auto made = prepared();

    made->draggable = true;
    make(std::move(made));
    parts::sweep(ui(), MIDDLE.x, MIDDLE.y, MIDDLE.x + 50.0, MIDDLE.y);

    EXPECT_EQ(started, 1);
    EXPECT_EQ(moved, 1);
    EXPECT_EQ(ended, 1);
    EXPECT_EQ(opened, 0);
}

TEST_F(EngineCardTest, AMoveInsideTheSlackIsAClick) {
    auto made = prepared();

    made->draggable = true;
    make(std::move(made));
    parts::sweep(ui(), MIDDLE.x, MIDDLE.y, MIDDLE.x + 3.0, MIDDLE.y);

    EXPECT_EQ(started, 0);
    EXPECT_EQ(opened, 1);
}

TEST_F(EngineCardTest, ACardOnTheBrowseShelfIsNotCarried) {
    make();
    parts::sweep(ui(), MIDDLE.x, MIDDLE.y, MIDDLE.x + 50.0, MIDDLE.y);

    EXPECT_EQ(started, 0);
}

TEST_F(EngineCardTest, ACarriedCardShowsTheGrabbingHand) {
    auto made = prepared();

    made->draggable = true;
    make(std::move(made));

    EXPECT_EQ(card->cursor_at(MIDDLE.x, MIDDLE.y), ttk::Cursor::Default);

    ui().motion(MIDDLE.x, MIDDLE.y);
    ui().press(ttk::Pointer{.x = MIDDLE.x, .y = MIDDLE.y});
    ui().motion(MIDDLE.x + 50.0, MIDDLE.y);

    EXPECT_EQ(card->cursor_at(MIDDLE.x, MIDDLE.y), ttk::Cursor::Grabbing);

    ui().release(ttk::Pointer{.x = MIDDLE.x + 50.0, .y = MIDDLE.y});

    EXPECT_EQ(card->cursor_at(MIDDLE.x, MIDDLE.y), ttk::Cursor::Default);
}

TEST_F(EngineCardTest, RestingOnATagSaysWhatItMeans) {
    make();
    ui().motion(lastTag().x - 60.0, lastTag().y);

    EXPECT_EQ(card->hint, "Downloaded and updated by ZDL4");
}

TEST_F(EngineCardTest, ATagWithNothingToSayHasNoHint) {
    make();
    ui().motion(lastTag().x, lastTag().y);

    EXPECT_TRUE(card->hint.empty());
}

TEST_F(EngineCardTest, OffTheTagsThereIsNoHint) {
    make();
    ui().motion(lastTag().x - 60.0, lastTag().y);
    ui().motion(MIDDLE.x, MIDDLE.y);

    EXPECT_TRUE(card->hint.empty());
}

TEST_F(EngineCardTest, LeavingClearsTheHint) {
    make();
    ui().motion(lastTag().x - 60.0, lastTag().y);
    ui().motion(410.0, 270.0);

    EXPECT_TRUE(card->hint.empty());
}

TEST_F(EngineCardTest, TheTagsSitAtTheTopRight) {
    auto made = prepared();

    made->tags.clear();
    make(std::move(made));

    const BLRectI corner{static_cast<int>(BOX.x + BOX.w) - 150, static_cast<int>(BOX.y) + 16, 134, 22};
    const size_t bare = parts::ink(canvas, corner, ttk::Theme::palette().surface.value).pixels;

    make();

    const parts::Ink drawn = parts::ink(canvas, corner, ttk::Theme::palette().surface.value);

    EXPECT_GT(drawn.pixels, bare);
    EXPECT_EQ(drawn.right, static_cast<int>(BOX.x + BOX.w) - 16);
}

TEST_F(EngineCardTest, WhatIsKnownIsFaint) {
    make();

    const ttk::Theme::Palette &palette = ttk::Theme::palette();

    EXPECT_TRUE(parts::leans(told(), palette.faint, palette.danger));
}

TEST_F(EngineCardTest, TroubleIsWrittenInDanger) {
    auto made = prepared();

    made->told = "The download stopped half way.";
    made->trouble = true;
    make(std::move(made));

    const ttk::Theme::Palette &palette = ttk::Theme::palette();

    EXPECT_TRUE(parts::leans(told(), palette.danger, palette.faint));
}

TEST_F(EngineCardTest, AMissingPortInTroubleHasItsNameInDanger) {
    auto made = prepared();

    made->missing = true;
    made->trouble = true;
    made->tags.clear();
    make(std::move(made));

    const BLRectI name{static_cast<int>(BOX.x) + 16, static_cast<int>(BOX.y) + 16, 150, 22};

    EXPECT_GT(parts::count_near(canvas, name, ttk::Theme::palette().danger, 60), 0);
}

TEST_F(EngineCardTest, AMissingPortWithoutTroubleKeepsItsName) {
    auto made = prepared();

    made->missing = true;
    made->tags.clear();
    make(std::move(made));

    const BLRectI name{static_cast<int>(BOX.x) + 16, static_cast<int>(BOX.y) + 16, 150, 22};

    EXPECT_EQ(parts::count_near(canvas, name, ttk::Theme::palette().danger, 60), 0);
}

TEST_F(EngineCardTest, AFetchShowsABarFilledToItsProgress) {
    auto made = prepared();

    made->working = true;
    made->progress = 0.5;
    make(std::move(made));

    const BLRectI track{TOLD.x, TOLD.y + 6, TOLD.w, 4};
    const int filled = parts::count_near(canvas, track, ttk::Theme::palette().accent, 30);

    EXPECT_NEAR(filled, TOLD.w * 4 / 2, TOLD.w / 4);
}

TEST_F(EngineCardTest, AProgressPastTheEndFillsTheBarOnce) {
    auto made = prepared();

    made->working = true;
    made->progress = 3.0;
    make(std::move(made));

    const BLRectI track{TOLD.x - 4, TOLD.y + 6, TOLD.w + 8, 4};

    EXPECT_LE(parts::count_near(canvas, track, ttk::Theme::palette().accent, 30), TOLD.w * 4);
}

TEST_F(EngineCardTest, ABarStandsInsteadOfWhatIsKnown) {
    auto made = prepared();

    made->working = true;
    made->progress = 0.0;
    make(std::move(made));

    const ttk::Theme::Palette &palette = ttk::Theme::palette();

    EXPECT_FALSE(parts::leans(told(), palette.faint, palette.surface));
}

TEST_F(EngineCardTest, HoverLightsTheCardAndLeavingPutsItBack) {
    make();

    const std::vector<std::uint32_t> resting = canvas.pixels();

    ui().motion(MIDDLE.x, MIDDLE.y);
    canvas.run(20);

    EXPECT_GT(parts::changed(canvas.pixels(), resting), 0);

    ui().motion(410.0, 270.0);
    canvas.run(20);

    EXPECT_EQ(canvas.pixels(), resting);
}

TEST_F(EngineCardTest, TheLightStaysWhileThePointerIsOnAButton) {
    make();
    ui().motion(MIDDLE.x, MIDDLE.y);
    canvas.run(20);

    const std::uint32_t lit = canvas.at(static_cast<int>(BOX.x) + 4, static_cast<int>(MIDDLE.y));

    parts::click(ui(), *card->buttons()->children().front());
    canvas.run(20);

    EXPECT_EQ(canvas.at(static_cast<int>(BOX.x) + 4, static_cast<int>(MIDDLE.y)), lit);
}

TEST_F(EngineCardTest, HoverRepaintsLikeAFullFrame) {
    make();
    ui().motion(MIDDLE.x, MIDDLE.y);
    canvas.run(3);
    ui().motion(card->buttons()->children().front()->box().x + 5.0, card->buttons()->box().y + 5.0);
    canvas.run(3);
    expectFull();
}

class EngineCardOnTheShelf : public parts::Settled {
protected:
    void SetUp() override {
        parts::Settled::SetUp();

        if (IsSkipped()) {
            return;
        }

        State::get().nav.engines = State::EnginesTab::Browse;
        rig().go(State::Page::Engines);
        rig().turn();

        // The first visit asks for the catalogue, which lands as a post. The rows a case sets come after it.
        rig().ready();
    }

    Card &shown(State::EngineRow row) {
        row.name = "Woof!";
        State::get().ports.rows = {row};
        State::get().touch();
        rig().turn();

        const std::vector<Card *> made = parts::all<Card>(&rig().enginesPage());

        EXPECT_EQ(made.size(), 1U);

        return *made.front();
    }

    static State::EngineRow row(const State::EngineState status) {
        State::EngineRow made;

        made.status = status;
        made.version = "15.2.0";
        made.sizeText = "4 MB";

        return made;
    }

    static std::vector<std::string> texts(const Card &card) {
        std::vector<std::string> out;

        for (const State::BadgeSpec &tag : card.tags) {
            out.push_back(tag.text);
        }

        return out;
    }
};

TEST_F(EngineCardOnTheShelf, AWaitingPortSaysItsReleaseAndSize) {
    Card &card = shown(row(State::EngineState::Waiting));

    EXPECT_TRUE(card.tags.empty());
    EXPECT_EQ(card.told, "15.2.0 · 4 MB");
    EXPECT_FALSE(card.working);
}

TEST_F(EngineCardOnTheShelf, AReadyPortCarriesNoTag) {
    Card &card = shown(row(State::EngineState::Ready));

    EXPECT_TRUE(card.tags.empty());
    EXPECT_FALSE(card.trouble);
}

TEST_F(EngineCardOnTheShelf, ACheckingPortSaysSo) {
    Card &card = shown(row(State::EngineState::Checking));

    EXPECT_EQ(texts(card), (std::vector<std::string>{"Checking"}));
    EXPECT_EQ(card.tags.front().kind, State::BadgeKind::None);
}

TEST_F(EngineCardOnTheShelf, APortFromItsOwnSiteSaysSo) {
    Card &card = shown(row(State::EngineState::Elsewhere));

    EXPECT_EQ(texts(card), (std::vector<std::string>{"Its own site"}));
    EXPECT_EQ(card.tags.front().kind, State::BadgeKind::Warning);
    EXPECT_EQ(card.told, "Fetched from its own site");
}

TEST_F(EngineCardOnTheShelf, AnUnavailablePortSaysThereIsNothingToFetch) {
    Card &card = shown(row(State::EngineState::Unavailable));

    EXPECT_EQ(texts(card), (std::vector<std::string>{"No downloads available"}));
    EXPECT_EQ(card.tags.front().kind, State::BadgeKind::Warning);
}

TEST_F(EngineCardOnTheShelf, AFetchShowsTheBarAtItsProgress) {
    State::EngineRow fetching = row(State::EngineState::Fetching);

    fetching.progress = 0.25F;

    Card &card = shown(fetching);

    EXPECT_TRUE(card.working);
    EXPECT_DOUBLE_EQ(card.progress, 0.25);
    EXPECT_TRUE(card.tags.empty());
}

TEST_F(EngineCardOnTheShelf, UnpackingFillsTheBar) {
    Card &card = shown(row(State::EngineState::Unpacking));

    EXPECT_TRUE(card.working);
    EXPECT_DOUBLE_EQ(card.progress, 1.0);
}

TEST_F(EngineCardOnTheShelf, AnInstalledPortSaysWhatIsHere) {
    State::EngineRow installed = row(State::EngineState::Installed);

    installed.have = "15.2.0";

    Card &card = shown(installed);

    EXPECT_EQ(texts(card), (std::vector<std::string>{"Installed"}));
    EXPECT_EQ(card.tags.front().kind, State::BadgeKind::Success);
    EXPECT_EQ(card.told, "15.2.0 here");
}

TEST_F(EngineCardOnTheShelf, AnInstalledPortBehindTheReleaseOffersTheUpdate) {
    State::EngineRow behind = row(State::EngineState::Installed);

    behind.have = "15.1.0";

    Card &card = shown(behind);

    EXPECT_EQ(texts(card), (std::vector<std::string>{"Update"}));
    EXPECT_EQ(card.told, "15.1.0 here · 15.2.0 released");
}

TEST_F(EngineCardOnTheShelf, AFailedPortIsInTroubleAndSaysWhy) {
    State::EngineRow failed = row(State::EngineState::Failed);

    failed.error = "The archive was damaged.";

    Card &card = shown(failed);

    EXPECT_EQ(texts(card), (std::vector<std::string>{"Failed"}));
    EXPECT_EQ(card.tags.front().kind, State::BadgeKind::Danger);
    EXPECT_TRUE(card.trouble);
    EXPECT_EQ(card.told, "The archive was damaged.");
}

TEST_F(EngineCardOnTheShelf, ADosPortSaysSoFirst) {
    State::EngineRow dos = row(State::EngineState::Checking);

    dos.dos = true;

    Card &card = shown(dos);

    EXPECT_EQ(texts(card), (std::vector<std::string>{"DOS", "Checking"}));
}

TEST_F(EngineCardOnTheShelf, AProgressMovingOnlyMovesTheBar) {
    State::EngineRow fetching = row(State::EngineState::Fetching);

    fetching.progress = 0.25F;

    Card *first = &shown(fetching);

    State::get().ports.rows.front().progress = 0.75F;
    State::get().touch();
    rig().turn();

    EXPECT_EQ(parts::all<Card>(&rig().enginesPage()).front(), first);
    EXPECT_DOUBLE_EQ(first->progress, 0.75);
}

}
