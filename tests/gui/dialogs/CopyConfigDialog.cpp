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
#include "ttk/toolkit/layout/Scroll.h"

#include "Dialogs.h"
#include "gui/dialogs/CopyConfigDialog.h"
#include "gui/state/State.h"

namespace {

using dialogs::CopyConfigDialog;

constexpr double ROW = 46.0;

class CopyConfigDialogTest : public modal::Dialogued {
protected:
    std::vector<std::string> picked;

    CopyConfigDialog *dialog = nullptr;
    ttk::Scroll *rows = nullptr;
    ttk::Button *cancel = nullptr;
    ttk::Button *copy = nullptr;

    static void donors(const int count) {
        for (int at = 0; at < count; ++at) {
            State::get().cfg.configDonors.push_back(State::ConfigDonor{
                    .id = "id-" + std::to_string(at),
                    .name = "Donor " + std::to_string(at),
                    .file = "/data/profiles/donor" + std::to_string(at) + "/donor.cfg",
                    .shared = at == 1,
            });
        }
    }

    void show() {
        State::get().cfg.port = "GZDoom";

        dialog = open(std::make_unique<CopyConfigDialog>([this](const std::string &id) { picked.push_back(id); }));

        const std::vector<ttk::Scroll *> scrolls = modal::all<ttk::Scroll>(dialog);
        const std::vector<ttk::Button *> buttons = modal::all<ttk::Button>(dialog);

        ASSERT_EQ(scrolls.size(), 1U);
        ASSERT_EQ(buttons.size(), 2U);

        rows = scrolls.front();
        cancel = buttons[0];
        copy = buttons[1];
    }

    void pick(const int row) {
        modal::click(canvas.ui(), rows->box().x + 40.0, rows->box().y + (row * ROW) + (ROW / 2.0));
        canvas.frame();
    }
};

TEST_F(CopyConfigDialogTest, NothingIsPickedAtFirst) {
    donors(2);

    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_FALSE(copy->enabled());
    EXPECT_TRUE(cancel->enabled());
}

TEST_F(CopyConfigDialogTest, PickingARowTurnsCopyOn) {
    donors(3);

    ASSERT_NO_FATAL_FAILURE(show());

    pick(1);

    EXPECT_TRUE(copy->enabled());
}

TEST_F(CopyConfigDialogTest, CopyAnswersThePickedProfile) {
    donors(3);

    ASSERT_NO_FATAL_FAILURE(show());

    pick(2);
    modal::click(canvas.ui(), copy);

    EXPECT_EQ(picked, (std::vector<std::string>{"id-2"}));
    EXPECT_FALSE(layer->covered());
}

TEST_F(CopyConfigDialogTest, TheLastRowPickedWins) {
    donors(3);

    ASSERT_NO_FATAL_FAILURE(show());

    pick(0);
    pick(2);
    pick(1);
    modal::click(canvas.ui(), copy);

    EXPECT_EQ(picked, (std::vector<std::string>{"id-1"}));
}

TEST_F(CopyConfigDialogTest, AClickPastTheLastRowPicksNothing) {
    donors(2);

    ASSERT_NO_FATAL_FAILURE(show());

    pick(4);

    EXPECT_FALSE(copy->enabled());
}

TEST_F(CopyConfigDialogTest, ACopyWithNothingPickedAnswersNothing) {
    donors(2);

    ASSERT_NO_FATAL_FAILURE(show());

    modal::click(canvas.ui(), copy);

    EXPECT_TRUE(picked.empty());
    EXPECT_TRUE(layer->covered());
}

TEST_F(CopyConfigDialogTest, CancelTakesItDownWithoutAnAnswer) {
    donors(2);

    ASSERT_NO_FATAL_FAILURE(show());

    pick(0);
    modal::click(canvas.ui(), cancel);

    EXPECT_TRUE(picked.empty());
    EXPECT_FALSE(layer->covered());
}

TEST_F(CopyConfigDialogTest, APickSurvivesASync) {
    donors(2);

    ASSERT_NO_FATAL_FAILURE(show());

    pick(1);
    turn();

    EXPECT_TRUE(copy->enabled());
}

TEST_F(CopyConfigDialogTest, ThePickedRowIsLit) {
    donors(3);

    ASSERT_NO_FATAL_FAILURE(show());

    canvas.full();

    const int x = static_cast<int>(rows->box().x + 4.0);
    const int y = static_cast<int>(rows->box().y + ROW + (ROW / 2.0));
    const std::uint32_t before = canvas.at(x, y);

    pick(1);
    canvas.ui().leave();
    canvas.full();

    EXPECT_NE(canvas.at(x, y), before);
}

TEST_F(CopyConfigDialogTest, ManyDonorsScroll) {
    donors(30);

    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_TRUE(rows->scrollable());
    EXPECT_DOUBLE_EQ(rows->reach(), 30.0 * ROW);
}

TEST_F(CopyConfigDialogTest, AFewDonorsDoNot) {
    donors(2);

    ASSERT_NO_FATAL_FAILURE(show());

    EXPECT_FALSE(rows->scrollable());
}

TEST_F(CopyConfigDialogTest, ARowScrolledToIsTheOnePicked) {
    donors(30);

    ASSERT_NO_FATAL_FAILURE(show());

    rows->scroll_to(10.0 * ROW);
    canvas.frame();

    pick(0);
    modal::click(canvas.ui(), copy);

    EXPECT_EQ(picked, (std::vector<std::string>{"id-10"}));
}

TEST_F(CopyConfigDialogTest, WithNoDonorsTheListSaysSo) {
    ASSERT_NO_FATAL_FAILURE(show());

    canvas.full();

    const BLRect box = rows->box();
    const std::uint32_t ground =
            canvas.at(static_cast<int>(box.x + box.w - 4.0), static_cast<int>(box.y + box.h - 4.0));

    const modal::Ink said = modal::ink(
            canvas, BLRectI{static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(box.w), 70}, ground);

    EXPECT_GT(said.pixels, 50U);
    EXPECT_FALSE(copy->enabled());
}

}
