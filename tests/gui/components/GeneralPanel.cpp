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

#include <chrono>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "ttk/toolkit/controls/Fact.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Toggle.h"

#include "Mount.h"
#include "core/config/Options.h"
#include "core/config/Session.h"
#include "gui/components/GeneralPanel.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

using parts::key;
namespace Code = ttk::Code;

constexpr BLRect SPOT{20.0, 20.0, 1000.0, 600.0};

Config general(const std::string &file) {
    Config made = parts::onEngine(file);

    made.profiles.front().set(Switch::generalOpen, true);

    return made;
}

class GeneralPanelTest : public parts::Screened {
protected:
    components::GeneralPanel *make(Config what) {
        tests::Fixtures::install(std::move(what));
        rig().config().reload();

        _panel = parts::place(screen(), std::make_unique<components::GeneralPanel>(&rig().reach()), SPOT);

        sync();

        return _panel;
    }

    void sync() {
        rig().shell().advance(0.0);
        _panel->sync();
        screen().ui().relayout();
        screen().run(30);
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

    static Profile &active() { return Session::get().config().activeProfile(); }

    ttk::Label &said() { return *static_cast<ttk::Label *>(_panel->pill()->parent()->children()[2].get()); }

    ttk::Toggle *pistolStart() {
        return parts::find<ttk::Toggle>(_panel, [](const ttk::Toggle &each) {
            return each.hint.starts_with("Starts every map with only the pistol");
        });
    }

    ttk::Toggle *cheats() {
        return parts::find<ttk::Toggle>(
                _panel, [](const ttk::Toggle &each) { return each.hint.starts_with("Turns on sv_cheats"); });
    }

    // The toggles come in table order, and the rules live under Gameplay.
    ttk::Toggle *toggle(const Dialect::Feature option) {
        std::size_t at = 0;

        for (const Options::Option &each : Options::ALL) {
            if (each.feature == option) {
                return parts::all<ttk::Toggle>(_panel).at(at);
            }

            at += each.group != Options::Group::rules ? 1 : 0;
        }

        return nullptr;
    }

    ttk::Toggle *levelstat() { return toggle(Dialect::Feature::levelstat); }

    ttk::Toggle *skipAutoload() { return toggle(Dialect::Feature::noAutoload); }

    ttk::Toggle *music() { return toggle(Dialect::Feature::nomusic); }

    ttk::Toggle *soundEffects() { return toggle(Dialect::Feature::nosfx); }

    // The switch under a group caption, as Display and Screenshots both offer Port.
    ttk::MultistateSwitch &under(const std::string &caption) {
        return *parts::find<ttk::MultistateSwitch>(parts::labelled(_panel, caption)->parent());
    }

    ttk::MultistateSwitch &shots() { return under("Screenshots"); }

    ttk::MultistateSwitch &mode() { return under("Display"); }

    ttk::Fact *where() { return parts::find<ttk::Fact>(_panel); }

    ttk::Field *folder() { return parts::captioned<ttk::Field>(_panel, "Directory"); }

    components::GeneralPanel *_panel = nullptr;
};

}

// The panel syncs on every change to the profile, and one that changes nothing here paints nothing.
TEST_F(GeneralPanelTest, ASyncThatChangesNothingPaintsNothing) {
    make(general("gzdoom"));

    ASSERT_TRUE(_panel->visible());

    _panel->sync();

    EXPECT_EQ(screen().frame(), 0U);
}

TEST_F(GeneralPanelTest, PistolStartAndCheatsLiveUnderGameplay) {
    make(general("gzdoom"));

    EXPECT_EQ(pistolStart(), nullptr);
    EXPECT_EQ(cheats(), nullptr);
}

TEST_F(GeneralPanelTest, ChocolateHasSoundDisplayAndAutoloadButNoRules) {
    make(general("chocolate-doom"));

    EXPECT_TRUE(_panel->visible());
    EXPECT_TRUE(parts::visibleToTheRoot(skipAutoload()));
    EXPECT_TRUE(parts::visibleToTheRoot(music()));
    EXPECT_TRUE(parts::visibleToTheRoot(soundEffects()));
    EXPECT_TRUE(parts::visibleToTheRoot(&mode()));
    EXPECT_FALSE(parts::visibleToTheRoot(&shots()));
}

