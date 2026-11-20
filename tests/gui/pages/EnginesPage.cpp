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
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/notices/Notice.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/util/Format.h"

#include "Pages.h"
#include "core/launch/KnownPorts.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "gui/components/EngineCard.h"
#include "gui/dialogs/EntryDialog.h"
#include "gui/pages/EnginesPage.h"
#include "gui/services/Filters.h"
#include "gui/state/State.h"
#include "support/FlatpakApp.h"
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

        out.reserve(card->tags.size());

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

    // Rows as the downloads service would push them, then a turn to show them.
    void rows(const std::vector<State::DownloadRow> &made) {
        State::get().downloads.rows = made;
        State::get().touch();
        rig().turn();
    }

    static State::DownloadRow row(const std::string &name, const State::DownloadStatus status) {
        State::DownloadRow made;

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

    ttk::Label *findLink() { return paging::hinted<ttk::Label>(&page(), "Finds engines installed on this machine"); }

    // Turns until done holds, or ten seconds pass.
    bool settles(const std::function<bool()> &done) {
        for (int at = 0; at < 2000 && !done(); ++at) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            rig().turn();
        }

        return done();
    }

    // Turns until a find has ended.
    bool found() {
        return settles([] { return !State::get().downloads.finding; });
    }

    // Every port detection finds on this machine, listed by hand, so that nothing found is new.
    static void listEverythingFound(Config &made) {
        for (const Detect::Found &each : Detect::ports()) {
            made.engines.push_back({.name = each.name, .file = ttk::Format::from_path(each.file)});
        }
    }

    [[nodiscard]] bool noticed(const std::string &body) {
        return std::ranges::any_of(rig().notify().messages(),
                                   [&body](const ttk::Notice &each) { return each.body == body; });
    }
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

TEST_F(EnginesPageTest, WithEnginesItOpensOnThoseInstalled) {
    load(paging::world(1));
    show(State::Page::Engines);

    EXPECT_EQ(State::get().nav.engines, State::EnginesTab::Installed);
    EXPECT_TRUE(pages::EnginesPage::installed());
}

TEST_F(EnginesPageTest, WithNoEnginesItOpensOnWhatCanBeFetched) {
    Config made = paging::world(1);

    made.engines.clear();
    load(made);
    show(State::Page::Engines);

    EXPECT_EQ(State::get().nav.engines, State::EnginesTab::Browse);
}

TEST_F(EnginesPageTest, ThereIsACardPerEngine) {
    load(paging::world(1));
    show(State::Page::Engines);

    EXPECT_EQ(names(), (std::vector<std::string>{"GZDoom", "PrBoom", "Vanilla"}));
    EXPECT_EQ(cards().front()->file, "/ports/gzdoom");
}

TEST_F(EnginesPageTest, TheNoteCountsTheEngines) {
    load(paging::world(1));
    show(State::Page::Engines);

    EXPECT_TRUE(paging::shows(&page(), "3 source ports · what the profiles are run with"));
}

TEST_F(EnginesPageTest, ADosEngineIsTagged) {
    load(paging::world(1));
    show(State::Page::Engines);

    EXPECT_EQ(tags(cards()[2]).front(), "DOS");
}

TEST_F(EnginesPageTest, TagsRunFromTheLeastImportantToTheMost) {
    Config made = paging::world(1);
    const fs::path file = tests::Sandbox::scratch("engines-order") / "crispy-doom";

    std::ofstream(file) << "x";

    made.engines.push_back(paging::engine("Crispy", "/ports/crispy-doom", true));
    made.engines.back().managed = true;
    made.engines.push_back(paging::engine("Found", file.string(), true));
    made.engines.back().managed = true;
    made.engines.back().detected = true;
    load(made);
    show(State::Page::Engines);

    ASSERT_EQ(cards().size(), 5U);
    EXPECT_EQ(tags(cards()[3]), (std::vector<std::string>{"Managed", "DOS", "Missing"}));
    EXPECT_EQ(tags(cards()[4]), (std::vector<std::string>{"Detected", "Managed", "DOS"}));
    EXPECT_EQ(cards()[4]->tagHints, (std::vector<std::string>{"", "Downloaded and updated by ZDL4", ""}));
}

