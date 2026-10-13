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
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/util/Format.h"

#include "Pages.h"
#include "core/ports/Catalog.h"
#include "gui/components/EngineCard.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/pages/EnginesPage.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using components::EngineCard;

class EnginesPageTest : public paging::Paged {
protected:
    pages::EnginesPage &page() { return rig().enginesPage(); }

    std::vector<EngineCard *> cards() { return paging::all<EngineCard>(&page()); }

    std::vector<std::string> names() {
        std::vector<std::string> out;

        for (const EngineCard *card : cards()) {
            out.push_back(card->name);
        }

        return out;
    }

    static std::vector<std::string> tags(const EngineCard *card) {
        std::vector<std::string> out;

        for (const State::BadgeSpec &tag : card->tags) {
            out.push_back(tag.text);
        }

        return out;
    }

    void browse() {
        State::get().nav.engines = State::EnginesTab::Browse;
        State::get().touch();
        rig().turn();
    }

    // Rows as the engines service would push them, then a turn to show them.
    void rows(const std::vector<State::EngineRow> &made) {
        State::get().ports.rows = made;
        State::get().touch();
        rig().turn();
    }

    static State::EngineRow row(const std::string &name, const State::EngineState status) {
        State::EngineRow made;

        made.name = name;
        made.status = status;

        return made;
    }

    // The tile after the last card: beside it, or first on the next row when the row is full.
    void clickAdder() {
        const BLRect first = cards().front()->box();
        const BLRect last = cards().back()->box();
        const double gap = cards().size() > 1 ? cards()[1]->box().x - (first.x + first.w) : 16.0;
        const bool beside = last.x + (2.0 * last.w) + gap <= page().box().x + page().box().w;
        const double x = beside ? last.x + last.w + gap : first.x;
        const double y = beside ? last.y : last.y + last.h + gap;

        paging::click(rig().ui(), x + (last.w / 2.0), y + (last.h / 2.0));
    }

    ttk::Button *recheck() { return paging::hinted<ttk::Button>(&page(), "GitHub limits requests per hour"); }
};

TEST_F(EnginesPageTest, ItIsNotBuiltBeforeItIsVisited) {
    load(paging::world(1));

    EXPECT_FALSE(rig().hasEnginesPage());
}

TEST_F(EnginesPageTest, AVisitBuildsAndShowsIt) {
    load(paging::world(1));
    show(State::Page::Engines);

    ASSERT_TRUE(rig().hasEnginesPage());
    EXPECT_TRUE(page().visible());
    EXPECT_FALSE(rig().library().visible());
}

TEST_F(EnginesPageTest, WithPortsItOpensOnThoseInstalled) {
    load(paging::world(1));
    show(State::Page::Engines);

    EXPECT_EQ(State::get().nav.engines, State::EnginesTab::Installed);
    EXPECT_TRUE(pages::EnginesPage::installed());
}

TEST_F(EnginesPageTest, WithNoPortsItOpensOnWhatCanBeFetched) {
    Config made = paging::world(1);

    made.ports.clear();
    load(made);
    show(State::Page::Engines);

    EXPECT_EQ(State::get().nav.engines, State::EnginesTab::Browse);
}

TEST_F(EnginesPageTest, ThereIsACardPerPort) {
    load(paging::world(1));
    show(State::Page::Engines);

    EXPECT_EQ(names(), (std::vector<std::string>{"GZDoom", "PrBoom", "Vanilla"}));
    EXPECT_EQ(cards().front()->file, "/ports/gzdoom");
}

TEST_F(EnginesPageTest, TheNoteCountsThePorts) {
    load(paging::world(1));
    show(State::Page::Engines);

    EXPECT_TRUE(paging::shows(&page(), "3 source ports · what the profiles are run with"));
}

TEST_F(EnginesPageTest, ADosPortIsTagged) {
    load(paging::world(1));
    show(State::Page::Engines);

    EXPECT_EQ(tags(cards()[2]).front(), "DOS");
}

TEST_F(EnginesPageTest, APortNotOnDiskIsMissing) {
    load(paging::world(1));
    show(State::Page::Engines);

    EXPECT_TRUE(cards().front()->missing);
    EXPECT_TRUE(cards().front()->trouble);
    EXPECT_EQ(tags(cards().front()), (std::vector<std::string>{"Missing"}));
}

