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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/TextView.h"

#include "Dialogs.h"
#include "gui/dialogs/AboutDialog.h"
#include "gui/state/State.h"

namespace {

using dialogs::AboutDialog;

class AboutDialogTest : public modal::Dialogued {
protected:
    AboutDialog *dialog = nullptr;

    void show() {
        State::get().sys.version = QZDL_VERSION;
        State::get().sys.runtime = "tinytk 9.9.9";

        dialog = open(std::make_unique<AboutDialog>());
    }

    [[nodiscard]] std::string authors() const {
        const std::vector<ttk::TextView *> views = modal::all<ttk::TextView>(dialog);

        return views.size() == 1 ? modal::read(views.front()) : std::string();
    }

    [[nodiscard]] ttk::GlyphButton *cross() const {
        for (ttk::GlyphButton *each : modal::all<ttk::GlyphButton>(dialog)) {
            if (each->hint.empty()) {
                return each;
            }
        }

        return nullptr;
    }
};

TEST_F(AboutDialogTest, ItIsCalledZdl4) {
    show();

    EXPECT_TRUE(modal::shows(dialog, "ZDL4"));
    EXPECT_TRUE(modal::shows(dialog, "A launcher for Doom engine source ports."));
}

TEST_F(AboutDialogTest, TheVersionLineCarriesTheVersionAndTheToolkit) {
    show();

    EXPECT_TRUE(modal::shows(dialog, std::string("Version ") + QZDL_VERSION + " · tinytk 9.9.9"));
}

TEST_F(AboutDialogTest, TheVersionFollowsTheState) {
    show();

    State::get().sys.version = "9.0.0 (abc1234)";
    turn();

    EXPECT_TRUE(modal::shows(dialog, "Version 9.0.0 (abc1234) · tinytk 9.9.9"));
}

TEST_F(AboutDialogTest, TheCopyrightsAreListed) {
    show();

    EXPECT_TRUE(modal::shows(dialog, "COPYRIGHT"));
    EXPECT_TRUE(modal::shows(dialog, "© 2023-2026 spacebub"));
    EXPECT_TRUE(modal::shows(dialog, "© 2004-2012 ZDL Software Foundation"));
}

TEST_F(AboutDialogTest, TheAuthorsComeFromTheAuthorsFile) {
    show();

    const std::string listed = authors();

    EXPECT_TRUE(listed.contains("spacebub")) << listed;
    EXPECT_TRUE(listed.contains("tinytk")) << listed;
}

TEST_F(AboutDialogTest, TheAuthorsLeaveOutTheTitleAndTheMarkdown) {
    show();

    const std::string listed = authors();

    EXPECT_FALSE(listed.contains("# Authors")) << listed;
    EXPECT_FALSE(listed.contains("## ")) << listed;
    EXPECT_FALSE(listed.contains("- spacebub")) << listed;
}

TEST_F(AboutDialogTest, NestedAuthorsStayIndented) {
    show();

    EXPECT_TRUE(authors().contains("    Ryan \"BioHazard\" Turner")) << authors();
}

TEST_F(AboutDialogTest, CloseTakesItDown) {
    show();

    const std::vector<ttk::Button *> buttons = modal::all<ttk::Button>(dialog);

    ASSERT_EQ(buttons.size(), 1U);

    modal::click(canvas.ui(), buttons.front());

    EXPECT_FALSE(layer->covered());
}

TEST_F(AboutDialogTest, TheCrossTakesItDown) {
    show();

    ASSERT_NE(cross(), nullptr);

    modal::click(canvas.ui(), cross());

    EXPECT_FALSE(layer->covered());
}

TEST_F(AboutDialogTest, TheProjectPageHasAButton) {
    show();

    EXPECT_NE(modal::hinted<ttk::GlyphButton>(dialog, "Project page"), nullptr);
}

TEST_F(AboutDialogTest, TheMarkIsPaintedTopLeft) {
    show();

    canvas.full();

    const BLRect card = dialog->card()->box();
    const BLRectI corner{static_cast<int>(card.x + 22.0), static_cast<int>(card.y + 23.0), 56, 56};
    const std::uint32_t ground = canvas.at(static_cast<int>(card.x + card.w - 6.0), static_cast<int>(card.y + 100.0));

    const modal::Ink painted = modal::ink(canvas, corner, ground);

    EXPECT_GT(painted.pixels, 56U * 56U / 4U);
}

}