TEST_F(EnginesPageTest, AnEngineWithReleasesAndNoVersionAsksForOne) {
    Config made = paging::world(1);

    made.engines.push_back(paging::engine("DSDA", "/ports/dsda-doom"));
    made.engines.push_back(paging::engine("LZDoom", "/ports/lzdoom"));
    load(made);
    show(State::Page::Engines);

    ASSERT_EQ(cards().size(), 5U);
    EXPECT_EQ(tags(cards()[3]), (std::vector<std::string>{"Pick a version", "Missing"}));
    EXPECT_EQ(cards()[3]->tags.front().kind, State::BadgeKind::Warning);
    EXPECT_FALSE(cards()[3]->tagHints.front().empty());
    EXPECT_EQ(tags(cards()[0]), (std::vector<std::string>{"Pick a version", "Missing"}));
    EXPECT_EQ(tags(cards()[1]), (std::vector<std::string>{"Pick a version", "Missing"}));
    EXPECT_EQ(std::ranges::count(tags(cards()[4]), "Pick a version"), 0);
}

TEST_F(EnginesPageTest, APickedVersionTakesTheBadgeAway) {
    Config made = paging::world(1);

    made.engines.push_back(paging::engine("DSDA", "/ports/dsda-doom"));
    load(made);
    show(State::Page::Engines);

    ASSERT_EQ(std::ranges::count(tags(cards()[3]), "Pick a version"), 1);

    rig().config().lists().updateEngine(3, {.name = "DSDA", .file = "/ports/dsda-doom", .version = "0.28.0"});
    rig().turn();

    EXPECT_EQ(std::ranges::count(tags(cards()[3]), "Pick a version"), 0);
}

TEST_F(EnginesPageTest, ADownloadedEngineCarriesNoBadgeToPickAVersion) {
    Config made = paging::world(1);
    Engine fetched = paging::engine("DSDA", "/ports/dsda-doom");

    fetched.port = Port::Port::dsda;
    fetched.managed = true;
    made.engines.push_back(fetched);
    load(made);
    show(State::Page::Engines);

    EXPECT_EQ(std::ranges::count(tags(cards()[3]), "Pick a version"), 0);
}

TEST_F(EnginesPageTest, ADosboxEngineCarriesNoBadgeToPickAVersion) {
    Config made = paging::world(1);

    made.engines.push_back(paging::engine("DSDA", "/ports/dsda-doom", true));
    load(made);
    show(State::Page::Engines);

    EXPECT_EQ(std::ranges::count(tags(cards()[3]), "Pick a version"), 0);
}

TEST_F(EnginesPageTest, AnEngineNotOnDiskIsMissing) {
    Config made = paging::world(1);

    made.engines.front().version = "4.12.1";
    load(made);
    show(State::Page::Engines);

    EXPECT_TRUE(cards().front()->missing);
    EXPECT_TRUE(cards().front()->trouble);
    EXPECT_EQ(tags(cards().front()), (std::vector<std::string>{"Missing"}));
}

TEST_F(EnginesPageTest, AnEngineOnDiskIsNotMissing) {
    Config made = paging::world(1);
    const fs::path file = tests::Sandbox::scratch("engines-page") / "gzdoom";

    std::ofstream(file) << "x";

    made.engines.front().file = file.string();
    made.engines.front().version = "4.12.1";
    load(made);
    show(State::Page::Engines);

    EXPECT_FALSE(cards().front()->missing);
    EXPECT_TRUE(tags(cards().front()).empty());
}

TEST_F(EnginesPageTest, AFetchedEngineIsManaged) {
    Config made = paging::world(1);

    made.engines.front().managed = true;
    load(made);
    show(State::Page::Engines);

    const std::vector<std::string> said = tags(cards().front());

    EXPECT_NE(std::ranges::find(said, "Managed"), said.end());
}

TEST_F(EnginesPageTest, ADetectedEngineIsTaggedDetected) {
    Config made = paging::world(1);
    const fs::path file = tests::Sandbox::scratch("engines-detected") / "gzdoom";

    std::ofstream(file) << "x";

    made.engines.front().file = file.string();
    made.engines.front().version = "4.12.1";
    made.engines.front().detected = true;
    load(made);
    show(State::Page::Engines);

    EXPECT_EQ(tags(cards().front()), (std::vector<std::string>{"Detected"}));
}

