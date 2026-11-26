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
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/notices/Notice.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/overlays/Menu.h"

#include "Pages.h"
#include "core/config/Import.h"
#include "core/config/Session.h"
#include "core/launch/Shortcut.h"
#include "gui/components/EngineCard.h"
#include "gui/components/LibraryCard.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/draw/Band.h"
#include "gui/draw/Cards.h"
#include "gui/model/ConfigBridge.h"
#include "gui/model/ProfileBridge.h"
#include "gui/pages/LibraryPage.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

using components::LibraryCard;

class LibraryPageTest : public paging::Paged {
protected:
    pages::LibraryPage &page() { return rig().library(); }

    // The ones on the shelf. A card the filter hides is kept on the grid, unseen.
    std::vector<LibraryCard *> cards() {
        std::vector<LibraryCard *> shown = held();

        std::erase_if(shown, [](const LibraryCard *card) { return !card->visible(); });

        return shown;
    }

    std::vector<LibraryCard *> held() { return paging::all<LibraryCard>(&page()); }

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

    ttk::Select *enginePick() {
        const std::vector<ttk::Select *> picks = paging::all<ttk::Select>(&page());

        return picks.size() == 1 ? picks.front() : nullptr;
    }

    ttk::MultistateSwitch *shelfSwitch() {
        const std::vector<ttk::MultistateSwitch *> switches = paging::all<ttk::MultistateSwitch>(&page());

        return switches.size() == 1 ? switches.front() : nullptr;
    }

    ttk::Button *addEngine() { return paging::hinted<ttk::Button>(&page(), "Nothing here can run until"); }

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

    // Bravo and Delta in Megawads, Alpha and Foxtrot in Speedruns, Echo in a folded Co-op, Charlie in none.
    static Config shelved(const bool on = true) {
        Config made = paging::world(6);
        const std::vector<int> groups = {2, 1, 0, 1, 3, 2};

        made.general.groupLibrary = on;
        made.profileGroups = {{.id = 1, .name = "Megawads", .collapsed = false},
                              {.id = 2, .name = "Speedruns", .collapsed = false},
                              {.id = 3, .name = "Co-op", .collapsed = true}};

        for (size_t at = 0; at < groups.size(); ++at) {
            made.profiles[at].slot.group = groups[at];
        }

        return made;
    }

    static std::vector<std::string> profileNames() {
        std::vector<std::string> out;

        for (const Profile &profile : config().profiles) {
            out.push_back(profile.name);
        }

        return out;
    }

    // The middle of the band above the cards of `card`, past its caption.
    static BLPoint bandAbove(const LibraryCard *card) {
        return BLPoint{card->box().x + 200.0, card->box().y - Band::STRIP + (Band::LINE / 2.0)};
    }

    // Whether the rule of the band above the cards of `card` is drawn in `tone`.
    bool ruled(const LibraryCard *card, const BLRgba32 tone) {
        const int x = static_cast<int>(card->box().x + 400.0);
        const int top = static_cast<int>(card->box().y - Band::STRIP);

        for (int y = top; y < top + static_cast<int>(Band::LINE); ++y) {
            if (canvas().at(x, y) == tone.value) {
                return true;
            }
        }

        return false;
    }

    // The menu button of the band above the cards of `card`.
    static BLRect menuAbove(const LibraryCard *card) {
        const BLRect shelf = card->parent()->box();

        return Band::menuBox(BLRect{shelf.x + ttk::Theme::bleed,
                                    card->box().y - Band::STRIP,
                                    shelf.w - (ttk::Theme::bleed * 2.0),
                                    Band::LINE});
    }

    // Presses the card and walks it to `to` in ten steps, without letting go.
    void carry(const LibraryCard *card, const BLPoint &to) {
        const double x = card->box().x + (card->box().w / 2.0);
        const double y = card->box().y + 40.0;

        rig().ui().motion(x, y);
        rig().ui().press(ttk::Pointer{.x = x, .y = y});

        for (int step = 1; step <= 10; ++step) {
            rig().ui().motion(x + ((to.x - x) * step / 10.0), y + ((to.y - y) * step / 10.0));
            canvas().run(1);
        }

        canvas().run(30);
    }

