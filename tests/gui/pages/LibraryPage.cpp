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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notice.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/overlays/Menu.h"

#include "Pages.h"
#include "gui/components/LibraryCard.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfileBridge.h"
#include "gui/pages/LibraryPage.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"
#include "support/Corpus.h"

namespace {

using components::LibraryCard;

class LibraryPageTest : public paging::Paged {
protected:
    pages::LibraryPage &page() { return rig().library(); }

    std::vector<LibraryCard *> cards() { return paging::all<LibraryCard>(&page()); }

    std::vector<std::string> titles() {
        std::vector<std::string> out;

        for (const LibraryCard *card : cards()) {
            out.push_back(card->title);
        }

        return out;
    }

    void games() {
        State::get().nav.shelf = State::Shelf::Games;
        State::get().touch();
        rig().turn();
    }

    ttk::Field *filter() {
        const std::vector<ttk::Field *> fields = paging::all<ttk::Field>(&page());

        return fields.size() == 1 ? fields.front() : nullptr;
    }

    ttk::Select *portPick() {
        const std::vector<ttk::Select *> picks = paging::all<ttk::Select>(&page());

        return picks.size() == 1 ? picks.front() : nullptr;
    }

    ttk::MultistateSwitch *shelfSwitch() {
        const std::vector<ttk::MultistateSwitch *> switches = paging::all<ttk::MultistateSwitch>(&page());

        return switches.size() == 1 ? switches.front() : nullptr;
    }

    ttk::Button *addPort() { return paging::hinted<ttk::Button>(&page(), "Nothing here can run until"); }

    // Picks the card's menu row called `label`.
    static void act(LibraryCard *card, const std::string &label) {
        for (const ttk::Menu::Row &row : card->actions) {
            if (row.label == label) {
                card->triggered(row.action);

                return;
            }
        }

        FAIL() << "no " << label << " on " << card->title;
    }