#ifdef __linux__
TEST_F(EnginesPageTest, AFlatpakEngineIsTaggedFlatpak) {
    Config made = paging::world(1);

    made.engines.front().file = "/var/lib/flatpak/exports/bin/org.zdoom.UZDoom";
    made.engines.front().version = "4.14.3";
    load(made);
    show(State::Page::Engines);

    const std::vector<std::string> flatpak = tags(cards().front());
    const std::vector<std::string> other = tags(cards()[1]);

    const auto pill = std::ranges::find(flatpak, "Flatpak");

    ASSERT_NE(pill, flatpak.end());
    EXPECT_EQ(cards().front()->tagHints.at(static_cast<size_t>(pill - flatpak.begin())), "Runs through flatpak run");
    EXPECT_EQ(std::ranges::find(other, "Flatpak"), other.end());
}

TEST_F(EnginesPageTest, OpeningAConfigStoresTheReleaseItsFlatpakInstalled) {
    const fs::path scratch = tests::Sandbox::scratch("engines-flatpak-open");
    const tests::FlatpakApp deployed = tests::deployFlatpak(scratch / "flatpak", "org.zdoom.UZDoom");
    const std::string file = ttk::Format::from_path(deployed.exported);
    Config other = paging::world(1);

    tests::nameRelease(deployed, "5.0.3");
    other.engines.front().file = file;
    other.engines.front().version = "4.14.3";
    ASSERT_TRUE(other.save(scratch / "zdl.json"));

    load(paging::world(1));
    show(State::Page::Engines);
    ASSERT_TRUE(found());

    rig().config().settings().load((scratch / "zdl.json").string());

    const auto stored = [&file] {
        const auto engine = std::ranges::find(config().engines, file, &Engine::file);

        return engine != config().engines.end() && engine->version == "5.0.3";
    };

    EXPECT_TRUE(settles(stored));
}

TEST_F(EnginesPageTest, TheFindLinkStoresTheReleaseOfAnUpdatedFlatpak) {
    const fs::path root = tests::Sandbox::scratch("engines-flatpak-find") / "flatpak";
    const tests::FlatpakApp deployed = tests::deployFlatpak(root, "org.zdoom.UZDoom");
    const std::string file = ttk::Format::from_path(deployed.exported);
    Config made = paging::world(1);

    tests::nameRelease(deployed, "4.14.3");
    made.engines.front().file = file;
    made.engines.front().version = "4.14.3";
    load(std::move(made));
    show(State::Page::Engines);
    ASSERT_TRUE(found());

    tests::nameRelease(tests::deployFlatpak(root, "org.zdoom.UZDoom", "stable", "4567cdef"), "5.0.3");
    paging::click(rig().ui(), findLink());

    ASSERT_TRUE(found());

    const auto engine = std::ranges::find(config().engines, file, &Engine::file);

    ASSERT_NE(engine, config().engines.end());
    EXPECT_EQ(engine->version, "5.0.3");
}
#endif

TEST_F(EnginesPageTest, TheFindLinkIsOnTheInstalledShelfOnly) {
    load(paging::world(1));
    show(State::Page::Engines);

    ASSERT_NE(findLink(), nullptr);
    EXPECT_EQ(findLink()->text(), "Detect engines");
    EXPECT_TRUE(findLink()->visible());

    browse();

    EXPECT_FALSE(findLink()->visible());
}

TEST_F(EnginesPageTest, TheFindLinkAddsAPortTheListLacks) {
    const fs::path file = tests::Sandbox::root() / "bin" / "helion";
    Config made = paging::world(1);

    listEverythingFound(made);
    load(std::move(made));
    show(State::Page::Engines);
    ASSERT_TRUE(found());

    fs::create_directories(file.parent_path());
    std::ofstream(file) << "#!/bin/sh\n";
    fs::permissions(file, fs::perms::owner_all);

    const size_t before = config().engines.size();

    paging::click(rig().ui(), findLink());

    EXPECT_TRUE(State::get().downloads.finding);
    ASSERT_TRUE(found());

    fs::remove(file);

    const auto added = std::ranges::find(config().engines, ttk::Format::from_path(file), &Engine::file);

    ASSERT_NE(added, config().engines.end());
    EXPECT_TRUE(added->detected);
    EXPECT_EQ(config().engines.size(), before + 1);
    EXPECT_EQ(std::ranges::count(names(), added->name), 1);

    paging::click(rig().ui(), findLink());

    ASSERT_TRUE(found());
    EXPECT_EQ(config().engines.size(), before + 1);
    EXPECT_TRUE(noticed("No other engines were found on this machine."));
}