TEST_F(GeneralPanelTest, HelionHasMusicButNoSoundEffectsDisplayOrAutoload) {
    make(general("helion"));

    EXPECT_TRUE(parts::visibleToTheRoot(music()));
    EXPECT_FALSE(parts::visibleToTheRoot(soundEffects()));
    EXPECT_FALSE(parts::visibleToTheRoot(skipAutoload()));
    EXPECT_FALSE(parts::visibleToTheRoot(&mode()));
}

TEST_F(GeneralPanelTest, LevelStatsSitInTheFilesRowForAPortThatWritesThem) {
    make(general("gzdoom"));

    EXPECT_FALSE(parts::visibleToTheRoot(levelstat()));

    make(general("dsda-doom"));

    ASSERT_TRUE(parts::visibleToTheRoot(levelstat()));
    EXPECT_EQ(levelstat()->parent(), skipAutoload()->parent());

    toggled(*levelstat());

    EXPECT_TRUE(active().optionOn(Dialect::Feature::levelstat));
}

TEST_F(GeneralPanelTest, EachGroupStartsItsOwnLine) {
    make(general("dsda-doom"));

    const double game = parts::labelled(_panel, "Files")->box().y;
    const double sound = parts::labelled(_panel, "Sound")->box().y;
    const double display = parts::labelled(_panel, "Display")->box().y;
    const double shots = parts::labelled(_panel, "Screenshots")->box().y;

    EXPECT_LT(game, sound);
    EXPECT_LT(sound, display);
    EXPECT_LT(display, shots);
}

TEST_F(GeneralPanelTest, EachToggleSaysWhatItDoes) {
    make(general("dsda-doom"));

    EXPECT_TRUE(levelstat()->hint.starts_with("Writes a levelstat.txt")) << levelstat()->hint;
    EXPECT_TRUE(skipAutoload()->hint.starts_with("Starts without the files the port loads on its own"))
            << skipAutoload()->hint;
    EXPECT_EQ(music()->hint, "Off starts the port with no music.");
    EXPECT_EQ(soundEffects()->hint, "Off starts the port with no sound effects.");
}

TEST_F(GeneralPanelTest, MusicAndSoundEffectsAreCheckedWhileTheirBitIsClear) {
    make(general("woof"));

    ASSERT_FALSE(active().optionOn(Dialect::Feature::nomusic));
    EXPECT_TRUE(music()->checked);
    EXPECT_TRUE(soundEffects()->checked);
    EXPECT_FALSE(levelstat()->checked);
    EXPECT_FALSE(skipAutoload()->checked);

    Config what = general("woof");

    what.profiles.front().setOption(Dialect::Feature::nomusic, true);
    make(std::move(what));

    EXPECT_FALSE(music()->checked);
    EXPECT_TRUE(soundEffects()->checked);
}

TEST_F(GeneralPanelTest, ClickingTheNewTogglesWritesThem) {
    make(general("woof"));
    toggled(*skipAutoload());
    toggled(*music());
    toggled(*soundEffects());

    EXPECT_TRUE(active().optionOn(Dialect::Feature::noAutoload));
    EXPECT_TRUE(active().optionOn(Dialect::Feature::nomusic));
    EXPECT_TRUE(active().optionOn(Dialect::Feature::nosfx));
    EXPECT_FALSE(music()->checked);
}

TEST_F(GeneralPanelTest, PickingWindowOnTheDisplaySwitchWritesIt) {
    make(general("dsda-doom"));

    ASSERT_EQ(mode().current(), static_cast<int>(Display::Port));

    press(mode(), Code::Right);

    EXPECT_EQ(active().display, Display::Window);
    EXPECT_EQ(mode().current(), static_cast<int>(Display::Window));
    EXPECT_FALSE(mode().hint.contains("keeps the choice"));

    make(general("gzdoom"));

    EXPECT_TRUE(mode().hint.contains("keeps the choice"));
}