    static const ttk::Menu::Row *row(const LibraryCard *card, const std::string &label) {
        for (const ttk::Menu::Row &each : card->actions) {
            if (each.label == label) {
                return &each;
            }
        }

        return nullptr;
    }
};

TEST_F(LibraryPageTest, ItIsBuiltAtTheStartAndShown) {
    rig().turn();

    EXPECT_TRUE(page().visible());
    EXPECT_TRUE(paging::shows(&page(), "Library"));
}

TEST_F(LibraryPageTest, ItIsHiddenOnAnotherPage) {
    show(State::Page::Settings);

    EXPECT_FALSE(page().visible());

    show(State::Page::Library);

    EXPECT_TRUE(page().visible());
}

TEST_F(LibraryPageTest, ThereIsACardPerProfile) {
    load(paging::world(3));

    EXPECT_EQ(titles(), (std::vector<std::string>{"Alpha", "Bravo", "Charlie"}));
}

TEST_F(LibraryPageTest, TheNoteCountsTheProfiles) {
    load(paging::world(3));

    EXPECT_TRUE(paging::shows(&page(), "3 profiles · press a card to set one up, or the play button to run it"));
}

TEST_F(LibraryPageTest, OneProfileIsCountedInTheSingular) {
    load(paging::world(1));

    EXPECT_TRUE(paging::shows(&page(), "1 profile · press a card to set one up, or the play button to run it"));
}

TEST_F(LibraryPageTest, AProfileCardSaysWhatItRuns) {
    load(paging::world(1));

    ASSERT_EQ(cards().size(), 1U);

    const LibraryCard *card = cards().front();

    EXPECT_EQ(card->caption, "DOOM II");
    EXPECT_EQ(card->subtitle, "GZDoom");
    EXPECT_TRUE(card->playable);
    EXPECT_EQ(card->playHint, "Launch Alpha");
    EXPECT_EQ(card->primary, LibraryCard::Primary::Open);
}

TEST_F(LibraryPageTest, AProfileWithNothingSetSaysSo) {
    Config made = paging::world(1);

    made.profiles.front().iwad.clear();
    made.profiles.front().port.clear();
    load(made);

    const LibraryCard *card = cards().front();

    EXPECT_EQ(card->caption, "NO GAME");
    EXPECT_EQ(card->subtitle, "No source port");
    EXPECT_FALSE(card->playable);
    EXPECT_EQ(card->playHint, "This profile has no source port to run");
}

TEST_F(LibraryPageTest, AProfileCardCarriesItsBadges) {
    Config made = paging::world(1);

    made.profiles.front().port = "Vanilla";
    made.profiles.front().files = {{.file = "/addons/a.wad", .enabled = true}};
    load(made);

    EXPECT_EQ(cards().front()->badges, ProfileBridge::badgesOf(cfg().shelfProfiles.front()));
    EXPECT_EQ(cards().front()->badges.front().text, "DOS");
}

TEST_F(LibraryPageTest, LaunchIsOffForACardThatCannotRun) {
    Config made = paging::world(1);

    made.profiles.front().port.clear();
    load(made);

    ASSERT_NE(row(cards().front(), "Launch"), nullptr);
    EXPECT_TRUE(row(cards().front(), "Launch")->disabled);
}

TEST_F(LibraryPageTest, OpeningACardSelectsItAndGoesToTheProfile) {
    load(paging::world(3));

    cards()[2]->opened();

    EXPECT_EQ(cfg().profileName, "Charlie");
    EXPECT_EQ(rig().visited.back(), State::Page::Profile);
}

TEST_F(LibraryPageTest, ClickingACardOpensIt) {
    load(paging::world(3));
    rig().ready();

    const BLRect box = cards()[1]->box();

    paging::click(rig().ui(), box.x + (box.w / 2.0), box.y + 40.0);

    EXPECT_EQ(cfg().profileName, "Bravo");
    EXPECT_EQ(rig().visited.back(), State::Page::Profile);
}

TEST_F(LibraryPageTest, OpenFromTheMenuGoesToTheProfile) {
    load(paging::world(2));

    act(cards()[1], "Open");

    EXPECT_EQ(cfg().profileName, "Bravo");
    EXPECT_EQ(rig().visited.back(), State::Page::Profile);
}

TEST_F(LibraryPageTest, PlayingACardLaunchesThatProfile) {
    load(paging::world(2));

    cards()[1]->played();

    EXPECT_EQ(cfg().profileName, "Bravo");
    EXPECT_EQ(rig().runs().stateOf(ConfigBridge::profileKey("id-bravo")), State::RunState::Failed);
}

TEST_F(LibraryPageTest, DuplicateFromTheMenuAddsACopy) {
    load(paging::world(2));

    act(cards()[0], "Duplicate");
    rig().turn();

    EXPECT_EQ(titles(), (std::vector<std::string>{"Alpha", "Bravo", "Alpha (2)"}));
}

TEST_F(LibraryPageTest, RenameAsksForTheNameWithTheOldOneFilledIn) {
    load(paging::world(2));

    act(cards()[1], "Rename");

    ASSERT_EQ(rig().prompts.size(), 1U);
    EXPECT_EQ(rig().prompts.front().title, "Rename profile");
    EXPECT_EQ(rig().prompts.front().value, "Bravo");
    EXPECT_EQ(rig().prompts.front().accept, "Rename");
}

TEST_F(LibraryPageTest, AnsweringRenameRenamesTheCard) {
    load(paging::world(2));

    act(cards()[1], "Rename");
    rig().prompts.front().accepted("Zulu");
    rig().turn();

    EXPECT_EQ(titles(), (std::vector<std::string>{"Alpha", "Zulu"}));
}

TEST_F(LibraryPageTest, DeleteAsksFirstAndWarnsOfTheFolder) {
    load(paging::world(2));

    act(cards()[0], "Delete");

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Delete \"Alpha\"?");
    EXPECT_EQ(rig().asks.front().body, ProfileBridge::removalNote());
    EXPECT_TRUE(rig().asks.front().danger);
    EXPECT_EQ(config().profiles.size(), 2U);
}

TEST_F(LibraryPageTest, AnsweringDeleteRemovesTheCard) {
    load(paging::world(2));

    act(cards()[0], "Delete");
    rig().asks.front().accepted();
    rig().turn();

    EXPECT_EQ(titles(), (std::vector<std::string>{"Bravo"}));
}