TEST_F(EnginesPageTest, AManagedEngineHasNoEditAndOpensNothing) {
    Config made = paging::world(1);

    made.engines[1].managed = true;
    load(std::move(made));
    show(State::Page::Engines);

    EXPECT_EQ(paging::hinted<ttk::Button>(cards()[1], "Rename or point at another file"), nullptr);
    EXPECT_FALSE(cards()[1]->opened);
    EXPECT_NE(paging::hinted<ttk::GlyphButton>(cards()[1], "Show in file explorer"), nullptr);

    const BLRect box = cards()[1]->box();

    paging::click(rig().ui(), box.x + (box.w / 2.0), box.y + 8.0);

    EXPECT_TRUE(rig().engineEdits.empty());
}

TEST_F(EnginesPageTest, AnEngineAddedByHandKeepsEditAndOpens) {
    load(paging::world(1));
    show(State::Page::Engines);

    ASSERT_NE(paging::hinted<ttk::Button>(cards()[1], "Rename or point at another file"), nullptr);
    ASSERT_TRUE(cards()[1]->opened);

    const BLRect box = cards()[1]->box();

    paging::click(rig().ui(), box.x + (box.w / 2.0), box.y + 8.0);

    ASSERT_EQ(rig().engineEdits.size(), 1U);
    EXPECT_EQ(rig().engineEdits.front().engine.name, config().engines[1].name);

    paging::click(rig().ui(), paging::hinted<ttk::Button>(cards()[1], "Rename or point at another file"));

    EXPECT_EQ(rig().engineEdits.size(), 2U);
}

TEST_F(EnginesPageTest, AnEngineThatBecomesManagedLosesEdit) {
    load(paging::world(1));
    show(State::Page::Engines);

    ASSERT_NE(paging::hinted<ttk::Button>(cards()[1], "Rename or point at another file"), nullptr);

    // As the downloads service marks a row it downloads.
    config().engines[1].managed = true;
    rig().config().lists().push();
    rig().turn();

    const std::vector<std::string> said = tags(cards()[1]);

    EXPECT_EQ(paging::hinted<ttk::Button>(cards()[1], "Rename or point at another file"), nullptr);
    EXPECT_NE(std::ranges::find(said, "Managed"), said.end());
}

TEST_F(EnginesPageTest, AFileInThePortsFolderKeepsEditAndNoManagedBadge) {
    Config made = paging::world(1);

    made.engines[1].file = ttk::Format::from_path(Catalog::directory() / "dsda-doom" / "dsda-doom");
    load(std::move(made));
    show(State::Page::Engines);

    const std::vector<std::string> said = tags(cards()[1]);

    EXPECT_NE(paging::hinted<ttk::Button>(cards()[1], "Rename or point at another file"), nullptr);
    EXPECT_TRUE(cards()[1]->opened);
    EXPECT_EQ(std::ranges::find(said, "Managed"), said.end());
}

TEST_F(EnginesPageTest, EditAsksWithTheNameFileAndDosFlagOfTheEngine) {
    load(paging::world(1));
    show(State::Page::Engines);

    paging::click(rig().ui(), paging::hinted<ttk::Button>(cards()[2], "Rename or point at another file"));

    ASSERT_EQ(rig().engineEdits.size(), 1U);

    const tests::EngineEdit &asked = rig().engineEdits.front();

    EXPECT_EQ(asked.title, "Edit Vanilla");
    EXPECT_EQ(asked.engine.name, "Vanilla");
    EXPECT_EQ(asked.engine.file, "/ports/doom2.exe");
    EXPECT_TRUE(asked.engine.dosbox);
}

TEST_F(EnginesPageTest, EditAsksWithAStoredPort) {
    Config world = paging::world(1);
    world.engines[1].port = Port::Port::dsda;
    load(std::move(world));
    show(State::Page::Engines);

    cards()[1]->opened();

    ASSERT_EQ(rig().engineEdits.size(), 1U);
    EXPECT_EQ(rig().engineEdits.front().engine.port, Port::Port::dsda);
}

TEST_F(EnginesPageTest, AFetchRepointingAnEngineKeepsItsPort) {
    Config world = paging::world(1);
    world.engines[1].port = Port::Port::dsda;
    load(std::move(world));

    rig().downloads().updateEngine(1, "PrBoom", "/shelf/prboom-plus", false);

    EXPECT_EQ(config().engines[1].file, "/shelf/prboom-plus");
    EXPECT_EQ(config().engines[1].port, Port::Port::dsda);
}