TEST_F(GeneralPanelTest, AnotherPortHidesWhatItCannotTake) {
    make(general("gzdoom"));

    ASSERT_TRUE(parts::visibleToTheRoot(soundEffects()));
    ASSERT_TRUE(parts::visibleToTheRoot(&mode()));

    Session::get().config().engines.front().file = "/games/helion";
    rig().config().profile().push();
    sync();

    EXPECT_TRUE(parts::visibleToTheRoot(music()));
    EXPECT_FALSE(parts::visibleToTheRoot(soundEffects()));
    EXPECT_FALSE(parts::visibleToTheRoot(skipAutoload()));
    EXPECT_FALSE(parts::visibleToTheRoot(&mode()));
}

TEST_F(GeneralPanelTest, ThePortPicksTheScreenshotFolderAtFirst) {
    make(general("dsda-doom"));

    EXPECT_EQ(shots().current(), static_cast<int>(Screenshots::Port));
    EXPECT_FALSE(parts::visibleToTheRoot(where()));
    EXPECT_FALSE(parts::visibleToTheRoot(folder()));
    EXPECT_EQ(said().text(), "");
}

TEST_F(GeneralPanelTest, ProfileShowsTheFolderInTheProfile) {
    make(general("dsda-doom"));
    press(shots(), Code::Right);

    EXPECT_EQ(active().screenshots, Screenshots::Profile);
    EXPECT_TRUE(State::get().cfg.screenshotPath.ends_with("/screenshots")) << State::get().cfg.screenshotPath;
    EXPECT_TRUE(parts::visibleToTheRoot(where()));
    EXPECT_TRUE(parts::labelled(where(), "Directory") != nullptr);
    EXPECT_TRUE(parts::labelled(where(), State::get().cfg.screenshotPath) != nullptr);
    EXPECT_FALSE(parts::visibleToTheRoot(folder()));
    EXPECT_EQ(said().text(), "");
}

TEST_F(GeneralPanelTest, CustomWithoutAFolderAsksForOne) {
    make(general("dsda-doom"));
    press(shots(), Code::Right);
    press(shots(), Code::Right);

    EXPECT_EQ(active().screenshots, Screenshots::Custom);
    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Select a screenshots folder");
    EXPECT_TRUE(parts::visibleToTheRoot(folder()));
    EXPECT_FALSE(parts::visibleToTheRoot(where()));
}

TEST_F(GeneralPanelTest, APickedFolderLandsInTheField) {
    make(general("dsda-doom"));
    press(shots(), Code::Right);
    press(shots(), Code::Right);
    rig().files().choose({"/picked/shots"});
    sync();

    EXPECT_EQ(active().screenshotFolder, "/picked/shots");
    EXPECT_EQ(folder()->text(), "/picked/shots");
}

TEST_F(GeneralPanelTest, ATypedFolderIsKept) {
    Config what = general("dsda-doom");

    what.profiles.front().screenshots = Screenshots::Custom;
    make(std::move(what));

    ui().focus(folder()->input());
    ui().wrote("/typed");
    sync();

    EXPECT_EQ(active().screenshotFolder, "/typed");
    EXPECT_EQ(active().screenshots, Screenshots::Custom);
}

TEST_F(GeneralPanelTest, ACustomFolderIsShownAndPassed) {
    Config what = general("dsda-doom");

    what.profiles.front().screenshots = Screenshots::Custom;
    what.profiles.front().screenshotFolder = "/shots";
    make(std::move(what));

    EXPECT_EQ(folder()->text(), "/shots");
    EXPECT_TRUE(parts::visibleToTheRoot(folder()));
    EXPECT_EQ(said().text(), "");
}

TEST_F(GeneralPanelTest, HelionHasNoScreenshotChoice) {
    make(general("helion"));

    EXPECT_TRUE(parts::visibleToTheRoot(levelstat()));
    EXPECT_FALSE(parts::visibleToTheRoot(&shots()));
}

TEST_F(GeneralPanelTest, TheFileTogglesSitUnderACaption) {
    make(general("woof"));

    ttk::Label *caption = parts::labelled(_panel, "Files");

    ASSERT_NE(caption, nullptr);
    EXPECT_TRUE(parts::visibleToTheRoot(caption));
    EXPECT_LT(caption->box().y, levelstat()->box().y);

    make(general("helion"));

    EXPECT_TRUE(parts::visibleToTheRoot(parts::labelled(_panel, "Files")));

    make(general("gzdoom"));

    EXPECT_TRUE(parts::visibleToTheRoot(parts::labelled(_panel, "Files"))) << "skip autoload alone";
}