TEST_F(LibraryPageTest, ARunningProfileCannotBeRenamedOrDeleted) {
    load(paging::world(2));

    rig().runs().began(ConfigBridge::profileKey("id-bravo"), "Bravo", "", 0, ttk::Process::NOTHING);
    rig().turn();

    EXPECT_TRUE(row(cards()[1], "Rename")->disabled);
    EXPECT_TRUE(row(cards()[1], "Delete")->disabled);
    EXPECT_FALSE(row(cards()[0], "Rename")->disabled);
}

TEST_F(LibraryPageTest, ARunStateReachesTheCardWithoutARebuild) {
    load(paging::world(2));

    const std::vector<LibraryCard *> before = cards();

    rig().runs().refused(ConfigBridge::profileKey("id-alpha"), "Alpha", "no port");
    rig().turn();

    EXPECT_EQ(cards(), before);
    EXPECT_EQ(cards().front()->status, State::RunState::Failed);
    EXPECT_EQ(cards().front()->statusReason, "no port");
}

TEST_F(LibraryPageTest, AChangeNoCardShowsKeepsTheCards) {
    load(paging::world(3));

    const std::vector<LibraryCard *> before = cards();

    rig().config().profile().setSkill(4);
    rig().turn();

    EXPECT_EQ(cards(), before);
}

TEST_F(LibraryPageTest, AChangeACardShowsRebuildsTheShelf) {
    load(paging::world(3));

    rig().config().profile().setPort("PrBoom");
    rig().turn();

    EXPECT_EQ(cards().front()->subtitle, "PrBoom");
}

TEST_F(LibraryPageTest, TypingInTheFilterNarrowsTheShelf) {
    load(paging::world(4));

    ASSERT_NE(filter(), nullptr);

    rig().ui().focus(filter()->input());
    rig().ui().wrote("ar");
    rig().turn();

    EXPECT_EQ(cfg().filter, "ar");
    EXPECT_EQ(titles(), (std::vector<std::string>{"Charlie"}));
}

TEST_F(LibraryPageTest, TheFilterFieldFollowsTheState) {
    load(paging::world(4));

    rig().config().library().setFilter("del");
    rig().turn();

    EXPECT_EQ(filter()->text(), "del");
    EXPECT_EQ(titles(), (std::vector<std::string>{"Delta"}));
}

TEST_F(LibraryPageTest, AFilteredShelfCannotBeReordered) {
    load(paging::world(4));

    EXPECT_TRUE(cards().front()->draggable);

    rig().config().library().setFilter("a");
    rig().turn();

    for (const LibraryCard *card : cards()) {
        EXPECT_FALSE(card->draggable) << card->title;
    }
}

TEST_F(LibraryPageTest, AFilterThatMatchesNothingEmptiesTheShelf) {
    load(paging::world(4));

    rig().config().library().setFilter("zzz");
    rig().turn();

    EXPECT_TRUE(cards().empty());
    EXPECT_TRUE(paging::shows(&page(), "4 profiles · press a card to set one up, or the play button to run it"));
}

TEST_F(LibraryPageTest, TheShelfSwitchShowsTheShelf) {
    load(paging::world(1));

    ASSERT_NE(shelfSwitch(), nullptr);
    EXPECT_EQ(shelfSwitch()->current(), static_cast<int>(State::Shelf::Profiles));

    games();

    EXPECT_EQ(shelfSwitch()->current(), static_cast<int>(State::Shelf::Games));
}

TEST_F(LibraryPageTest, TheShelfSwitchChangesTheShelf) {
    load(paging::world(1));

    shelfSwitch()->key(ttk::Key{.code = ttk::Code::Right});
    rig().turn();

    EXPECT_EQ(State::get().nav.shelf, State::Shelf::Games);
    EXPECT_EQ(titles(), (std::vector<std::string>{"Doom II", "Plutonia"}));
}

TEST_F(LibraryPageTest, TheGamesShelfCountsTheGames) {
    load(paging::world(1));
    games();

    EXPECT_TRUE(paging::shows(&page(), "2 games · everything the profiles are built on"));
}

TEST_F(LibraryPageTest, AGameCardSaysWhatItIs) {
    load(paging::world(1));
    games();

    const LibraryCard *card = cards().front();

    EXPECT_EQ(card->title, "Doom II");
    EXPECT_EQ(card->caption, ".WAD");
    EXPECT_EQ(card->primary, LibraryCard::Primary::Play);
}