TEST_F(EnginesPageTest, AFetchRepointingAnEngineKeepsItsVersionAndManagedFlag) {
    Config world = paging::world(1);
    world.engines[1].port = Port::Port::dsda;
    world.engines[1].version = "0.27.0";
    world.engines[1].managed = true;
    load(std::move(world));

    rig().downloads().updateEngine(1, "PrBoom", "/shelf/dsda-doom", true);

    EXPECT_EQ(config().engines[1].file, "/shelf/dsda-doom");
    EXPECT_TRUE(config().engines[1].dosbox);
    EXPECT_EQ(config().engines[1].port, Port::Port::dsda);
    EXPECT_EQ(config().engines[1].version, "0.27.0");
    EXPECT_TRUE(config().engines[1].managed);
}

TEST_F(EnginesPageTest, EditAsksWithAStoredVersion) {
    Config world = paging::world(1);
    world.engines[1].version = "0.27.0";
    load(std::move(world));
    show(State::Page::Engines);

    cards()[1]->opened();

    ASSERT_EQ(rig().engineEdits.size(), 1U);
    EXPECT_EQ(rig().engineEdits.front().engine.version, "0.27.0");
    EXPECT_FALSE(rig().engineEdits.front().adding);
}

TEST_F(EnginesPageTest, AnAnsweredEditKeepsTheVersion) {
    load(paging::world(1));
    show(State::Page::Engines);

    cards()[1]->opened();
    rig().engineEdits.front().accepted({.name = "PrBoom", .file = "/ports/prboom", .version = "0.26.0"});
    rig().turn();

    EXPECT_EQ(Session::get().config().engines[1].version, "0.26.0");
}

TEST_F(EnginesPageTest, OpeningACardEditsIt) {
    load(paging::world(1));
    show(State::Page::Engines);

    cards()[1]->opened();

    ASSERT_EQ(rig().engineEdits.size(), 1U);
    EXPECT_EQ(rig().engineEdits.front().engine.name, "PrBoom");
}

TEST_F(EnginesPageTest, AnsweringTheEditUpdatesTheCard) {
    load(paging::world(1));
    show(State::Page::Engines);

    cards()[0]->opened();
    rig().engineEdits.front().accepted(
            {.name = "Zandronum", .file = "/ports/zandronum", .dosbox = true, .port = Port::Port::doom});
    rig().turn();

    EXPECT_EQ(cards().front()->name, "Zandronum");
    EXPECT_EQ(cards().front()->file, "/ports/zandronum");
    EXPECT_EQ(tags(cards().front()).front(), "DOS");
    EXPECT_EQ(Session::get().config().engines.front().port, Port::Port::doom);
}

TEST_F(EnginesPageTest, RemovingAnEngineAsksFirst) {
    load(paging::world(1));
    show(State::Page::Engines);

    paging::click(rig().ui(), paging::hinted<ttk::GlyphButton>(cards()[1], "Remove from the list"));

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Remove \"PrBoom\"?");
    EXPECT_TRUE(rig().asks.front().body.contains("The file itself is left where it is."));
    EXPECT_TRUE(rig().asks.front().danger);
    EXPECT_EQ(config().engines.size(), 3U);
}

TEST_F(EnginesPageTest, AnsweringRemoveTakesTheCardAway) {
    load(paging::world(1));
    show(State::Page::Engines);

    paging::click(rig().ui(), paging::hinted<ttk::GlyphButton>(cards()[1], "Remove from the list"));
    rig().asks.front().accepted();
    rig().turn();

    EXPECT_EQ(names(), (std::vector<std::string>{"GZDoom", "Vanilla"}));
}

TEST_F(EnginesPageTest, AFetchedEngineWarnsItsFilesGoToo) {
    Config made = paging::world(1);

    made.engines.front().managed = true;
    load(made);
    show(State::Page::Engines);

    paging::click(rig().ui(), paging::hinted<ttk::GlyphButton>(cards()[0], "Delete what was fetched"));

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_TRUE(rig().asks.front().body.starts_with("Everything ZDL4 unpacked is deleted"));
}