TEST_F(EnginesPageTest, APortOnDiskIsNotMissing) {
    Config made = paging::world(1);
    const fs::path file = tests::Sandbox::scratch("engines-page") / "gzdoom";

    std::ofstream(file) << "x";

    made.ports.front().file = file.string();
    load(made);
    show(State::Page::Engines);

    EXPECT_FALSE(cards().front()->missing);
    EXPECT_TRUE(tags(cards().front()).empty());
}

TEST_F(EnginesPageTest, AFetchedPortIsManaged) {
    Config made = paging::world(1);

    made.ports.front().file = ttk::Format::from_path(Catalog::directory() / "gzdoom" / "gzdoom");
    load(made);
    show(State::Page::Engines);

    const std::vector<std::string> said = tags(cards().front());

    EXPECT_NE(std::ranges::find(said, "Managed"), said.end());
}

TEST_F(EnginesPageTest, EditAsksWithThePortsNameFileAndDosFlag) {
    load(paging::world(1));
    show(State::Page::Engines);

    paging::click(rig().ui(), paging::hinted<ttk::Button>(cards()[2], "Rename or point at another file"));

    ASSERT_EQ(rig().edits.size(), 1U);

    const tests::Edit &asked = rig().edits.front();

    EXPECT_EQ(asked.title, "Edit Vanilla");
    EXPECT_EQ(asked.kind, dialogs::EntryDialog::Kind::Port);
    EXPECT_EQ(asked.filters, Filters::port());
    EXPECT_EQ(asked.remember, LastDir::SRC);
    EXPECT_EQ(asked.name, "Vanilla");
    EXPECT_EQ(asked.file, "/ports/doom2.exe");
    EXPECT_TRUE(asked.offerDos);
    EXPECT_TRUE(asked.dosbox);
}

TEST_F(EnginesPageTest, OpeningACardEditsIt) {
    load(paging::world(1));
    show(State::Page::Engines);

    cards()[1]->opened();

    ASSERT_EQ(rig().edits.size(), 1U);
    EXPECT_EQ(rig().edits.front().name, "PrBoom");
}

TEST_F(EnginesPageTest, AnsweringTheEditUpdatesTheCard) {
    load(paging::world(1));
    show(State::Page::Engines);

    cards()[0]->opened();
    rig().edits.front().accepted("Zandronum", "/ports/zandronum", true);
    rig().turn();

    EXPECT_EQ(cards().front()->name, "Zandronum");
    EXPECT_EQ(cards().front()->file, "/ports/zandronum");
    EXPECT_EQ(tags(cards().front()).front(), "DOS");
}

TEST_F(EnginesPageTest, RemovingAPortAsksFirst) {
    load(paging::world(1));
    show(State::Page::Engines);

    paging::click(rig().ui(), paging::hinted<ttk::GlyphButton>(cards()[1], "Remove from the list"));

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Remove \"PrBoom\"?");
    EXPECT_TRUE(rig().asks.front().body.contains("The file itself is left where it is."));
    EXPECT_TRUE(rig().asks.front().danger);
    EXPECT_EQ(config().ports.size(), 3U);
}

TEST_F(EnginesPageTest, AnsweringRemoveTakesTheCardAway) {
    load(paging::world(1));
    show(State::Page::Engines);

    paging::click(rig().ui(), paging::hinted<ttk::GlyphButton>(cards()[1], "Remove from the list"));
    rig().asks.front().accepted();
    rig().turn();

    EXPECT_EQ(names(), (std::vector<std::string>{"GZDoom", "Vanilla"}));
}

TEST_F(EnginesPageTest, AFetchedPortWarnsItsFilesGoToo) {
    Config made = paging::world(1);

    made.ports.front().file = ttk::Format::from_path(Catalog::directory() / "gzdoom" / "gzdoom");
    load(made);
    show(State::Page::Engines);

    paging::click(rig().ui(), paging::hinted<ttk::GlyphButton>(cards()[0], "Delete what was fetched"));

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_TRUE(rig().asks.front().body.starts_with("Everything ZDL4 unpacked is deleted"));
}