    ttk::Label *newGroup() {
        for (ttk::Label *label : paging::all<ttk::Label>(&page())) {
            if (label->text() == "New group") {
                return label;
            }
        }

        return nullptr;
    }
};

namespace {

BLPoint shelfMiddle(pages::LibraryPage &page) {
    const BLRect shelf = paging::all<ttk::Scroll>(&page).front()->box();

    return BLPoint{shelf.x + (shelf.w / 2.0), shelf.y + (shelf.h / 2.0)};
}

}

TEST_F(LibraryPageTest, GamesDroppedOnTheGamesShelfAreAddedAndTheRestLeftOut) {
    load(paging::world(1));
    games();

    const BLPoint at = shelfMiddle(page());
    const size_t before = Session::get().config().iwads.size();

    rig().ui().drag_files(at.x, at.y);

    EXPECT_EQ(rig().ui().drag_target(), &page());
    EXPECT_TRUE(rig().ui().drop_files({"/games/tnt.wad", "/games/tnt.deh"}, at.x, at.y));
    rig().turn();

    ASSERT_EQ(Session::get().config().iwads.size(), before + 1);
    EXPECT_EQ(Session::get().config().iwads.back().file, "/games/tnt.wad");
    ASSERT_FALSE(rig().notify().messages().empty());
    EXPECT_EQ(rig().notify().messages().back().body, "tnt.deh is not a game, so it was left out.");
}

TEST_F(LibraryPageTest, AZdlDroppedOnTheProfilesShelfLoadsAsAProfile) {
    load(paging::world(1));
    rig().turn();

    Profile written;

    written.name = "Imported";
    written.iwad = "Doom II";

    const std::filesystem::path file = tests::Sandbox::scratch("library-drop") / "imported.zdl";

    ASSERT_TRUE(Import::saveZdlFile(file, written));

    const BLPoint at = shelfMiddle(page());

    rig().ui().drag_files(at.x, at.y);

    EXPECT_EQ(rig().ui().drag_target(), &page());
    EXPECT_TRUE(rig().ui().drop_files({file.string(), "/games/tnt.wad"}, at.x, at.y));
    rig().turn();

    ASSERT_EQ(Session::get().config().profiles.size(), 2U);
    EXPECT_EQ(Session::get().config().activeProfile().name, "Imported");
    EXPECT_EQ(Session::get().config().iwads.size(), paging::world(1).iwads.size());
}

TEST_F(LibraryPageTest, FilesDroppedOnTheHeaderAreRefused) {
    load(paging::world(1));
    games();

    // The filter takes the pointer, so the drop reaches the page through it.
    const BLRect field = filter()->box();
    const double x = field.x + (field.w / 2.0);
    const double y = field.y + (field.h / 2.0);

    rig().ui().motion(x, y);

    ASSERT_NE(rig().ui().hovered(), nullptr);

    rig().ui().drag_files(x, y);

    EXPECT_EQ(rig().ui().drag_target(), nullptr);
    EXPECT_FALSE(rig().ui().drop_files({"/games/tnt.wad"}, x, y));
}

TEST_F(LibraryPageTest, SeveralZdlFilesLoadWithOneToast) {
    load(paging::world(1));
    rig().turn();

    const std::filesystem::path folder = tests::Sandbox::scratch("library-drops");
    std::vector<std::string> files;

    for (const char *name : {"one", "two"}) {
        Profile written;

        written.name = name;
        written.iwad = "Doom II";

        const std::filesystem::path file = folder / (std::string(name) + ".zdl");

        ASSERT_TRUE(Import::saveZdlFile(file, written));
        files.push_back(file.string());
    }

    const size_t before = rig().notify().messages().size();
    const BLPoint at = shelfMiddle(page());

    EXPECT_TRUE(rig().ui().drop_files(files, at.x, at.y));
    rig().turn();

    EXPECT_EQ(Session::get().config().profiles.size(), 3U);
    ASSERT_EQ(rig().notify().messages().size(), before + 1);
    EXPECT_EQ(rig().notify().messages().back().body, "Added 2 profiles from .zdl files.");
}

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
    made.profiles.front().engine.clear();
    load(made);

    const LibraryCard *card = cards().front();

    EXPECT_EQ(card->caption, "NO GAME");
    EXPECT_EQ(card->subtitle, "No source port");
    EXPECT_FALSE(card->playable);
    EXPECT_EQ(card->playHint, "This profile has no source port to run");
}

