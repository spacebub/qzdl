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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/Toggle.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/RunPanel.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

using parts::key;
namespace Code = ttk::Code;

constexpr const char *NOTE = "Treated as the latest release until a version is picked.";

constexpr BLRect SPOT{20.0, 20.0, components::RunPanel::WIDTH, 820.0};

// The game is the corpus IWAD, so the map list is read out of it.
Config grounded(const std::string &program = "gzdoom", const bool dos = false) {
    Config made = tests::Fixtures::grounded(3, 0);

    made.ports.front().file = "/ports/" + program;
    made.ports.front().dosbox = dos;

    return made;
}

class RunPanelTest : public parts::Screened {
protected:
    components::RunPanel *make(Config what = grounded(), const BLRect &spot = SPOT) {
        parts::install(rig(), std::move(what));

        _panel = parts::place(screen(), std::make_unique<components::RunPanel>(&rig().reach()), spot);

        sync();

        return _panel;
    }

    void sync() {
        rig().shell().advance(0.0);
        _panel->sync();
        screen().ui().relayout();
        screen().frame();
    }

    static Profile &active() { return Session::get().config().activeProfile(); }

    ttk::Select &select(const std::string &caption) { return *parts::captioned<ttk::Select>(_panel, caption); }

    ttk::Toggle &capture() { return *parts::all<ttk::Toggle>(_panel)[0]; }

    ttk::Toggle &toggle(const std::string &hint) { return *parts::hinted<ttk::Toggle>(_panel, hint); }

    ttk::Toggle &fullscreen() { return toggle("Give DOSBox the whole screen rather than a window"); }

    ttk::Toggle &autoClose() { return toggle("Quit DOSBox along with the running command"); }

    ttk::Toggle &shared() {
        return toggle("Launch on the settings the source port keeps for itself, shared with everything else that "
                      "uses them");
    }

    ttk::Label *note() { return parts::labelled(_panel, NOTE); }

    ttk::Label *pickVersion() { return parts::labelled(_panel, "Pick a version"); }

    // A link answers only over its text, which starts at the left of its box.
    void clickLink() {
        const BLRect &box = pickVersion()->box();

        parts::click(ui(), box.x + 8.0, box.y + (box.h / 2.0));
    }

    ttk::Button &addPort() { return *parts::hinted<ttk::Button>(_panel, "Ports are set up on the Engines page"); }

    ttk::Button &addGame() {
        return *parts::hinted<ttk::Button>(_panel, "Games are added on the library's games shelf");
    }

    void press(ttk::Widget &target, const int code) {
        ui().focus(&target);
        ui().key(key(code));
        sync();
    }

    void toggled(ttk::Widget &target) {
        parts::flip(ui(), target);
        sync();
    }

    components::RunPanel *_panel = nullptr;
};

TEST_F(RunPanelTest, WithNoPortsItOffersToAddOne) {
    Config what = grounded();

    what.ports.clear();
    make(std::move(what));

    EXPECT_TRUE(addPort().visible());
    EXPECT_FALSE(select("Source port").visible());
}

TEST_F(RunPanelTest, AddingAPortGoesToTheEngines) {
    Config what = grounded();

    what.ports.clear();
    make(std::move(what));
    parts::click(ui(), addPort());

    EXPECT_EQ(rig().visited, (std::vector<State::Page>{State::Page::Engines}));
}

TEST_F(RunPanelTest, WithNoGamesItOffersToAddOne) {
    Config what = grounded();

    what.iwads.clear();
    make(std::move(what));

    EXPECT_TRUE(addGame().visible());
    EXPECT_FALSE(select("Game").visible());
}

TEST_F(RunPanelTest, AddingAGameGoesToTheGamesShelf) {
    Config what = grounded();

    what.iwads.clear();
    make(std::move(what));
    parts::click(ui(), addGame());

    EXPECT_EQ(State::get().nav.shelf, State::Shelf::Games);
    EXPECT_EQ(rig().visited, (std::vector<State::Page>{State::Page::Library}));
}

TEST_F(RunPanelTest, WithPortsAndGamesThereIsNothingToAdd) {
    make();

    EXPECT_FALSE(addPort().visible());
    EXPECT_FALSE(addGame().visible());
}

TEST_F(RunPanelTest, ThePortSelectShowsTheProfilesPort) {
    make();

    EXPECT_EQ(select("Source port").current(), 0);
}

TEST_F(RunPanelTest, PickingTheNextPortWritesIt) {
    make();
    press(select("Source port"), Code::Down);

    EXPECT_EQ(active().port, "Port 1");
    EXPECT_EQ(select("Source port").current(), 1);
}

TEST_F(RunPanelTest, ClearingThePortEmptiesIt) {
    make();
    press(select("Source port"), Code::Up);

    EXPECT_TRUE(active().port.empty());
    EXPECT_EQ(select("Source port").current(), -1);
}

TEST_F(RunPanelTest, TheGameSelectShowsAndWritesTheGame) {
    make();

    EXPECT_EQ(select("Game").current(), 0);

    press(select("Game"), Code::Down);

    EXPECT_EQ(active().iwad, "Test PK3");
}