TEST_F(EnginesPageTest, AChangeElsewhereKeepsTheCards) {
    load(paging::world(1));
    show(State::Page::Engines);

    const std::vector<EngineCard *> before = cards();

    rig().config().profile().setSkill(3);
    rig().turn();

    EXPECT_EQ(cards(), before);
}

TEST_F(EnginesPageTest, AnAddedPortGetsACard) {
    load(paging::world(1));
    show(State::Page::Engines);

    std::ignore = rig().config().lists().addPort("/ports/woof", "Woof", false);
    rig().turn();

    EXPECT_EQ(names().back(), "Woof");
}

TEST_F(EnginesPageTest, TheAddTileOpensThePickerWithADosChoice) {
    load(paging::world(1));
    show(State::Page::Engines);
    rig().ready();

    clickAdder();

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Add a source port");
    EXPECT_EQ(rig().files().state().option, "DOS program");
}

TEST_F(EnginesPageTest, APortPickedIsAdded) {
    load(paging::world(1));
    show(State::Page::Engines);
    rig().ready();

    clickAdder();
    rig().files().choose({"/opt/ports/woof"});
    rig().turn();

    EXPECT_EQ(names().back(), "woof");
}

TEST_F(EnginesPageTest, TheSwitchMovesToWhatCanBeFetched) {
    load(paging::world(1));
    show(State::Page::Engines);

    const std::vector<ttk::MultistateSwitch *> switches = paging::all<ttk::MultistateSwitch>(&page());

    ASSERT_EQ(switches.size(), 1U);

    switches.front()->key(ttk::Key{.code = ttk::Code::Right});
    rig().turn();

    EXPECT_EQ(State::get().nav.engines, State::EnginesTab::Browse);
    EXPECT_EQ(switches.front()->current(), static_cast<int>(State::EnginesTab::Browse));
}

TEST_F(EnginesPageTest, CheckingAgainIsOfferedOnlyWhenBrowsing) {
    load(paging::world(1));
    show(State::Page::Engines);

    ASSERT_NE(recheck(), nullptr);
    EXPECT_FALSE(paging::seen(recheck()));

    browse();

    EXPECT_TRUE(paging::seen(recheck()));
}

TEST_F(EnginesPageTest, BrowsingShowsARowPerPortThatCanBeFetched) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    rows({row("GZDoom", State::EngineState::Ready), row("Woof", State::EngineState::Ready)});

    EXPECT_EQ(names(), (std::vector<std::string>{"GZDoom", "Woof"}));
}

TEST_F(EnginesPageTest, BrowsingSaysWhereThePortsAreKept) {
    load(paging::world(1));
    show(State::Page::Engines);

    State::get().ports.directory = "/data/source_ports";
    browse();

    rows({row("GZDoom", State::EngineState::Ready)});

    EXPECT_TRUE(paging::shows(&page(), "Where they are kept"));
}

TEST_F(EnginesPageTest, AReadyRowOffersAnInstall) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::EngineRow ready = row("GZDoom", State::EngineState::Ready);

    ready.version = "4.14.2";
    ready.sizeText = "24 MB";
    rows({ready});

    EXPECT_TRUE(tags(cards().front()).empty());
    EXPECT_EQ(cards().front()->told, "4.14.2 · 24 MB");
    EXPECT_NE(paging::hinted<ttk::Button>(cards().front(), "Fetch it and add it"), nullptr);
}

TEST_F(EnginesPageTest, AnInstalledRowOffersToFetchAgain) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::EngineRow present = row("GZDoom", State::EngineState::Installed);

    present.version = "4.14.2";
    present.have = "4.14.2";
    rows({present});

    EXPECT_EQ(tags(cards().front()), (std::vector<std::string>{"Installed"}));
    EXPECT_EQ(cards().front()->told, "4.14.2 here");
    EXPECT_NE(paging::hinted<ttk::Button>(cards().front(), "Fetch the latest build"), nullptr);
    EXPECT_NE(paging::hinted<ttk::GlyphButton>(cards().front(), "Delete what was fetched"), nullptr);
}