TEST_F(LibraryPageTest, AProfileCardCarriesItsBadges) {
    Config made = paging::world(1);

    made.profiles.front().engine = "Vanilla";
    made.profiles.front().files = {{.file = "/addons/a.wad", .enabled = true}};
    load(made);

    EXPECT_EQ(cards().front()->badges, ProfileBridge::badgesOf(cfg().shelfProfiles.front()));
    EXPECT_EQ(cards().front()->badges.front().text, "DOS");
}

TEST_F(LibraryPageTest, LaunchIsOffForACardThatCannotRun) {
    Config made = paging::world(1);

    made.profiles.front().engine.clear();
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

// Windows would put a real icon on the desktop.
#ifndef _WIN32
TEST_F(LibraryPageTest, TheShortcutFromTheMenuIsForThatCard) {
    load(paging::world(2));

    act(cards()[1], Shortcut::CREATE);

    ASSERT_FALSE(rig().notify().messages().empty());
    EXPECT_EQ(rig().notify().messages().back().severity, ttk::Severity::Success);
    EXPECT_NE(rig().notify().messages().back().body.find("Bravo"), std::string::npos);
}
#endif

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

    rig().config().profile().setEngine("PrBoom");
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

TEST_F(LibraryPageTest, AFilterKeepsTheCardsItLeavesOnTheShelf) {
    load(paging::world(4));

    const std::vector<LibraryCard *> before = cards();

    rig().config().library().setFilter("ar");
    rig().turn();

    EXPECT_EQ(cards(), (std::vector<LibraryCard *>{before[2]}));
}

TEST_F(LibraryPageTest, ClearingTheFilterBringsBackTheSameCards) {
    load(paging::world(4));

    const std::vector<LibraryCard *> before = cards();

    rig().config().library().setFilter("ar");
    rig().turn();
    rig().config().library().setFilter("");
    rig().turn();

    EXPECT_EQ(cards(), before);
    EXPECT_EQ(held(), before);
}

TEST_F(LibraryPageTest, FilteringAndClearingRepaintsLikeAFullFrame) {
    load(paging::world(4));
    rig().ready();
    canvas().full();

    rig().config().library().setFilter("ar");
    rig().turn();
    canvas().run(20);

    EXPECT_TRUE(canvas().matchesFull());

    rig().config().library().setFilter("");
    rig().turn();
    canvas().run(20);

    EXPECT_TRUE(canvas().matchesFull());
}

TEST_F(LibraryPageTest, AFilteredShelfHoldsNoMoreCardsThanTheWholeOne) {
    load(paging::world(4));

    rig().config().library().setFilter("ar");
    rig().turn();

    EXPECT_EQ(held().size(), 4U);

    games();

    EXPECT_EQ(held().size(), cards().size());
}

TEST_F(LibraryPageTest, ACardTheFilterHidesTakesNoClick) {
    load(paging::world(4));
    rig().ready();

    const BLRect last = cards()[3]->box();

    rig().config().library().setFilter("ar");
    rig().turn();
    paging::click(rig().ui(), last.x + (last.w / 2.0), last.y + 40.0);

    EXPECT_EQ(cfg().profileName, "Alpha");
    EXPECT_TRUE(rig().visited.empty());
}

TEST_F(LibraryPageTest, DuplicatingKeepsTheCardsThatWereThere) {
    load(paging::world(3));

    const std::vector<LibraryCard *> before = cards();

    act(before.front(), "Duplicate");
    rig().turn();

    const std::vector<LibraryCard *> after = cards();

    ASSERT_EQ(after.size(), 4U);
    EXPECT_EQ(std::vector<LibraryCard *>(after.begin(), after.begin() + 3), before);
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

    ASSERT_EQ(rig().gameEdits.size(), 1U);

    const tests::GameEdit &asked = rig().gameEdits.front();

    EXPECT_EQ(asked.title, "Edit Plutonia");
    EXPECT_EQ(asked.game.name, "Plutonia");
    EXPECT_EQ(asked.game.file, "/games/plutonia.wad");
}

TEST_F(LibraryPageTest, AnsweringTheEditUpdatesTheGame) {
    load(paging::world(1));
    games();

    act(cards()[1], "Edit");
    rig().gameEdits.front().accepted({.name = "Hell", .file = "/games/plutonia.wad"});
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

TEST_F(LibraryPageTest, TheGamesShelfOffersAnEngineWhenThereIsNone) {
    Config made = paging::world(1);

    made.engines.clear();
    load(made);
    games();

    ASSERT_NE(addEngine(), nullptr);
    EXPECT_TRUE(paging::seen(addEngine()));
    EXPECT_FALSE(paging::seen(enginePick()));

    paging::click(rig().ui(), addEngine());

    EXPECT_EQ(rig().visited.back(), State::Page::Engines);
}

TEST_F(LibraryPageTest, TheGamesShelfPicksItsEngineWhenThereAreSome) {
    Config made = paging::world(1);

    made.general.gameEngine = "PrBoom";
    load(made);
    games();

    ASSERT_NE(enginePick(), nullptr);
    EXPECT_TRUE(paging::seen(enginePick()));
    EXPECT_FALSE(paging::seen(addEngine()));
    EXPECT_EQ(enginePick()->current(), 1);
}

TEST_F(LibraryPageTest, TheProfilesShelfHidesTheEnginePick) {
    load(paging::world(1));

    EXPECT_FALSE(paging::seen(enginePick()));
    EXPECT_FALSE(paging::seen(addEngine()));
}

TEST_F(LibraryPageTest, PickingTheEngineSetsTheLibrarysEngine) {
    load(paging::world(1));
    games();

    enginePick()->key(ttk::Key{.code = ttk::Code::Down});

    EXPECT_EQ(config().general.gameEngine, "GZDoom");

    rig().turn();

    EXPECT_EQ(enginePick()->current(), 0);
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

    canvas().run(60);

    EXPECT_TRUE(canvas().matchesFull());
}

// The one dropped is made anew and lands at rest. The ones it walked past are kept.
TEST_F(LibraryPageTest, ADropRemakesOnlyTheCardDropped) {
    load(paging::world(4));
    rig().ready();

    const std::vector<LibraryCard *> before = cards();
    const BLRect from = before[0]->box();
    const BLRect onto = before[2]->box();
    const double x = from.x + (from.w / 2.0);
    const double y = from.y + 40.0;
    const double to = onto.x + (onto.w / 2.0) + 30.0;

    rig().ui().motion(x, y);
    rig().ui().press(ttk::Pointer{.x = x, .y = y});

    for (int step = 1; step <= 10; ++step) {
        rig().ui().motion(x + ((to - x) * step / 10.0), y);
        canvas().run(1);
    }

    canvas().run(30);
    rig().ui().release(ttk::Pointer{.x = to, .y = y});
    rig().turn();

    ASSERT_NE(config().profiles.front().name, "Alpha");

    for (const LibraryCard *card : cards()) {
        const bool kept = std::ranges::find(before, card) != before.end();

        EXPECT_EQ(kept, card->title != "Alpha") << card->title;
        EXPECT_FALSE(card->hovered()) << card->title;
    }
}


TEST_F(LibraryPageTest, CardsTheFilterHidesKeepTheirArt) {
    load(paging::world(4));
    rig().ready();
    canvas().full();

    const std::vector<LibraryCard *> before = cards();

    rig().config().library().setFilter("zzz");
    rig().turn();
    canvas().run(20);

    for (const LibraryCard *card : before) {
        ASSERT_FALSE(card->visible()) << card->title;
        EXPECT_NE(card->sprites(), nullptr) << card->title;
    }
}

TEST_F(LibraryPageTest, ACardFarFromTheViewLetsGoOfItsArtAndGetsItBack) {
    load(paging::world(120));
    rig().ready();
    canvas().full();

    LibraryCard *first = cards().front();
    const BLRect shelf = paging::all<ttk::Scroll>(&page()).front()->box();
    const BLPoint at{shelf.x + (shelf.w / 2.0), shelf.y + (shelf.h / 2.0)};

    ASSERT_NE(first->sprites(), nullptr);

    for (int notch = 0; notch < 40; ++notch) {
        rig().ui().wheel(-3.0, at.x, at.y);
        canvas().run(10);
    }

    EXPECT_EQ(first->sprites(), nullptr);

    for (int notch = 0; notch < 40; ++notch) {
        rig().ui().wheel(3.0, at.x, at.y);
        canvas().run(10);
    }

    EXPECT_NE(first->sprites(), nullptr);
}

// The carried card is drawn under the pointer, so its art stays however far the shelf scrolls from its slot.
TEST_F(LibraryPageTest, ACarriedCardKeepsItsArtFarFromItsSlot) {
    load(paging::world(120));
    rig().ready();
    canvas().full();

    LibraryCard *card = cards().front();

    // Its own sprites, which no other card on the shelf could keep alive.
    card->playable = !card->playable;
    canvas().full();

    const BLRect from = card->box();
    const double x = from.x + (from.w / 2.0);
    const double y = from.y + 40.0;

    rig().ui().motion(x, y);
    rig().ui().press(ttk::Pointer{.x = x, .y = y});
    rig().ui().motion(x + 30.0, y + 30.0);
    canvas().run(2);

    for (int notch = 0; notch < 40; ++notch) {
        rig().ui().wheel(-3.0, x + 30.0, y + 30.0);
        canvas().run(10);
    }

    const std::weak_ptr<const components::CardSprites> held = card->sprites();

    ASSERT_FALSE(held.expired());

    for (int step = 0; step < 10; ++step) {
        rig().ui().motion(x + 31.0 + step, y + 30.0);
        canvas().run(1);
    }

    EXPECT_FALSE(held.expired());
    EXPECT_EQ(card->sprites(), held.lock());

    rig().ui().release(ttk::Pointer{.x = x + 41.0, .y = y + 30.0});
    rig().turn();
}

TEST_F(LibraryPageTest, AGroupedShelfDrawsABandAboveEachGroupAndClickingOneFoldsIt) {
    load(shelved());
    rig().ready();

    ASSERT_EQ(titles(), (std::vector<std::string>{"Bravo", "Delta", "Alpha", "Foxtrot", "Charlie"}));

    const BLPoint band = bandAbove(cards().front());

    EXPECT_TRUE(ruled(cards().front(), ttk::Theme::palette().border));

    paging::click(rig().ui(), band.x, band.y);
    rig().turn();
    canvas().run(20);
    rig().turn();

    EXPECT_TRUE(config().profileGroups.front().collapsed);
    EXPECT_EQ(titles(), (std::vector<std::string>{"Alpha", "Foxtrot", "Charlie"}));
}

TEST_F(LibraryPageTest, TheBandMenuRenamesTheGroup) {
    load(shelved());
    rig().ready();

    const BLRect shelf = cards().front()->parent()->box();

    paging::click(rig().ui(), shelf.x + shelf.w - ttk::Theme::bleed - 18.0, bandAbove(cards().front()).y);

    ttk::Widget *popups = rig().ui().layer(ttk::Root::POPUPS);

    ASSERT_EQ(popups->children().size(), 1U);

    const BLRect menu = popups->children().front()->box();

    paging::click(rig().ui(), menu.x + 60.0, menu.y + 21.0);

    ASSERT_EQ(rig().prompts.size(), 1U);
    EXPECT_EQ(rig().prompts.front().value, "Megawads");
}

TEST_F(LibraryPageTest, ACardCarriedIntoAnotherGroupLandsThereInPlace) {
    load(shelved());
    rig().ready();

    const LibraryCard *bravo = cards()[0];

    carry(cards()[3], BLPoint{bravo->box().x + 40.0, bravo->box().y + 40.0});
    rig().ui().release(ttk::Pointer{.x = bravo->box().x + 40.0, .y = bravo->box().y + 40.0});
    rig().turn();

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot"}));
    EXPECT_EQ(config().profiles[5].slot, (Slot{.group = 1, .place = 0}));
    EXPECT_EQ(cfg().shelfProfiles[0].name, "Foxtrot");

    canvas().run(60);

    EXPECT_TRUE(canvas().matchesFull());
}

TEST_F(LibraryPageTest, ACardDroppedOnAFoldedBandGoesToTheEndOfThatGroup) {
    canvas().resize(1280, 1300);
    load(shelved());
    rig().ready();

    const LibraryCard *alpha = cards()[2];
    const BLPoint folded{alpha->box().x + 200.0, alpha->box().y + Cards::rowHeight + ttk::Theme::gutter + 18.0};

    carry(cards()[4], folded);
    rig().ui().release(ttk::Pointer{.x = folded.x, .y = folded.y});

    // A glyph is stroked once and blitted from a mask after, a unit apart, so the pointer leaves the hovered band.
    rig().ui().motion(folded.x, folded.y + 300.0);
    rig().turn();

    EXPECT_EQ(profileNames(), (std::vector<std::string>{"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot"}));
    EXPECT_EQ(config().profiles[2].slot, (Slot{.group = 3, .place = 1}));
    EXPECT_EQ(titles(), (std::vector<std::string>{"Bravo", "Delta", "Alpha", "Foxtrot"}));

    canvas().run(60);

    EXPECT_TRUE(canvas().matchesFull());
}

TEST_F(LibraryPageTest, TheBandACarryWouldJoinLightsAndRepaintsWithoutALayout) {
    load(shelved());
    rig().ready();

    const LibraryCard *delta = cards()[1];

    carry(cards()[3], BLPoint{delta->box().x + 40.0, delta->box().y + 40.0});

    EXPECT_TRUE(ruled(cards().front(), ttk::Theme::palette().accent));
    EXPECT_TRUE(canvas().matchesFull());

    rig().ui().release(ttk::Pointer{.x = delta->box().x + 40.0, .y = delta->box().y + 40.0});
    rig().turn();
}

TEST_F(LibraryPageTest, NewGroupAsksForANameAndAddsIt) {
    load(shelved());
    games();
    rig().ready();

    ASSERT_NE(newGroup(), nullptr);
    ASSERT_TRUE(paging::seen(newGroup()));

    paging::click(rig().ui(), newGroup());

    ASSERT_EQ(rig().prompts.size(), 1U);
    EXPECT_EQ(rig().prompts.front().title, "New group");

    rig().prompts.front().accepted("Doom");

    ASSERT_EQ(config().gameGroups.size(), 1U);
    EXPECT_EQ(config().gameGroups.front().name, "Doom");
    EXPECT_EQ(config().profileGroups.size(), 3U);
}

TEST_F(LibraryPageTest, NewGroupHidesWithTheSwitchOff) {
    load(shelved(false));
    rig().ready();

    EXPECT_FALSE(paging::seen(newGroup()));
}

TEST_F(LibraryPageTest, TheAddTileSitsInTheUngroupedSection) {
    canvas().resize(1280, 1300);
    load(shelved());
    rig().ready();

    const LibraryCard *charlie = cards().back();
    const BLRect box = charlie->box();

    ASSERT_EQ(charlie->title, "Charlie");
    EXPECT_TRUE(ruled(charlie, ttk::Theme::palette().border));

    paging::click(rig().ui(), box.x + box.w + 60.0 + (box.w / 2.0), box.y + (box.h / 2.0));

    ASSERT_EQ(rig().prompts.size(), 1U);
    EXPECT_EQ(rig().prompts.front().title, "New profile");
}

TEST_F(LibraryPageTest, TheBandMenuAnswersOnlyInsideItsBox) {
    load(shelved());
    rig().ready();

    const BLRect button = menuAbove(cards().front());
    const double y = bandAbove(cards().front()).y;
    ttk::Widget *popups = rig().ui().layer(ttk::Root::POPUPS);

    paging::click(rig().ui(), button.x - 1.0, y);
    rig().turn();
    canvas().run(20);
    rig().turn();

    EXPECT_TRUE(popups->children().empty());
    EXPECT_TRUE(config().profileGroups.front().collapsed);

    paging::click(rig().ui(), button.x + 1.0, y);

    EXPECT_EQ(popups->children().size(), 1U);
}

TEST_F(LibraryPageTest, TheBandsShowThePointingHand) {
    load(shelved());
    rig().ready();

    const LibraryCard *bravo = cards().front();
    const BLRect button = menuAbove(bravo);
    const ttk::Widget *shelf = bravo->parent();

    EXPECT_EQ(shelf->cursor_at(button.x + (button.w / 2.0), bandAbove(bravo).y), ttk::Cursor::Pointer);
    EXPECT_EQ(shelf->cursor_at(bandAbove(bravo).x, bandAbove(bravo).y), ttk::Cursor::Pointer);
    EXPECT_EQ(shelf->cursor_at(bandAbove(bravo).x, bravo->box().y + Cards::rowHeight + 5.0), ttk::Cursor::Default);
}

TEST_F(LibraryPageTest, AFoldSlidesTheCardsShutThenFoldsTheGroup) {
    load(shelved());
    rig().ready();

    const LibraryCard *bravo = cards()[0];
    const LibraryCard *delta = cards()[1];
    const LibraryCard *alpha = cards()[2];
    const double was = alpha->box().y;
    const double top = bravo->box().y;
    const BLPoint band = bandAbove(bravo);

    paging::click(rig().ui(), band.x, band.y);
    rig().turn();
    canvas().run(6);

    // Mid slide the group is still open in the config, and the cards below have moved up by what it hides.
    const double hidden = was - alpha->box().y;
    const double cut = top + Cards::rowHeight + ttk::Theme::gutter - hidden;
    const int y = static_cast<int>(cut) + 3;

    EXPECT_FALSE(config().profileGroups.front().collapsed);
    EXPECT_GT(hidden, 0.0);
    EXPECT_LT(hidden, Cards::rowHeight + ttk::Theme::gutter);
    EXPECT_EQ(canvas().at(static_cast<int>(bravo->box().x + (bravo->box().w / 2.0)), y),
              canvas().at(static_cast<int>(delta->box().x + delta->box().w + 100.0), y));

    canvas().run(20);
    rig().turn();

    EXPECT_TRUE(config().profileGroups.front().collapsed);
    EXPECT_EQ(titles(), (std::vector<std::string>{"Alpha", "Foxtrot", "Charlie"}));
    EXPECT_FALSE(page().advance(canvas().now()));
    EXPECT_TRUE(canvas().matchesFull());
}

TEST_F(LibraryPageTest, ACardPutBackInItsGroupWritesNothing) {
    load(shelved());
    rig().ready();
    rig().turn(1.0);

    const std::filesystem::path file = Session::get().path();

    std::filesystem::last_write_time(file, std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));

    const auto written = std::filesystem::last_write_time(file);
    const LibraryCard *delta = cards()[1];
    const LibraryCard *alpha = cards()[2];
    const double x = delta->box().x + (delta->box().w / 2.0);
    const double y = delta->box().y + 40.0;

    rig().ui().motion(x, y);
    rig().ui().press(ttk::Pointer{.x = x, .y = y});

    for (const double to : {alpha->box().y + 40.0, y}) {
        for (int step = 1; step <= 10; ++step) {
            rig().ui().motion(x, y + ((to - y) * step / 10.0));
            canvas().run(1);
        }

        canvas().run(10);
    }

    rig().ui().release(ttk::Pointer{.x = x, .y = y});
    rig().turn(1.0);

    EXPECT_EQ(std::filesystem::last_write_time(file), written);
    EXPECT_EQ(config().profiles[3].slot.group, 1);
}

TEST_F(LibraryPageTest, TheEnginesPageIsNotGrouped) {
    const auto first = [this](const bool on) {
        load(shelved(on));
        show(State::Page::Engines);

        return paging::all<components::EngineCard>(&rig().enginesPage()).front()->box();
    };

    const BLRect flat = first(false);
    const BLRect grouped = first(true);

    EXPECT_EQ(grouped, flat);
}

}
