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
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Select.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "core/launch/Storage.h"
#include "gui/components/SavesPanel.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

using parts::key;
namespace Code = ttk::Code;
namespace fs = std::filesystem;

constexpr BLRect SPOT{20.0, 20.0, 1000.0, 600.0};

Config saving(const std::string &program, const bool enabled, const std::string &file = {}) {
    Config made = parts::onPort(program);
    Profile &first = made.profiles.front();

    first.save = SaveSettings{.enabled = enabled, .file = file};
    first.saveOpen = enabled;

    return made;
}

class SavesPanelTest : public parts::Screened {
protected:
    components::SavesPanel *make(Config what, const std::vector<std::string> &saves = {}) {
        tests::Fixtures::install(std::move(what));

        // Every case in the process shares the sandbox, so the folder starts over.
        if (const fs::path folder = Storage::saveFolder(Session::get().config()); !folder.empty()) {
            fs::remove_all(folder);
        }

        for (const std::string &save : saves) {
            write(save);
        }

        rig().config().reload();

        _panel = parts::place(screen(), std::make_unique<components::SavesPanel>(&rig().reach()), SPOT);

        sync();

        return _panel;
    }

    // Each a second newer than the last, so newest first is the order written.
    void write(const std::string &name) {
        const fs::path folder = Storage::saveFolder(Session::get().config());

        fs::create_directories(folder);
        std::ofstream(folder / name) << "save";

        fs::last_write_time(folder / name, fs::file_time_type::clock::now() + std::chrono::seconds(++_written));
    }

    void sync() {
        rig().shell().advance(0.0);
        _panel->sync();
        screen().ui().relayout();
        screen().run(30);
    }

    static SaveSettings &settings() { return Session::get().config().activeProfile().save; }

    static Profile &active() { return Session::get().config().activeProfile(); }

    ttk::Label &said() { return *static_cast<ttk::Label *>(_panel->pill()->parent()->children()[2].get()); }

    // After the rule and the row the list is picked from.
    ttk::Label &note() { return *static_cast<ttk::Label *>(_panel->body()->children()[2].get()); }

    ttk::MultistateSwitch &on() { return *static_cast<ttk::MultistateSwitch *>(_panel->tools()->children()[0].get()); }

    ttk::Select &saves() { return *parts::find<ttk::Select>(_panel); }

    void press(ttk::Widget &target, const int code) {
        ui().focus(&target);
        ui().key(key(code));
        sync();
    }

    components::SavesPanel *_panel = nullptr;

private:
    int _written = 0;
};

TEST_F(SavesPanelTest, IsShownForAPortThatLoadsByName) {
    make(saving("gzdoom", false));

    EXPECT_TRUE(_panel->visible());
}

TEST_F(SavesPanelTest, IsHiddenForAPortThatCannotLoadOne) {
    make(saving("dsda-doom", false));

    EXPECT_FALSE(_panel->visible());
}

TEST_F(SavesPanelTest, IsHiddenForAPortWithNoSaveFolder) {
    make(saving("doomlegacy", false));

    EXPECT_FALSE(_panel->visible());
}

TEST_F(SavesPanelTest, OffItSaysEveryLaunchStartsANewGame) {
    make(saving("gzdoom", false));

    EXPECT_EQ(said().text(), "Off · every launch starts a new game");
    EXPECT_EQ(on().current(), 0);
    EXPECT_DOUBLE_EQ(_panel->wanted_height(screen().type(), SPOT.w), ttk::Theme::control + 32.0);
}

TEST_F(SavesPanelTest, TurningItOnOpensThePanel) {
    make(saving("gzdoom", false));
    press(on(), Code::Right);

    EXPECT_TRUE(settings().enabled);
    EXPECT_TRUE(active().saveOpen);
    EXPECT_TRUE(_panel->body()->visible());
}

TEST_F(SavesPanelTest, TurningItOnPicksTheNewestSave) {
    make(saving("gzdoom", false), {"old.zds", "new.zds"});
    press(on(), Code::Right);

    EXPECT_EQ(settings().file, "new.zds");
}

TEST_F(SavesPanelTest, TurningItOffFoldsThePanel) {
    make(saving("gzdoom", true, "one.zds"), {"one.zds"});
    press(on(), Code::Left);

    EXPECT_FALSE(settings().enabled);
    EXPECT_FALSE(active().saveOpen);
}

TEST_F(SavesPanelTest, NothingSavedYetSaysSo) {
    make(saving("gzdoom", true));

    EXPECT_EQ(said().text(), "nothing picked yet");
    EXPECT_TRUE(note().visible());
    EXPECT_TRUE(note().text().starts_with("Nothing has been saved in this profile yet."));
    EXPECT_FALSE(saves().enabled());
}

TEST_F(SavesPanelTest, SavesWithNonePickedWarn) {
    make(saving("gzdoom", true), {"one.zds"});

    EXPECT_TRUE(note().text().starts_with("Without a save picked there is nothing to load"));
    EXPECT_TRUE(saves().enabled());
    EXPECT_EQ(saves().current(), -1);
}