TEST_F(EnginesPageTest, AnInstalledRowBehindTheReleaseOffersAnUpdate) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::EngineRow behind = row("GZDoom", State::EngineState::Installed);

    behind.version = "4.14.2";
    behind.have = "4.13.0";
    rows({behind});

    ASSERT_EQ(cards().front()->tags.size(), 1U);
    EXPECT_EQ(cards().front()->tags.front().text, "Update");
    EXPECT_EQ(cards().front()->tags.front().kind, State::BadgeKind::Success);
    EXPECT_EQ(cards().front()->told, "4.13.0 here · 4.14.2 released");
}

TEST_F(EnginesPageTest, AFailedRowSaysWhyInRed) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::EngineRow failed = row("GZDoom", State::EngineState::Failed);

    failed.error = "The archive was damaged.";
    rows({failed});

    EXPECT_EQ(cards().front()->tags.front().text, "Failed");
    EXPECT_EQ(cards().front()->tags.front().kind, State::BadgeKind::Danger);
    EXPECT_EQ(cards().front()->told, "The archive was damaged.");
    EXPECT_TRUE(cards().front()->trouble);
}

TEST_F(EnginesPageTest, ARowFetchedElsewhereHasNoInstall) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    rows({row("Doom Legacy", State::EngineState::Elsewhere)});

    EXPECT_EQ(tags(cards().front()), (std::vector<std::string>{"Its own site"}));
    EXPECT_EQ(cards().front()->told, "Fetched from its own site");
    EXPECT_EQ(paging::hinted<ttk::Button>(cards().front(), "Fetch"), nullptr);
}

TEST_F(EnginesPageTest, ARowWithNothingToFetchIsAWarning) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    rows({row("Odamex", State::EngineState::Unavailable)});

    EXPECT_EQ(cards().front()->tags.front().text, "No downloads available");
    EXPECT_EQ(cards().front()->tags.front().kind, State::BadgeKind::Warning);
}

TEST_F(EnginesPageTest, ARowBeingAskedAboutSaysSo) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    rows({row("GZDoom", State::EngineState::Checking)});

    EXPECT_EQ(cards().front()->tags.front().text, "Checking");
}

TEST_F(EnginesPageTest, AFetchShowsItsProgressAndCanBeStopped) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::EngineRow fetching = row("GZDoom", State::EngineState::Fetching);

    fetching.progress = 0.25F;
    rows({fetching});

    EXPECT_TRUE(cards().front()->working);
    EXPECT_DOUBLE_EQ(cards().front()->progress, 0.25);
    EXPECT_TRUE(cards().front()->tags.empty());
    EXPECT_EQ(paging::hinted<ttk::Button>(cards().front(), "Fetch"), nullptr);
    EXPECT_EQ(paging::all<ttk::Button>(cards().front()).size(), 1U);
}

TEST_F(EnginesPageTest, UnpackingIsShownAsAFullBar) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    rows({row("GZDoom", State::EngineState::Unpacking)});

    EXPECT_DOUBLE_EQ(cards().front()->progress, 1.0);
}

TEST_F(EnginesPageTest, ProgressMovesTheBarWithoutARebuild) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::EngineRow fetching = row("GZDoom", State::EngineState::Fetching);

    fetching.progress = 0.25F;
    rows({fetching});

    const std::vector<EngineCard *> before = cards();

    fetching.progress = 0.75F;
    rows({fetching});

    EXPECT_EQ(cards(), before);
    EXPECT_DOUBLE_EQ(cards().front()->progress, 0.75);
}

TEST_F(EnginesPageTest, TroubleFetchingTheListIsTheNote) {
    load(paging::world(1));
    show(State::Page::Engines);

    State::get().ports.trouble = "GitHub could not be reached.";
    browse();

    EXPECT_TRUE(paging::shows(&page(), "GitHub could not be reached."));
}

TEST_F(EnginesPageTest, ADosRowIsTagged) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::EngineRow dos = row("Doom", State::EngineState::Ready);

    dos.dos = true;
    rows({dos});

    EXPECT_EQ(tags(cards().front()), (std::vector<std::string>{"DOS"}));
}

}