TEST_F(RunPanelTest, TheMapSelectShowsTheWarp) {
    Config what = grounded();

    what.profiles.front().warp = "MAP05";
    make(std::move(what));

    EXPECT_EQ(select("Map").current(), 4);
}

TEST_F(RunPanelTest, PickingTheNextMapWritesTheWarp) {
    Config what = grounded();

    what.profiles.front().warp = "MAP05";
    make(std::move(what));
    press(select("Map"), Code::Down);

    EXPECT_EQ(active().warp, "MAP06");
}

TEST_F(RunPanelTest, AWarpTheGameDoesNotHaveShowsNothingPicked) {
    Config what = grounded();

    what.profiles.front().warp = "E9M9";
    make(std::move(what));

    EXPECT_EQ(select("Map").current(), -1);
}

TEST_F(RunPanelTest, TheSkillSelectCountsFromOne) {
    Config what = grounded();

    what.profiles.front().skill = 4;
    make(std::move(what));

    EXPECT_EQ(select("Skill").current(), 3);

    press(select("Skill"), Code::Down);

    EXPECT_EQ(active().skill, 5);
}

TEST_F(RunPanelTest, ClearingTheSkillLeavesItToThePort) {
    Config what = grounded();

    what.profiles.front().skill = 1;
    make(std::move(what));
    press(select("Skill"), Code::Up);

    EXPECT_EQ(active().skill, 0);
}

TEST_F(RunPanelTest, TheMonstersSelectCountsFromOne) {
    Config what = grounded();

    what.profiles.front().monsters = 0;
    make(std::move(what));

    EXPECT_EQ(select("Monsters").current(), -1);

    press(select("Monsters"), Code::Down);

    EXPECT_EQ(active().monsters, 1);
}

TEST_F(RunPanelTest, ClickingCaptureLogsTheOutput) {
    make();

    EXPECT_FALSE(capture().checked);

    toggled(capture());

    EXPECT_TRUE(active().on(Switch::captureOutput));
    EXPECT_TRUE(capture().checked);
}

TEST_F(RunPanelTest, CaptureIsOffWhileZDLClosesOnLaunch) {
    Config what = grounded();

    what.general.autoClose = true;
    what.profiles.front().set(Switch::captureOutput, true);
    make(std::move(what));

    EXPECT_FALSE(capture().enabled());
    EXPECT_FALSE(capture().checked);
    EXPECT_TRUE(capture().hint.starts_with("Nothing to record while ZDL4 closes on launch"));
}

TEST_F(RunPanelTest, ADosPortHidesCaptureAndShowsDosBoxs) {
    make(grounded("doom2", true));

    EXPECT_FALSE(capture().visible());
    EXPECT_TRUE(parts::visibleToTheRoot(&fullscreen()));
    EXPECT_TRUE(parts::visibleToTheRoot(&autoClose()));
}

TEST_F(RunPanelTest, AnotherPortHasNoDosBoxToggles) {
    make();

    EXPECT_TRUE(capture().visible());
    EXPECT_FALSE(parts::visibleToTheRoot(&fullscreen()));
}

TEST_F(RunPanelTest, ClickingFullScreenWritesIt) {
    make(grounded("doom2", true));

    ASSERT_TRUE(fullscreen().checked);

    toggled(fullscreen());

    EXPECT_FALSE(active().on(Switch::dosFullscreen));
}

TEST_F(RunPanelTest, ClickingAutoCloseWritesIt) {
    make(grounded("doom2", true));
    toggled(autoClose());

    EXPECT_FALSE(active().on(Switch::dosExit));
}

TEST_F(RunPanelTest, ClickingThePortsConfigSharesIt) {
    make();

    ASSERT_TRUE(shared().visible());

    toggled(shared());

    EXPECT_TRUE(active().on(Switch::sharedConfig));
}

TEST_F(RunPanelTest, WithoutProfileConfigsThereIsNothingToShare) {
    Config what = grounded();

    what.general.profileConfigs = false;
    make(std::move(what));

    EXPECT_FALSE(shared().visible());
}

TEST_F(RunPanelTest, ADosPortHasNothingToShare) {
    make(grounded("doom2", true));

    EXPECT_FALSE(shared().visible());
}

TEST_F(RunPanelTest, TheDirectoryIsTheProfilesFolder) {
    make();

    ASSERT_FALSE(State::get().cfg.profileDirectory.empty());
    EXPECT_NE(parts::labelled(_panel, State::get().cfg.profileDirectory), nullptr);
}

TEST_F(RunPanelTest, TheSelectsRunTheWidthOfThePanel) {
    make();

    const BLRect &port = select("Source port").box();

    EXPECT_DOUBLE_EQ(port.x, SPOT.x + 16.0);
    EXPECT_DOUBLE_EQ(port.w, SPOT.w - 32.0);
}

TEST_F(RunPanelTest, SkillAndMonstersShareARowOnAWidePanel) {
    make(grounded(), BLRect{20.0, 20.0, 600.0, 820.0});

    const BLRect &skill = select("Skill").box();
    const BLRect &monsters = select("Monsters").box();

    EXPECT_DOUBLE_EQ(skill.y, monsters.y);
    EXPECT_NEAR(skill.w, monsters.w, 1.0);
    EXPECT_NEAR(monsters.x - (skill.x + skill.w), 12.0, 1.0);
}

