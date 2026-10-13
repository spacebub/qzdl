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

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/layout/Box.h"

#include "Mount.h"
#include "gui/components/Parts.h"
#include "support/Canvas.h"

namespace {

constexpr BLRect SPOT{20.0, 20.0, 300.0, 40.0};

TEST(Say, OneIsSingular) {
    EXPECT_EQ(components::say(1, "profile"), "1 profile");
}

TEST(Say, NoneIsPlural) {
    EXPECT_EQ(components::say(0, "profile"), "0 profiles");
}

TEST(Say, ManyArePlural) {
    EXPECT_EQ(components::say(4, "game"), "4 games");
    EXPECT_EQ(components::say(1000, "source port"), "1000 source ports");
}

TEST(PanelTitle, IsAppendedToTheBoxItIsGiven) {
    auto box = ttk::Box::column();

    box->append(std::make_unique<ttk::Label>("before"));

    ttk::Label *made = components::panelTitle(box.get(), "Add-ons");

    ASSERT_EQ(box->children().size(), 2U);
    EXPECT_EQ(box->children().back().get(), made);
    EXPECT_EQ(made->text(), "Add-ons");
}

class PanelTitleTest : public tests::Painted {
protected:
    void SetUp() override {
        tests::Painted::SetUp();

        if (IsSkipped()) {
            return;
        }

        ttk::Theme::set_mode(ttk::Theme::Mode::Light);
    }

    tests::Canvas canvas{340, 80};
};

TEST_F(PanelTitleTest, IsSetInTheMediumHeadingFace) {
    auto box = ttk::Box::column();
    ttk::Label *made = components::panelTitle(box.get(), "Add-ons");

    ttk::Label same("Add-ons");

    same.font(ttk::Theme::palette().headingWeight, ttk::Theme::fontMedium);

    EXPECT_DOUBLE_EQ(made->natural_width(canvas.type()), same.natural_width(canvas.type()));
    EXPECT_DOUBLE_EQ(made->natural_height(canvas.type(), 300.0), same.natural_height(canvas.type(), 300.0));
}

TEST_F(PanelTitleTest, IsWiderThanBodyText) {
    auto box = ttk::Box::column();
    ttk::Label *made = components::panelTitle(box.get(), "Add-ons");

    ttk::Label body("Add-ons");

    EXPECT_GT(made->natural_width(canvas.type()), body.natural_width(canvas.type()));
}

TEST_F(PanelTitleTest, IsWrittenInTheTextTone) {
    auto box = ttk::Box::column();

    components::panelTitle(box.get(), "The run");
    parts::place(canvas, std::move(box), SPOT);
    canvas.full();

    const ttk::Theme::Palette &palette = ttk::Theme::palette();
    const std::uint32_t inked = parts::peak(canvas, parts::pixels_of(SPOT), palette.background.value);

    EXPECT_TRUE(parts::leans(inked, palette.text, palette.faint));
}

}