TEST_F(EnginesPageTest, AnEngineAddedByHandInsideThePortsFolderOnlyLeavesTheList) {
    Config made = paging::world(1);

    made.engines.front().file = ttk::Format::from_path(Catalog::directory() / "gzdoom" / "gzdoom");
    load(made);
    show(State::Page::Engines);

    paging::click(rig().ui(), paging::hinted<ttk::GlyphButton>(cards()[0], "Remove from the list"));

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_TRUE(rig().asks.front().body.starts_with("It goes out of this list"));
}

TEST_F(EnginesPageTest, AChangeElsewhereKeepsTheCards) {
    load(paging::world(1));
    show(State::Page::Engines);

    const std::vector<EngineCard *> before = cards();

    rig().config().profile().setSkill(3);
    rig().turn();

    EXPECT_EQ(cards(), before);
}

TEST_F(EnginesPageTest, AnAddedEngineGetsACard) {
    load(paging::world(1));
    show(State::Page::Engines);

    std::ignore = rig().config().lists().addEngine({.name = "Woof", .file = "/ports/woof"});
    rig().turn();

    EXPECT_EQ(names().back(), "Woof");
}

TEST_F(EnginesPageTest, TheAddTileAsksForAnEmptyEngine) {
    load(paging::world(1));
    show(State::Page::Engines);
    rig().ready();

    clickAdder();

    ASSERT_EQ(rig().engineEdits.size(), 1U);
    EXPECT_EQ(rig().engineEdits.front().title, "Add engine");
    EXPECT_TRUE(rig().engineEdits.front().adding);
    EXPECT_EQ(rig().engineEdits.front().engine.file, "");
    EXPECT_EQ(rig().engineEdits.front().engine.name, "");
}

TEST_F(EnginesPageTest, AProgramDroppedOnTheShelfOpensTheAddDialogFilledIn) {
    load(paging::world(1));
    show(State::Page::Engines);
    rig().ready();

    const BLRect shelf = paging::all<ttk::Scroll>(&page()).front()->box();
    const double x = shelf.x + (shelf.w / 2.0);
    const double y = shelf.y + (shelf.h / 2.0);

    rig().ui().drag_files(x, y);

    EXPECT_EQ(rig().ui().drag_target(), &page());
    // Only an .exe is a program on Windows.
#ifdef _WIN32
    const std::string woof = "C:/ports/woof.exe";
    const std::string dsda = "C:/ports/dsda-doom.exe";
#else
    const std::string woof = "/opt/ports/woof";
    const std::string dsda = "/opt/ports/dsda-doom";
#endif

    EXPECT_TRUE(rig().ui().drop_files({woof, dsda}, x, y));

    ASSERT_EQ(rig().engineEdits.size(), 1U);
    EXPECT_EQ(rig().engineEdits.front().title, "Add engine");
    EXPECT_TRUE(rig().engineEdits.front().adding);
    EXPECT_EQ(rig().engineEdits.front().engine.file, woof);
    EXPECT_FALSE(rig().engineEdits.front().engine.name.empty());
    EXPECT_FALSE(rig().engineEdits.front().engine.dosbox);
    ASSERT_FALSE(rig().notify().messages().empty());
    EXPECT_EQ(rig().notify().messages().back().body,
              "One engine is added at a time, so " + fs::path(dsda).filename().string() + " was left out.");
}

TEST_F(EnginesPageTest, FilesDroppedOffTheShelfAreRefused) {
    load(paging::world(1));
    show(State::Page::Engines);
    rig().ready();

    // The tab switch above the shelf takes the pointer, so the drop reaches the page through it.
    const BLRect tabs = paging::all<ttk::MultistateSwitch>(&page()).front()->box();
    const double x = tabs.x + (tabs.w / 2.0);
    const double y = tabs.y + (tabs.h / 2.0);

    rig().ui().motion(x, y);

    ASSERT_NE(rig().ui().hovered(), nullptr);

    rig().ui().drag_files(x, y);

    EXPECT_EQ(rig().ui().drag_target(), nullptr);
    EXPECT_FALSE(rig().ui().drop_files({"/opt/ports/woof"}, x, y));
    EXPECT_TRUE(rig().engineEdits.empty());
}

