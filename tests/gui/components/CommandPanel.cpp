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

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/notices/Notice.h"
#include "ttk/toolkit/controls/Chip.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Wrap.h"

#include "Mount.h"
#include "core/config/Session.h"
#include "gui/components/CommandPanel.h"
#include "gui/state/State.h"
#include "support/Rig.h"

namespace {

constexpr BLRect SPOT{20.0, 20.0, 900.0, 600.0};

class CommandPanelTest : public parts::Screened {
protected:
    components::CommandPanel *make(Config what = parts::onPort("gzdoom"), const BLRect &spot = SPOT) {
        parts::install(rig(), std::move(what));

        _panel = parts::place(screen(), std::make_unique<components::CommandPanel>(&rig().reach()), spot);

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

    ttk::Toggle & override() { return *parts::find<ttk::Toggle>(_panel); }

    ttk::Field &extra() { return *parts::captioned<ttk::Field>(_panel, "Extra arguments"); }

    ttk::Field &command() { return *parts::captioned<ttk::Field>(_panel, "The command"); }

    ttk::Wrap &tokens() { return *parts::find<ttk::Wrap>(_panel); }

    ttk::GlyphButton &copy() { return *parts::hinted<ttk::GlyphButton>(_panel, "Copy"); }

    // The line beside the copy button.
    ttk::Label &resolved() { return *static_cast<ttk::Label *>(copy().parent()->children().front().get()); }

    // The first chip, in the heading: the DOSBox command count.
    ttk::Chip &budget() { return *parts::find<ttk::Chip>(_panel); }

    components::CommandPanel *_panel = nullptr;
};

TEST_F(CommandPanelTest, GeneratedItTakesExtraArguments) {
    make();

    EXPECT_TRUE(parts::visibleToTheRoot(&extra()));
    EXPECT_FALSE(parts::visibleToTheRoot(&command()));
    EXPECT_FALSE(parts::visibleToTheRoot(&tokens()));
    EXPECT_FALSE(override().checked);
}

TEST_F(CommandPanelTest, TheExtraArgumentsAreTheProfiles) {
    make();

    EXPECT_EQ(extra().text(), "-fast -respawn");
}

TEST_F(CommandPanelTest, TypingExtraArgumentsWritesThem) {
    make();
    type(extra(), "-nomonsters");

    EXPECT_EQ(active().extra, "-nomonsters");
}

TEST_F(CommandPanelTest, ExtraArgumentsTypedReachTheLine) {
    make();
    type(extra(), "-nomonsters");
    sync();

    EXPECT_NE(resolved().text().find("-nomonsters"), std::string::npos) << resolved().text();
}

TEST_F(CommandPanelTest, ClickingOverrideWritesACustomCommand) {
    make();
    parts::flip(ui(), override());
    sync();

    EXPECT_TRUE(active().customCommand);
    EXPECT_TRUE(override().checked);
}

TEST_F(CommandPanelTest, AnOverrideTakesTheWholeCommandAndOffersTheTokens) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().customCommand = true;
    what.profiles.front().command = "{source_port} -iwad {game}";
    make(std::move(what));

    EXPECT_FALSE(parts::visibleToTheRoot(&extra()));
    EXPECT_TRUE(parts::visibleToTheRoot(&command()));
    EXPECT_TRUE(parts::visibleToTheRoot(&tokens()));
    EXPECT_EQ(command().text(), "{source_port} -iwad {game}");
}

TEST_F(CommandPanelTest, AnOverrideMakesThePanelTaller) {
    make();

    const double plain = _panel->wanted_height(screen().type(), SPOT.w);

    parts::flip(ui(), override());
    sync();

    EXPECT_GT(_panel->wanted_height(screen().type(), SPOT.w), plain);
}

TEST_F(CommandPanelTest, TypingTheCommandWritesIt) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().customCommand = true;
    make(std::move(what));
    type(command(), "{source_port} -file {addon_1}");

    EXPECT_EQ(active().command, "{source_port} -file {addon_1}");
}

TEST_F(CommandPanelTest, EveryTokenExplainsItself) {
    Config what = parts::onPort("gzdoom");

    what.profiles.front().customCommand = true;
    make(std::move(what));

    EXPECT_EQ(parts::all<ttk::Chip>(&tokens()).size(), 9U);
    EXPECT_NE(parts::hinted<ttk::Chip>(&tokens(), "The source port this profile is set to."), nullptr);
    EXPECT_NE(parts::hinted<ttk::Chip>(&tokens(), "An add-on from the list, counting from one."), nullptr);
}