TEST_F(SavesPanelTest, APickedSaveIsSaidAndShown) {
    make(saving("gzdoom", true, "one.zds"), {"one.zds", "two.zds"});

    EXPECT_TRUE(said().text().starts_with("from ")) << said().text();
    EXPECT_EQ(saves().current(), 1);
    EXPECT_FALSE(note().visible());
}

TEST_F(SavesPanelTest, ThePathSaysWhereTheSaveIs) {
    make(saving("gzdoom", true, "one.zds"), {"one.zds"});

    ttk::Label *path = parts::labelled(_panel, State::get().cfg.savePath);

    ASSERT_NE(path, nullptr);
    EXPECT_TRUE(parts::visibleToTheRoot(path));
    EXPECT_EQ(path->hint, "Show in file explorer");
}

TEST_F(SavesPanelTest, PickingASaveFromTheListWritesIt) {
    make(saving("gzdoom", true, "one.zds"), {"one.zds", "two.zds"});
    press(saves(), Code::Up);

    EXPECT_EQ(settings().file, "two.zds");
}

TEST_F(SavesPanelTest, RefreshFindsASaveWrittenSince) {
    make(saving("gzdoom", true, "one.zds"), {"one.zds"});
    write("since.zds");
    parts::click(ui(), *parts::hinted<ttk::GlyphButton>(_panel, "Refresh folder"));
    sync();

    EXPECT_EQ(State::get().cfg.saveFiles, (std::vector<std::string>{"since.zds", "one.zds"}));
}

TEST_F(SavesPanelTest, OnlySavesOfThePortsKindAreListed) {
    make(saving("gzdoom", true), {"one.zds", "notes.txt", "two.dsg"});

    EXPECT_EQ(State::get().cfg.saveFiles, (std::vector<std::string>{"one.zds"}));
}

TEST_F(SavesPanelTest, ASlotPortSaysTheSlotIsWhatTheyAreHanded) {
    make(saving("prboom-plus", true));

    EXPECT_EQ(saves().hint, "The saves in this profile's folder, newest first. The port is handed the slot it sits in");
}

TEST_F(SavesPanelTest, ANamePortSaysOnlyWhatIsListed) {
    make(saving("gzdoom", true));

    EXPECT_EQ(saves().hint, "The saves in this profile's folder, newest first");
}

TEST_F(SavesPanelTest, APortsOwnSettingsHaveNoSavesOfTheirOwn) {
    Config what = saving("gzdoom", true);

    what.profiles.front().sharedConfig = true;
    make(std::move(what));

    ASSERT_TRUE(_panel->visible());
    EXPECT_TRUE(note().text().starts_with("This profile launches on the settings the source port keeps"));
}

TEST_F(SavesPanelTest, ARecordingLeavesTheSaveOut) {
    Config what = saving("gzdoom", true, "one.zds");

    what.profiles.front().replay.mode = ReplayMode::Record;
    what.profiles.front().replay.file = "run";
    make(std::move(what), {"one.zds"});

    EXPECT_TRUE(note().text().starts_with("A demo is being recorded"));
}

TEST_F(SavesPanelTest, APlaybackLeavesTheSaveOut) {
    Config what = saving("gzdoom", true, "one.zds");

    what.profiles.front().replay.mode = ReplayMode::Play;
    what.profiles.front().replay.file = "run.lmp";
    make(std::move(what), {"one.zds"});

    EXPECT_TRUE(note().text().starts_with("A demo is being played back"));
}

TEST_F(SavesPanelTest, TroubleIsSaidInTheNote) {
    make(saving("gzdoom", true, "one.zds"), {"one.zds"});
    State::get().cfg.saveTrouble = "The save is gone.";
    _panel->sync();

    EXPECT_TRUE(note().visible());
    EXPECT_EQ(note().text(), "The save is gone.");
}

TEST_F(SavesPanelTest, ClickingTheHeadingOpensIt) {
    make(saving("gzdoom", false));
    parts::click(ui(), SPOT.x + 40.0, SPOT.y + 16.0 + (ttk::Theme::control / 2.0));
    sync();

    EXPECT_TRUE(active().saveOpen);
    EXPECT_FALSE(settings().enabled);
}

TEST_F(SavesPanelTest, OpeningRepaintsLikeAFullFrame) {
    make(saving("gzdoom", false), {"one.zds"});
    screen().full();
    ui().focus(&on());
    ui().key(key(Code::Right));
    rig().shell().advance(0.0);
    _panel->sync();
    screen().ui().relayout();
    screen().run(5);

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

// The demo note checks for a save file but not that saves are on.
TEST_F(SavesPanelTest, ADemoDoesNotExplainAwayASaveThatIsOff) {
    Config what = saving("gzdoom", false, "one.zds");

    what.profiles.front().saveOpen = true;
    what.profiles.front().replay.mode = ReplayMode::Record;
    what.profiles.front().replay.file = "run";
    make(std::move(what), {"one.zds"});

    ASSERT_EQ(said().text(), "Off · every launch starts a new game");
    EXPECT_FALSE(note().visible() && note().text().starts_with("A demo is being"));
}

}
