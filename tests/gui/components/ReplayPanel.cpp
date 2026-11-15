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
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Pill.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/Toggle.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "core/launch/Dialect.h"
#include "core/launch/Storage.h"
#include "gui/components/ReplayPanel.h"
#include "gui/state/State.h"
#include "support/Rig.h"
#include "support/Sandbox.h"

namespace {

using parts::key;
namespace Code = ttk::Code;
namespace fs = std::filesystem;

constexpr BLRect SPOT{20.0, 20.0, 1000.0, 820.0};

Config replaying(const std::string &program, const ReplayMode mode, const std::string &file = {}) {
    Config made = parts::onPort(program);
    Profile &first = made.profiles.front();

    first.replay = ReplaySettings{};
    first.replay.mode = mode;
    first.replay.file = file;
    first.set(Switch::replayOpen, mode != ReplayMode::Off);

    return made;
}

class ReplayPanelTest : public parts::Screened {
protected:
    components::ReplayPanel *make(Config what, const std::vector<std::string> &demos = {}) {
        tests::Fixtures::install(std::move(what));

        // Every case in the process shares the sandbox, so the folder starts over.
        const fs::path folder = Storage::replayDirectory(Session::get().config());

        fs::remove_all(folder);

        for (const std::string &demo : demos) {
            record(demo);
        }

        rig().config().reload();

        _panel = parts::place(
                screen(), std::make_unique<components::ReplayPanel>(&rig().reach(), [this] { ++_shown; }), SPOT);

        sync();

        return _panel;
    }

    // Each a second newer than the last, so newest first is the order written.
    void record(const std::string &name) {
        const fs::path folder = Storage::replayDirectory(Session::get().config());

        fs::create_directories(folder);
        std::ofstream(folder / name) << "demo";

        fs::last_write_time(folder / name, fs::file_time_type::clock::now() + std::chrono::seconds(++_written));
    }

    void sync() {
        rig().shell().advance(0.0);
        _panel->sync();
        screen().ui().relayout();
        screen().run(30);
    }

    static ReplaySettings &settings() { return Session::get().config().activeProfile().replay; }

    static Profile &active() { return Session::get().config().activeProfile(); }

    ttk::Label &said() { return *static_cast<ttk::Label *>(_panel->pill()->parent()->children()[2].get()); }

    ttk::Label &note() { return *static_cast<ttk::Label *>(_panel->body()->children()[1].get()); }

    ttk::MultistateSwitch &mode() {
        return *static_cast<ttk::MultistateSwitch *>(_panel->tools()->children()[0].get());
    }

    ttk::GlyphButton &reset() { return *static_cast<ttk::GlyphButton *>(_panel->tools()->children()[1].get()); }

    ttk::MultistateSwitch &speed() { return *parts::all<ttk::MultistateSwitch>(_panel)[1]; }

    ttk::Select &demos() { return *parts::captioned<ttk::Select>(_panel, "Replay"); }

    ttk::Select &source() { return *parts::captioned<ttk::Select>(_panel, "Continue from"); }

    // The link to the complevel, the only label that starts with what it names.
    ttk::Label &complevel() {
        for (ttk::Label *each : parts::all<ttk::Label>(_panel)) {
            if (each->text().starts_with("Complevel") || each->text().starts_with("No complevel")
                || each->text().starts_with("Game version")) {
                return *each;
            }
        }

        throw std::runtime_error("no complevel link");
    }

    ttk::Toggle &longtics() { return *parts::all<ttk::Toggle>(_panel)[0]; }

    ttk::Toggle &soloNet() { return *parts::all<ttk::Toggle>(_panel)[1]; }

    ttk::Field &name() { return *parts::captioned<ttk::Field>(_panel, "Record it as"); }

    void press(ttk::Widget &target, const int code) {
        ui().focus(&target);
        ui().key(key(code));
        sync();
    }

    double pillWidth(const std::string &text) {
        ttk::Pill made(text);

        return made.natural_width(screen().type());
    }

    components::ReplayPanel *_panel = nullptr;