TEST_F(CommandPanelTest, TheLineIsTheCommandLine) {
    make();

    ASSERT_FALSE(State::get().cfg.commandLine.empty());
    EXPECT_EQ(resolved().text(), State::get().cfg.commandLine);
}

TEST_F(CommandPanelTest, TroubleIsShownInsteadOfTheLine) {
    make();
    State::get().cfg.commandTrouble = "{nonsense} is not a token.";
    _panel->sync();

    EXPECT_EQ(resolved().text(), "{nonsense} is not a token.");
}

TEST_F(CommandPanelTest, NothingToRunSaysSo) {
    make();
    State::get().cfg.commandLine.clear();
    State::get().cfg.dosPort = false;
    _panel->sync();

    EXPECT_EQ(resolved().text(), "Nothing to run yet.");
}

TEST_F(CommandPanelTest, ADosPortWithoutDOSBoxSaysWhereToSetOne) {
    make();

    State::Cfg &cfg = State::get().cfg;

    cfg.commandLine.clear();
    cfg.dosPort = true;
    cfg.dosbox.clear();
    cfg.systemDosbox.clear();
    _panel->sync();

    EXPECT_EQ(resolved().text(), "A DOS source port, and no DOSBox to run it in. Set one in Settings.");
}

TEST_F(CommandPanelTest, CopyIsOffWithNoLine) {
    make();
    State::get().cfg.commandLine.clear();
    _panel->sync();

    EXPECT_FALSE(copy().enabled());
}

TEST_F(CommandPanelTest, CopyingSaysSoInANotice) {
    make();

    const size_t was = rig().notify().messages().size();

    parts::click(ui(), copy());

    ASSERT_EQ(rig().notify().messages().size(), was + 1);
    EXPECT_EQ(rig().notify().messages().back().severity, ttk::Severity::Success);
    EXPECT_EQ(rig().notify().messages().back().body, "The command line is on the clipboard.");
}

TEST_F(CommandPanelTest, ClickingTheLineShowsTheCommand) {
    make();
    parts::click(ui(), resolved());

    EXPECT_EQ(rig().commandShown, 1);
}

TEST_F(CommandPanelTest, TheBudgetIsHiddenOutsideDOSBox) {
    make();

    EXPECT_FALSE(budget().visible());
}

TEST_F(CommandPanelTest, TheBudgetShowsForADOSBoxLaunch) {
    make();
    State::get().cfg.dosCommands = 4;
    _panel->sync();

    EXPECT_TRUE(budget().visible());
}

TEST_F(CommandPanelTest, TheBudgetExplainsTheLimit) {
    make();

    EXPECT_TRUE(budget().hint.starts_with("DOSBox runs only the first eleven commands"));
}

TEST_F(CommandPanelTest, ANarrowerPanelWrapsALongLine) {
    make();

    State::get().cfg.commandLine = tests::Fixtures::words(40);
    _panel->sync();

    const double wide = _panel->wanted_height(screen().type(), 900.0);

    EXPECT_GT(_panel->wanted_height(screen().type(), 420.0), wide);
}

TEST_F(CommandPanelTest, TheLineStopsGrowingAtFourRows) {
    make();

    State::get().cfg.commandLine = tests::Fixtures::words(5);
    _panel->sync();

    const double one = _panel->wanted_height(screen().type(), 420.0);

    State::get().cfg.commandLine = tests::Fixtures::words(400);
    _panel->sync();

    const double tall = _panel->wanted_height(screen().type(), 420.0);

    State::get().cfg.commandLine = tests::Fixtures::words(800);
    _panel->sync();

    EXPECT_GT(tall, one);
    EXPECT_DOUBLE_EQ(_panel->wanted_height(screen().type(), 420.0), tall);
}

TEST_F(CommandPanelTest, ALongLineStaysInsideItsPanel) {
    make();

    State::get().cfg.commandLine = tests::Fixtures::words(800);
    _panel->sync();
    screen().ui().relayout();
    screen().frame();

    const BLRect line = resolved().box();
    const BLRect panel = resolved().parent()->parent()->box();
    const double step = screen().type().line_height(screen().type().at(ttk::Typeface::mono, ttk::Theme::fontSmall));

    EXPECT_DOUBLE_EQ(line.h, 4 * step);
    EXPECT_LE(line.y + line.h, panel.y + panel.h);
}

TEST_F(CommandPanelTest, OverridingRepaintsLikeAFullFrame) {
    make();
    screen().full();
    parts::flip(ui(), override());
    sync();
    screen().run(20);

    const parts::Ink off = parts::mismatch(screen());

    EXPECT_EQ(off.pixels, 0U) << off;
}

}
