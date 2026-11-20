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
#include "ttk/toolkit/controls/TextView.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "Dialogs.h"
#include "core/wad/Readmes.h"
#include "gui/dialogs/ReadmeDialog.h"

namespace {

using dialogs::ReadmeDialog;

class ReadmeDialogTest : public modal::Dialogued {
protected:
    ReadmeDialog *dialog = nullptr;
    ttk::TextView *view = nullptr;
    ttk::Scroll *rows = nullptr;
    ttk::Button *back = nullptr;

    void show(std::vector<Readmes::Text> texts) {
        dialog = open(std::make_unique<ReadmeDialog>("mod.pk3", std::move(texts)));

        view = modal::all<ttk::TextView>(dialog).front();
        rows = modal::all<ttk::Scroll>(dialog).back();
        back = modal::all<ttk::Button>(dialog).front();
    }

    static std::vector<Readmes::Text> two() {
        return {{.name = "mod.txt", .inside = false, .body = "beside"},
                {.name = "credits.txt", .inside = true, .body = "people"}};
    }
};

TEST_F(ReadmeDialogTest, OneTextOpensStraightAway) {
    show({{.name = "mod.txt", .inside = false, .body = "about the mod"}});

    EXPECT_TRUE(modal::seen(view));
    EXPECT_FALSE(modal::seen(rows));
    EXPECT_FALSE(modal::seen(back));
    EXPECT_EQ(modal::read(view), "about the mod");
}

TEST_F(ReadmeDialogTest, SeveralTextsAreListedAndAPickShowsOne) {
    show(two());

    EXPECT_TRUE(modal::seen(rows));
    EXPECT_FALSE(modal::seen(view));

    modal::click(canvas.ui(), rows->box().x + 40.0, rows->box().y + 46.0 + 23.0);
    canvas.frame();

    EXPECT_TRUE(modal::seen(view));
    EXPECT_TRUE(modal::seen(back));
    EXPECT_EQ(modal::read(view), "people");

    modal::click(canvas.ui(), back);
    canvas.frame();

    EXPECT_TRUE(modal::seen(rows));
    EXPECT_FALSE(modal::seen(view));
}

}