TEST_F(LibraryPageTest, AGameNotOnDiskIsMissingAndCannotPlay) {
    load(paging::world(1));
    games();

    const LibraryCard *card = cards().front();

    EXPECT_FALSE(card->playable);
    ASSERT_EQ(card->badges.size(), 1U);
    EXPECT_EQ(card->badges.front().text, "Missing");
    EXPECT_EQ(card->badges.front().kind, State::BadgeKind::Danger);
    EXPECT_TRUE(row(card, "Play")->disabled);
}

TEST_F(LibraryPageTest, AGameOnDiskCanPlayAndHintsItsCommand) {
    Config made = paging::world(1);

    made.iwads.front().file = tests::Corpus::iwad().string();
    load(made);
    games();

    const LibraryCard *card = cards().front();

    EXPECT_TRUE(card->playable);
    EXPECT_TRUE(card->badges.empty());
    EXPECT_EQ(card->playHint, LibraryBridge::gameCommandLine("Doom II"));
}

TEST_F(LibraryPageTest, AGameShowsItsFolderOnlyWithPathsShown) {
    Config made = paging::world(1);

    made.general.showPaths = true;
    load(made);
    games();

    EXPECT_FALSE(cards().front()->subtitle.empty());

    rig().config().settings().setShowPaths(false);
    rig().turn();

    EXPECT_TRUE(cards().front()->subtitle.empty());
}

TEST_F(LibraryPageTest, UsingAGameSetsItOnTheActiveProfile) {
    load(paging::world(1));
    games();

    act(cards()[1], "Use in active profile");

    EXPECT_EQ(config().activeProfile().iwad, "Plutonia");

    ASSERT_FALSE(rig().notify().messages().empty());
    EXPECT_EQ(rig().notify().messages().back().body, "\"Alpha\" now plays Plutonia.");
}

TEST_F(LibraryPageTest, EditingAGameAsksWithItsNameAndFile) {
    load(paging::world(1));
    games();

    act(cards()[1], "Edit");

    ASSERT_EQ(rig().edits.size(), 1U);

    const tests::Edit &asked = rig().edits.front();

    EXPECT_EQ(asked.title, "Edit Plutonia");
    EXPECT_EQ(asked.kind, dialogs::EntryDialog::Kind::Game);
    EXPECT_EQ(asked.filters, Filters::wad());
    EXPECT_EQ(asked.remember, LastDir::WAD);
    EXPECT_EQ(asked.name, "Plutonia");
    EXPECT_EQ(asked.file, "/games/plutonia.wad");
    EXPECT_FALSE(asked.offerDos);
}

TEST_F(LibraryPageTest, AnsweringTheEditUpdatesTheGame) {
    load(paging::world(1));
    games();

    act(cards()[1], "Edit");
    rig().edits.front().accepted("Hell", "/games/plutonia.wad", false);
    rig().turn();

    EXPECT_EQ(titles(), (std::vector<std::string>{"Doom II", "Hell"}));
}

TEST_F(LibraryPageTest, RemovingAGameAsksFirst) {
    load(paging::world(1));
    games();

    act(cards()[0], "Remove");

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Remove \"Doom II\"?");
    EXPECT_TRUE(rig().asks.front().danger);

    rig().asks.front().accepted();
    rig().turn();

    EXPECT_EQ(titles(), (std::vector<std::string>{"Plutonia"}));
}

TEST_F(LibraryPageTest, TheGamesShelfOffersAPortWhenThereIsNone) {
    Config made = paging::world(1);

    made.ports.clear();
    load(made);
    games();

    ASSERT_NE(addPort(), nullptr);
    EXPECT_TRUE(paging::seen(addPort()));
    EXPECT_FALSE(paging::seen(portPick()));

    paging::click(rig().ui(), addPort());

    EXPECT_EQ(rig().visited.back(), State::Page::Engines);
}

TEST_F(LibraryPageTest, TheGamesShelfPicksItsPortWhenThereAreSome) {
    Config made = paging::world(1);

    made.general.gamePort = "PrBoom";
    load(made);
    games();

    ASSERT_NE(portPick(), nullptr);
    EXPECT_TRUE(paging::seen(portPick()));
    EXPECT_FALSE(paging::seen(addPort()));
    EXPECT_EQ(portPick()->current(), 1);
}

