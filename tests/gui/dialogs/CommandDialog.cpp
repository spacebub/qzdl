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
#include <vector>

#include <gtest/gtest.h>

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/TextView.h"

#include "Dialogs.h"
#include "gui/dialogs/CommandDialog.h"
#include "gui/state/State.h"

namespace {

using dialogs::CommandDialog;

class CommandDialogTest : public modal::Dialogued {
protected:
    int copied = 0;

    CommandDialog *dialog = nullptr;
    ttk::TextView *view = nullptr;
    ttk::Button *copy = nullptr;
    ttk::Button *close = nullptr;
    ttk::GlyphButton *shut = nullptr;

    void show() {
        dialog = open(std::make_unique<CommandDialog>([this] { ++copied; }));

        const std::vector<ttk::TextView *> views = modal::all<ttk::TextView>(dialog);
        const std::vector<ttk::Button *> buttons = modal::all<ttk::Button>(dialog);
        const std::vector<ttk::GlyphButton *> glyphs = modal::all<ttk::GlyphButton>(dialog);

        ASSERT_EQ(views.size(), 1U);
        ASSERT_EQ(buttons.size(), 2U) << "Copy and Close";
        ASSERT_EQ(glyphs.size(), 1U);

        view = views.front();
        copy = buttons[0];
        close = buttons[1];
        shut = glyphs.front();
    }
};

TEST_F(CommandDialogTest, ItShowsTheCommandLine) {
    State::get().cfg.commandLine = "/ports/gzdoom -iwad /games/doom2.wad";

    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_EQ(modal::read(view), "/ports/gzdoom -iwad /games/doom2.wad");
}

TEST_F(CommandDialogTest, ALongLineIsFoldedToTheView) {
    std::string line = "/ports/gzdoom";

    for (int at = 0; at < 40; ++at) {
        line += " -file /addons/a-long-name-" + std::to_string(at) + ".wad";
    }

    State::get().cfg.commandLine = line;

    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_GT(view->rows(), 1U);
    EXPECT_EQ(modal::read(view), line);
}

TEST_F(CommandDialogTest, CopyIsOnForALine) {
    State::get().cfg.commandLine = "/ports/gzdoom";

    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_TRUE(copy->enabled());
}

TEST_F(CommandDialogTest, WithNoPortItSaysSoAndCopiesNothing) {
    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_EQ(modal::read(view), "Nothing to launch yet: no source port is selected.");
    EXPECT_FALSE(copy->enabled());
}

TEST_F(CommandDialogTest, ADosPortWithNoDosboxSaysWhy) {
    State::get().cfg.dosPort = true;

    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_TRUE(modal::read(view).contains("there is no DOSBox on this machine")) << modal::read(view);
}

TEST_F(CommandDialogTest, ADosPortWithADosboxSetIsJustUnset) {
    State::get().cfg.dosPort = true;
    State::get().cfg.dosbox = "/opt/dosbox";

    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_EQ(modal::read(view), "Nothing to launch yet: no source port is selected.");
}

TEST_F(CommandDialogTest, ADosboxFoundOnTheMachineIsEnough) {
    State::get().cfg.dosPort = true;
    State::get().cfg.systemDosbox = "/usr/bin/dosbox";

    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_FALSE(modal::read(view).contains("DOSBox"));
}

TEST_F(CommandDialogTest, ALineArrivingWhileOpenIsShown) {
    ASSERT_NO_FATAL_FAILURE(show());

    State::get().cfg.commandLine = "/ports/woof -iwad /games/doom2.wad";
    turn();

    EXPECT_EQ(modal::read(view), "/ports/woof -iwad /games/doom2.wad");
    EXPECT_TRUE(copy->enabled());
}

TEST_F(CommandDialogTest, CloseTakesItDown) {
    ASSERT_NO_FATAL_FAILURE(show());

    modal::click(canvas.ui(), close);

    EXPECT_FALSE(layer->covered());
    EXPECT_EQ(copied, 0);
}

TEST_F(CommandDialogTest, TheCrossTakesItDown) {
    ASSERT_NO_FATAL_FAILURE(show());

    modal::click(canvas.ui(), shut);

    EXPECT_FALSE(layer->covered());
}

TEST_F(CommandDialogTest, ADisabledCopyAnswersNothing) {
    ASSERT_NO_FATAL_FAILURE(show());

    modal::click(canvas.ui(), copy);

    EXPECT_EQ(copied, 0);
    EXPECT_TRUE(layer->covered());
}

TEST_F(CommandDialogTest, TheButtonsSitAlongTheBottomRight) {
    ASSERT_NO_FATAL_FAILURE(show());

    const BLRect card = dialog->card()->box();

    EXPECT_LT(copy->box().x, close->box().x);
    EXPECT_DOUBLE_EQ(copy->box().y, close->box().y);
    EXPECT_NEAR(close->box().x + close->box().w, card.x + card.w - 22.0, 0.5);
    EXPECT_GT(close->box().y, view->box().y);
}

TEST_F(CommandDialogTest, TheCrossSitsTopRight) {
    ASSERT_NO_FATAL_FAILURE(show());

    const BLRect card = dialog->card()->box();

    EXPECT_GT(shut->box().x, card.x + (card.w / 2.0));
    EXPECT_LT(shut->box().y, card.y + 60.0);
}

}