    // How often the complevel link asked for the Gameplay panel.
    int _shown = 0;

private:
    int _written = 0;
};

TEST_F(ReplayPanelTest, IsHiddenWhenThePortRecordsNothing) {
    make(replaying("gzdoom", ReplayMode::Off));
    State::get().cfg.replayRecords = false;
    _panel->sync();

    EXPECT_FALSE(_panel->visible());
}

TEST_F(ReplayPanelTest, OffItSaysSoFolded) {
    make(replaying("gzdoom", ReplayMode::Off));

    EXPECT_EQ(said().text(), "Off · nothing is recorded and nothing is played back");
    EXPECT_FALSE(_panel->pill()->visible());
    EXPECT_EQ(mode().current(), static_cast<int>(ReplayMode::Off));
    EXPECT_DOUBLE_EQ(_panel->wanted_height(screen().type(), SPOT.w), ttk::Theme::control + 32.0);
}

// The summary is worked out on every sync, and the same one again leaves the heading alone.
TEST_F(ReplayPanelTest, ASyncThatChangesNothingPaintsNothing) {
    make(replaying("gzdoom", ReplayMode::Off));
    _panel->sync();

    EXPECT_EQ(screen().frame(), 0U);
}

TEST_F(ReplayPanelTest, AChangedSummaryIsPainted) {
    make(replaying("gzdoom", ReplayMode::Off));
    settings().mode = ReplayMode::Record;
    rig().config().panels().pushReplay();
    _panel->sync();

    EXPECT_GT(screen().frame(), 0U);
    EXPECT_NE(said().text(), "Off · nothing is recorded and nothing is played back");
}

TEST_F(ReplayPanelTest, OpenAndOffItExplainsRecordAndPlay) {
    Config what = replaying("gzdoom", ReplayMode::Off);

    what.profiles.front().set(Switch::replayOpen, true);
    make(std::move(what));

    EXPECT_TRUE(note().visible());
    EXPECT_TRUE(note().text().starts_with("This profile neither records nor plays anything back."));
    EXPECT_FALSE(parts::visibleToTheRoot(&name()));
    EXPECT_FALSE(parts::visibleToTheRoot(&demos()));
}

TEST_F(ReplayPanelTest, ClickingTheHeadingOpensIt) {
    make(replaying("gzdoom", ReplayMode::Off));
    parts::click(ui(), SPOT.x + 40.0, SPOT.y + 16.0 + (ttk::Theme::control / 2.0));
    sync();

    EXPECT_TRUE(active().on(Switch::replayOpen));
    EXPECT_TRUE(_panel->body()->visible());
}

TEST_F(ReplayPanelTest, PickingRecordOpensThePanelRecording) {
    make(replaying("gzdoom", ReplayMode::Off));
    press(mode(), Code::Right);

    EXPECT_EQ(settings().mode, ReplayMode::Record);
    EXPECT_TRUE(active().on(Switch::replayOpen));
    EXPECT_TRUE(parts::visibleToTheRoot(&name()));
}

TEST_F(ReplayPanelTest, PickingPlayPicksTheNewestDemo) {
    make(replaying("gzdoom", ReplayMode::Off), {"old.lmp", "new.lmp"});
    press(mode(), Code::End);

    EXPECT_EQ(settings().mode, ReplayMode::Play);
    EXPECT_EQ(settings().file, "new.lmp");
}

TEST_F(ReplayPanelTest, PickingOffFoldsThePanel) {
    make(replaying("gzdoom", ReplayMode::Record, "run"));
    press(mode(), Code::Home);

    EXPECT_EQ(settings().mode, ReplayMode::Off);
    EXPECT_FALSE(active().on(Switch::replayOpen));
}

TEST_F(ReplayPanelTest, RecordingWithoutANameWarns) {
    make(replaying("gzdoom", ReplayMode::Record));

    EXPECT_EQ(said().text(), "no name yet");
    EXPECT_TRUE(note().visible());
    EXPECT_TRUE(note().text().starts_with("Without a name there is nothing to record into"));
}

TEST_F(ReplayPanelTest, RecordingSaysWhereItRecordsInto) {
    make(replaying("gzdoom", ReplayMode::Record, "run"));

    EXPECT_TRUE(said().text().starts_with("into ")) << said().text();
    EXPECT_FALSE(note().visible());
    EXPECT_TRUE(_panel->pill()->visible());
    EXPECT_DOUBLE_EQ(_panel->pill()->natural_width(screen().type()), pillWidth("Recording"));
}

TEST_F(ReplayPanelTest, RecordingShowsTheNameItRecordsAs) {
    make(replaying("gzdoom", ReplayMode::Record, "speedrun"));

    EXPECT_EQ(name().text(), "speedrun");
}

TEST_F(ReplayPanelTest, TypingANameWritesIt) {
    make(replaying("gzdoom", ReplayMode::Record));
    type(name(), "pacifist");

    EXPECT_EQ(settings().file, "pacifist");
}

TEST_F(ReplayPanelTest, ANameAlreadyTakenWarns) {
    make(replaying("gzdoom", ReplayMode::Record, "taken.lmp"), {"taken.lmp"});

    EXPECT_TRUE(note().visible());
    EXPECT_TRUE(note().text().starts_with("A demo by that name is in the folder already."));
}

TEST_F(ReplayPanelTest, ThePathSaysWhereTheDemoGoes) {
    make(replaying("gzdoom", ReplayMode::Record, "run"));

    ttk::Label *path = parts::labelled(_panel, State::get().cfg.replayPath);

    ASSERT_NE(path, nullptr);
    EXPECT_TRUE(parts::visibleToTheRoot(path));
    EXPECT_EQ(path->hint, "Show in file explorer");
}

TEST_F(ReplayPanelTest, PlayingWithNothingRecordedSaysSo) {
    make(replaying("gzdoom", ReplayMode::Play));

    EXPECT_EQ(said().text(), "nothing picked yet");
    EXPECT_TRUE(note().text().starts_with("This profile has recorded nothing yet"));
    EXPECT_FALSE(demos().enabled());
}

TEST_F(ReplayPanelTest, PlayingWithoutAPickWarns) {
    make(replaying("gzdoom", ReplayMode::Play), {"one.lmp"});

    EXPECT_TRUE(note().text().starts_with("Without a demo picked there is nothing to play back"));
    EXPECT_TRUE(demos().enabled());
    EXPECT_EQ(demos().current(), -1);
}

TEST_F(ReplayPanelTest, PlayingShowsThePickInTheList) {
    make(replaying("gzdoom", ReplayMode::Play, "one.lmp"), {"one.lmp", "two.lmp"});

    EXPECT_EQ(demos().current(), 1);
    EXPECT_FALSE(note().visible());
    EXPECT_DOUBLE_EQ(_panel->pill()->natural_width(screen().type()), pillWidth("Playing"));
}

TEST_F(ReplayPanelTest, PickingADemoFromTheListWritesIt) {
    make(replaying("gzdoom", ReplayMode::Play, "one.lmp"), {"one.lmp", "two.lmp"});
    press(demos(), Code::Up);

    EXPECT_EQ(settings().file, "two.lmp");
}

TEST_F(ReplayPanelTest, ClearingTheListPickEmptiesTheFile) {
    make(replaying("gzdoom", ReplayMode::Play, "two.lmp"), {"one.lmp", "two.lmp"});
    press(demos(), Code::Up);

    EXPECT_TRUE(settings().file.empty());
}

TEST_F(ReplayPanelTest, RefreshFindsADemoRecordedSince) {
    make(replaying("gzdoom", ReplayMode::Play, "one.lmp"), {"one.lmp"});
    record("since.lmp");

    // Continue has a refresh of its own, hidden while playing.
    parts::click(ui(), *parts::find<ttk::GlyphButton>(_panel, [](const ttk::GlyphButton &each) {
                     return each.hint == "Refresh folder" && parts::visibleToTheRoot(&each);
                 }));
    sync();

    EXPECT_EQ(State::get().cfg.replayFiles, (std::vector<std::string>{"since.lmp", "one.lmp"}));
}

TEST_F(ReplayPanelTest, BrowsingOpensThePicker) {
    make(replaying("gzdoom", ReplayMode::Play));
    parts::click(ui(), *parts::hinted<ttk::GlyphButton>(_panel, "Play a demo from somewhere else"));

    EXPECT_TRUE(rig().files().state().open);
    EXPECT_EQ(rig().files().state().title, "Select a replay");
}

TEST_F(ReplayPanelTest, ADemoBrowsedForIsPlayedFromWhereItIs) {
    make(replaying("gzdoom", ReplayMode::Play));
    parts::click(ui(), *parts::hinted<ttk::GlyphButton>(_panel, "Play a demo from somewhere else"));

    const fs::path elsewhere = tests::Sandbox::scratch("replay-browse") / "far.lmp";

    std::ofstream(elsewhere) << "demo";
    rig().files().choose({elsewhere.string()});

    EXPECT_EQ(settings().file, elsewhere.string());
}

TEST_F(ReplayPanelTest, ATimedemoPortOffersHowItPlays) {
    make(replaying("gzdoom", ReplayMode::Play, "one.lmp"), {"one.lmp"});

    EXPECT_TRUE(parts::visibleToTheRoot(&speed()));
}

TEST_F(ReplayPanelTest, PickingTimedWritesThePlayback) {
    make(replaying("gzdoom", ReplayMode::Play, "one.lmp"), {"one.lmp"});
    press(speed(), Code::Right);

    EXPECT_EQ(settings().playback, Playback::Timed);
}

TEST_F(ReplayPanelTest, AFastdemoPortOffersAThirdSpeed) {
    make(replaying("dsda-doom", ReplayMode::Play, "one.lmp"), {"one.lmp"});
    press(speed(), Code::End);

    EXPECT_EQ(settings().playback, Playback::Fast);
}

TEST_F(ReplayPanelTest, APortWithoutTimedemoHidesHowItPlays) {
    make(replaying("helion", ReplayMode::Play, "one.lmp"), {"one.lmp"});

    EXPECT_FALSE(parts::visibleToTheRoot(&speed()));
}

TEST_F(ReplayPanelTest, ARecordingOnDsdaOffersCompatibility) {
    make(replaying("dsda-doom", ReplayMode::Record, "run"));

    EXPECT_TRUE(parts::visibleToTheRoot(&complevel()));
    EXPECT_TRUE(parts::visibleToTheRoot(&longtics()));
    EXPECT_TRUE(parts::visibleToTheRoot(&soloNet()));
    EXPECT_NE(parts::labelled(_panel, "Compatibility"), nullptr);
}

TEST_F(ReplayPanelTest, ARecordingOnGZDoomHasNoCompatibility) {
    make(replaying("gzdoom", ReplayMode::Record, "run"));

    EXPECT_FALSE(parts::visibleToTheRoot(parts::labelled(_panel, "Compatibility")));
}

TEST_F(ReplayPanelTest, PlayingBackHidesCompatibility) {
    make(replaying("dsda-doom", ReplayMode::Play, "one.lmp"), {"one.lmp"});

    EXPECT_FALSE(parts::visibleToTheRoot(&complevel()));
}

TEST_F(ReplayPanelTest, TheComplevelLinkSaysThePortsOwnWhereNoneIsNeeded) {
    make(replaying("prboom-plus", ReplayMode::Record, "run"));

    EXPECT_EQ(complevel().text(), "Complevel: the port's own");
    EXPECT_FALSE(note().visible() && note().text().contains("complevel"));
}

TEST_F(ReplayPanelTest, TheComplevelLinkNamesTheOneSetUnderGameplay) {
    Config what = replaying("dsda-doom", ReplayMode::Record, "run");

    what.profiles.front().complevel = 21;
    make(std::move(what));

    EXPECT_EQ(complevel().text(), "Complevel: MBF21 · 21");
}

TEST_F(ReplayPanelTest, RecordingWithoutTheComplevelThePortNeedsWarns) {
    make(replaying("dsda-doom", ReplayMode::Record, "run"));

    EXPECT_EQ(complevel().text(), "No complevel");
    EXPECT_TRUE(note().visible());
    EXPECT_TRUE(note().text().contains("only with a complevel")) << note().text();
}

TEST_F(ReplayPanelTest, TheComplevelLinkShowsTheGameplayPanel) {
    make(replaying("dsda-doom", ReplayMode::Record, "run"));
    parts::click(ui(), complevel());

    EXPECT_EQ(_shown, 1);
}

TEST_F(ReplayPanelTest, LongTicsAndSoloNetAreWritten) {
    make(replaying("dsda-doom", ReplayMode::Record, "run"));
    parts::flip(ui(), longtics());
    sync();
    parts::flip(ui(), soloNet());
    sync();

    EXPECT_TRUE(settings().longtics);
    EXPECT_TRUE(settings().soloNet);
    EXPECT_TRUE(longtics().checked);
}

TEST_F(ReplayPanelTest, TroubleIsSaidInTheNoteInDanger) {
    make(replaying("gzdoom", ReplayMode::Record, "run"));
    State::get().cfg.replayTrouble = "The replays folder cannot be written.";
    _panel->sync();

    EXPECT_TRUE(note().visible());
    EXPECT_EQ(note().text(), "The replays folder cannot be written.");
}

TEST_F(ReplayPanelTest, ResetIsOffWithNothingSet) {
    make(replaying("gzdoom", ReplayMode::Off));

    EXPECT_FALSE(reset().enabled());
    EXPECT_EQ(reset().hint, "Nothing here has been set");
}

TEST_F(ReplayPanelTest, ResetAsksFirst) {
    make(replaying("gzdoom", ReplayMode::Record, "run"));
    parts::click(ui(), reset());

    ASSERT_EQ(rig().asks.size(), 1U);
    EXPECT_EQ(rig().asks.front().title, "Reset the replay settings?");
    EXPECT_EQ(settings().mode, ReplayMode::Record);
}

TEST_F(ReplayPanelTest, AcceptingTheResetClearsTheReplay) {
    make(replaying("dsda-doom", ReplayMode::Record, "run"), {"kept.lmp"});
    parts::click(ui(), reset());

    ASSERT_EQ(rig().asks.size(), 1U);

    rig().asks.front().accepted();

    EXPECT_EQ(settings(), ReplaySettings{});
    EXPECT_TRUE(fs::exists(Storage::replayDirectory(Session::get().config()) / "kept.lmp"));
}

TEST_F(ReplayPanelTest, SwitchingModeRepaintsLikeAFullFrame) {
    make(replaying("dsda-doom", ReplayMode::Play, "one.lmp"), {"one.lmp"});

    // A glyph is stroked the first time and laid from a mask after, a level apart at its edges.
    screen().full();
    screen().full();
    press(mode(), Code::Left);
    screen().full();
    ui().focus(&mode());
    ui().key(key(Code::Right));
    rig().shell().advance(0.0);
    _panel->sync();
    screen().ui().relayout();
    screen().run(4);

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

}

TEST_F(ReplayPanelTest, ContinueIsOfferedOnlyWherePortsTakeIt) {
    make(replaying("dsda-doom", ReplayMode::Off));

    for (int step = 0; step < 3; step++) {
        press(mode(), Code::Right);
    }

    EXPECT_EQ(settings().mode, ReplayMode::Continue);

    make(replaying("gzdoom", ReplayMode::Off));

    for (int step = 0; step < 3; step++) {
        press(mode(), Code::Right);
    }

    EXPECT_EQ(settings().mode, ReplayMode::Play);
}

TEST_F(ReplayPanelTest, ContinuePicksTheNewestDemoAndAsksForANewName) {
    make(replaying("woof", ReplayMode::Off), {"one.lmp", "two.lmp"});

    for (int step = 0; step < 3; step++) {
        press(mode(), Code::Right);
    }

    EXPECT_EQ(settings().source, "two.lmp");
    EXPECT_TRUE(parts::visibleToTheRoot(&source()));
    EXPECT_TRUE(parts::visibleToTheRoot(&name()));
    EXPECT_FALSE(parts::visibleToTheRoot(&demos()));
    EXPECT_EQ(said().text(), "no name yet");
}

TEST_F(ReplayPanelTest, PickingTheSourceFromTheListWritesIt) {
    Config what = replaying("prboom-plus", ReplayMode::Continue, "next");

    what.profiles.front().replay.source = "two.lmp";
    make(std::move(what), {"one.lmp", "two.lmp"});
    press(source(), Code::Down);

    EXPECT_EQ(settings().source, "one.lmp");
}

TEST_F(ReplayPanelTest, AProfileLeftOnContinueSaysThePortCannot) {
    Config what = replaying("gzdoom", ReplayMode::Continue, "next");

    what.profiles.front().replay.source = "one.lmp";
    make(std::move(what), {"one.lmp"});

    EXPECT_TRUE(note().text().starts_with("This port cannot continue a demo")) << note().text();
}

TEST_F(ReplayPanelTest, ChocolateNamesTheGameVersionInsteadOfAComplevel) {
    Config what = replaying("chocolate-doom", ReplayMode::Record, "run");

    what.profiles.front().complevel = Dialect::GAME_VERSIONS + 5;
    make(std::move(what));

    EXPECT_EQ(complevel().text(), "Game version: Doom 1.9 · 1.9");
}