TEST_F(EnginesPageTest, AFolderDroppedOnTheShelfIsLeftOut) {
    load(paging::world(1));
    show(State::Page::Engines);
    rig().ready();

    const BLRect shelf = paging::all<ttk::Scroll>(&page()).front()->box();
    const fs::path folder = tests::Sandbox::scratch("engines-drop");

    EXPECT_TRUE(rig().ui().drop_files({folder.string()}, shelf.x + 20.0, shelf.y + 20.0));
    EXPECT_TRUE(rig().engineEdits.empty());
    ASSERT_FALSE(rig().notify().messages().empty());
    EXPECT_EQ(rig().notify().messages().back().body, "engines-drop is not a program, so it was left out.");
}

TEST_F(EnginesPageTest, TheShelfOfDownloadsTakesNoFiles) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    const BLRect shelf = paging::all<ttk::Scroll>(&page()).front()->box();

    EXPECT_FALSE(rig().ui().drop_files({"/opt/ports/woof"}, shelf.x + 20.0, shelf.y + 20.0));
    EXPECT_TRUE(rig().engineEdits.empty());
}

TEST_F(EnginesPageTest, AnAnsweredAddIsAdded) {
    load(paging::world(1));
    show(State::Page::Engines);
    rig().ready();

    clickAdder();
    rig().engineEdits.front().accepted({.name = "", .file = "/opt/ports/woof", .port = Port::Port::woof});
    rig().turn();

    EXPECT_EQ(names().back(), "Woof!");
    EXPECT_EQ(Session::get().config().engines.back().port, Port::Port::woof);
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

    rows({row("GZDoom", State::DownloadStatus::Ready), row("Woof", State::DownloadStatus::Ready)});

    EXPECT_EQ(names(), (std::vector<std::string>{"GZDoom", "Woof"}));
}

TEST_F(EnginesPageTest, BrowsingSaysWhereThePortsAreKept) {
    load(paging::world(1));
    show(State::Page::Engines);

    State::get().downloads.directory = "/data/source_ports";
    browse();

    rows({row("GZDoom", State::DownloadStatus::Ready)});

    EXPECT_TRUE(paging::shows(&page(), "Directory"));
}

TEST_F(EnginesPageTest, AReadyRowOffersAnInstall) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::DownloadRow ready = row("GZDoom", State::DownloadStatus::Ready);

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

    State::DownloadRow present = row("GZDoom", State::DownloadStatus::Installed);

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

    State::DownloadRow behind = row("GZDoom", State::DownloadStatus::Installed);

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

    State::DownloadRow failed = row("GZDoom", State::DownloadStatus::Failed);

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

    rows({row("Doom Legacy", State::DownloadStatus::Elsewhere)});

    EXPECT_EQ(tags(cards().front()), (std::vector<std::string>{"Its own site"}));
    EXPECT_EQ(cards().front()->told, "Fetched from its own site");
    EXPECT_EQ(paging::hinted<ttk::Button>(cards().front(), "Fetch"), nullptr);
}

TEST_F(EnginesPageTest, ARowWithNothingToFetchIsAWarning) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    rows({row("Odamex", State::DownloadStatus::Unavailable)});

    EXPECT_EQ(cards().front()->tags.front().text, "No downloads available");
    EXPECT_EQ(cards().front()->tags.front().kind, State::BadgeKind::Warning);
}

TEST_F(EnginesPageTest, ARowBeingAskedAboutSaysSo) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    rows({row("GZDoom", State::DownloadStatus::Checking)});

    EXPECT_EQ(cards().front()->tags.front().text, "Checking");
}

TEST_F(EnginesPageTest, AFetchShowsItsProgressAndCanBeStopped) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::DownloadRow fetching = row("GZDoom", State::DownloadStatus::Fetching);

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

    rows({row("GZDoom", State::DownloadStatus::Unpacking)});

    EXPECT_DOUBLE_EQ(cards().front()->progress, 1.0);
}

TEST_F(EnginesPageTest, ProgressMovesTheBarWithoutARebuild) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::DownloadRow fetching = row("GZDoom", State::DownloadStatus::Fetching);

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

    State::get().downloads.trouble = "GitHub could not be reached.";
    browse();

    EXPECT_TRUE(paging::shows(&page(), "GitHub could not be reached."));
}

TEST_F(EnginesPageTest, ADosRowIsTagged) {
    load(paging::world(1));
    show(State::Page::Engines);
    browse();

    State::DownloadRow dos = row("Doom", State::DownloadStatus::Ready);

    dos.dos = true;
    rows({dos});

    EXPECT_EQ(tags(cards().front()), (std::vector<std::string>{"DOS"}));
}

}