TEST_F(LibraryPageTest, TheProfilesShelfHidesThePortPick) {
    load(paging::world(1));

    EXPECT_FALSE(paging::seen(portPick()));
    EXPECT_FALSE(paging::seen(addPort()));
}

TEST_F(LibraryPageTest, PickingThePortSetsTheLibrarysPort) {
    load(paging::world(1));
    games();

    portPick()->key(ttk::Key{.code = ttk::Code::Down});

    EXPECT_EQ(config().general.gamePort, "GZDoom");

    rig().turn();

    EXPECT_EQ(portPick()->current(), 0);
}

TEST_F(LibraryPageTest, TheNewProfileTileAsksForAName) {
    load(paging::world(1));
    rig().ready();

    const LibraryCard *last = cards().back();
    const BLRect box = last->box();

    paging::click(rig().ui(), box.x + box.w + 60.0 + (box.w / 2.0), box.y + (box.h / 2.0));

    ASSERT_EQ(rig().prompts.size(), 1U);
    EXPECT_EQ(rig().prompts.front().title, "New profile");
    EXPECT_EQ(rig().prompts.front().value, "New profile");
}

TEST_F(LibraryPageTest, AnsweringTheNewProfileTileAddsItAndOpensIt) {
    load(paging::world(1));
    rig().ready();

    const BLRect box = cards().back()->box();

    paging::click(rig().ui(), box.x + box.w + 60.0 + (box.w / 2.0), box.y + (box.h / 2.0));

    ASSERT_EQ(rig().prompts.size(), 1U);

    rig().prompts.front().accepted("Zulu");

    EXPECT_EQ(cfg().profileName, "Zulu");
    EXPECT_EQ(rig().visited.back(), State::Page::Profile);
}

TEST_F(LibraryPageTest, TheAddGameTileOpensThePicker) {
    load(paging::world(1));
    games();
    rig().ready();

    const BLRect box = cards().back()->box();

    paging::click(rig().ui(), box.x + box.w + 60.0 + (box.w / 2.0), box.y + (box.h / 2.0));

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Add games");
}

TEST_F(LibraryPageTest, GamesPickedAreAdded) {
    load(paging::world(1));
    games();
    rig().ready();

    const BLRect box = cards().back()->box();

    paging::click(rig().ui(), box.x + box.w + 60.0 + (box.w / 2.0), box.y + (box.h / 2.0));
    rig().files().choose({tests::Corpus::iwad().string()});
    rig().turn();

    EXPECT_EQ(titles().back(), "Test Doom");
}

TEST_F(LibraryPageTest, ALongShelfScrolls) {
    load(paging::world(30));
    rig().ready();

    const std::vector<ttk::Scroll *> scrolls = paging::all<ttk::Scroll>(&page());

    ASSERT_EQ(scrolls.size(), 1U);
    EXPECT_TRUE(scrolls.front()->scrollable());

    rig().ui().wheel(-3.0, 600.0, 400.0);
    canvas().run(40);

    EXPECT_GT(scrolls.front()->offset(), 0.0);
}

TEST_F(LibraryPageTest, AShortShelfDoesNotScroll) {
    load(paging::world(2));
    rig().ready();

    EXPECT_FALSE(paging::all<ttk::Scroll>(&page()).front()->scrollable());
}

TEST_F(LibraryPageTest, DraggingACardOntoAnotherReordersTheProfiles) {
    load(paging::world(3));
    rig().ready();

    const BLRect from = cards()[0]->box();
    const BLRect onto = cards()[2]->box();
    const double x = from.x + (from.w / 2.0);
    const double y = from.y + 40.0;

    rig().ui().motion(x, y);
    rig().ui().press(ttk::Pointer{.x = x, .y = y});

    for (int step = 1; step <= 10; ++step) {
        rig().ui().motion(x + ((onto.x + (onto.w / 2.0) + 30.0 - x) * step / 10.0), y);
        canvas().run(1);
    }

    rig().ui().release(ttk::Pointer{.x = onto.x + (onto.w / 2.0) + 30.0, .y = y});
    rig().turn();

    EXPECT_EQ(config().profiles.back().name, "Alpha");
    EXPECT_EQ(config().profiles.front().name, "Bravo");
}

}