TEST_F(RunPanelTest, APortWithReleasesAndNoVersionSaysSoWithALink) {
    make(grounded("dsda-doom"));

    EXPECT_TRUE(parts::visibleToTheRoot(note()));
    EXPECT_TRUE(parts::visibleToTheRoot(pickVersion()));
}

TEST_F(RunPanelTest, APortThatNeedsNothingShowsNoNote) {
    make(grounded("gzdoom"));

    EXPECT_FALSE(parts::visibleToTheRoot(note()));
    EXPECT_FALSE(parts::visibleToTheRoot(pickVersion()));
}

TEST_F(RunPanelTest, AVersionPickedClearsTheNote) {
    Config what = grounded("dsda-doom");

    what.ports.front().version = "0.28.0";
    make(std::move(what));

    EXPECT_FALSE(parts::visibleToTheRoot(note()));
    EXPECT_FALSE(parts::visibleToTheRoot(pickVersion()));
}

TEST_F(RunPanelTest, ADownloadedPortShowsNoNote) {
    Config what = grounded("dsda-doom");

    what.ports.front().portId = "dsda-doom";
    make(std::move(what));

    EXPECT_FALSE(parts::visibleToTheRoot(note()));
    EXPECT_FALSE(parts::visibleToTheRoot(pickVersion()));
}

TEST_F(RunPanelTest, ADosboxPortShowsNoNote) {
    make(grounded("dsda-doom", true));

    EXPECT_FALSE(parts::visibleToTheRoot(note()));
}

TEST_F(RunPanelTest, ThePortPickedDecidesWhetherTheNoteShows) {
    Config what = grounded("gzdoom");

    what.ports[1].file = "/ports/dsda-doom";
    make(std::move(what));

    EXPECT_FALSE(parts::visibleToTheRoot(note()));

    press(select("Source port"), Code::Down);

    EXPECT_TRUE(parts::visibleToTheRoot(note()));

    press(select("Source port"), Code::Up);
    press(select("Source port"), Code::Up);

    EXPECT_FALSE(parts::visibleToTheRoot(note()));
}

TEST_F(RunPanelTest, NoPortPickedShowsNoNote) {
    make(grounded("dsda-doom"));
    press(select("Source port"), Code::Up);

    EXPECT_FALSE(parts::visibleToTheRoot(note()));
}

TEST_F(RunPanelTest, TheNoteLeavesNoGapForAPortThatNeedsNothing) {
    make(grounded("gzdoom"));

    const double bare = select("Game").box().y;

    Config what = grounded("dsda-doom");

    what.ports.front().version = "0.28.0";
    parts::install(rig(), std::move(what));
    sync();

    EXPECT_DOUBLE_EQ(select("Game").box().y, bare);

    Config asked = grounded("dsda-doom");

    parts::install(rig(), std::move(asked));
    sync();

    EXPECT_GT(select("Game").box().y, bare);
}

TEST_F(RunPanelTest, TheLinkOpensThePortToPickItsVersion) {
    make(grounded("dsda-doom"));
    clickLink();

    ASSERT_EQ(rig().portEdits.size(), 1U);
    EXPECT_EQ(rig().portEdits.front().title, "Edit Port 0");
    EXPECT_EQ(rig().portEdits.front().port.file, "/ports/dsda-doom");
    EXPECT_FALSE(rig().portEdits.front().adding);
}

TEST_F(RunPanelTest, APickedVersionTakesTheNoteAway) {
    make(grounded("dsda-doom"));
    clickLink();

    PortEntry edited = rig().portEdits.front().port;

    edited.version = "0.28.0";
    rig().portEdits.front().accepted(edited);
    sync();

    EXPECT_EQ(Session::get().config().ports.front().version, "0.28.0");
    EXPECT_FALSE(parts::visibleToTheRoot(note()));
}

TEST_F(RunPanelTest, TheNoteRepaintsLikeAFullFrame) {
    make(grounded("gzdoom"));
    screen().full();

    Config what = grounded("dsda-doom");

    parts::install(rig(), std::move(what));
    rig().shell().advance(0.0);
    _panel->sync();
    screen().run(20);

    EXPECT_TRUE(parts::visibleToTheRoot(note()));
    EXPECT_EQ(parts::mismatch(screen()).pixels, 0U);
}

TEST_F(RunPanelTest, ASyncRepaintsLikeAFullFrame) {
    make();
    screen().full();
    toggled(capture());
    press(select("Skill"), Code::Down);
    screen().run(20);

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

}

TEST_F(RunPanelTest, TheOutputAndConfigTogglesStayFirst) {
    make(grounded("prboom-plus"));

    const std::vector<ttk::Toggle *> toggles = parts::all<ttk::Toggle>(_panel);

    ASSERT_GE(toggles.size(), 2U);
    EXPECT_EQ(toggles[0], &capture());
    EXPECT_EQ(toggles[1], &shared());
}
