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
#include <set>
#include <vector>

#include <blend2d/blend2d.h>
#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/StatusIndicator.h"

#include "Pixels.h"
#include "gui/draw/Cards.h"
#include "gui/draw/Hues.h"

namespace {

int brightness(const BLRgba32 colour) {
    return draw::channel(colour.value, 0) + draw::channel(colour.value, 8) + draw::channel(colour.value, 16);
}

std::vector<BLRgba32> tones(const ttk::StatusIndicator::Tones &set) {
    return {set.launching, set.running, set.failing, set.idle};
}

TEST(Cards, ARowIsTheArtAndTheTextUnderIt) {
    EXPECT_DOUBLE_EQ(Cards::rowHeight, Cards::art + 94.0);
    EXPECT_GT(Cards::width, Cards::art);
}

TEST(Cards, TheShelfIsLaidOutInCardsAndRows) {
    const ttk::ReorderGrid::Metrics metrics = Cards::shelfMetrics();

    EXPECT_DOUBLE_EQ(metrics.narrowest, Cards::width);
    EXPECT_DOUBLE_EQ(metrics.row_height, Cards::rowHeight);
    EXPECT_DOUBLE_EQ(metrics.step, Cards::step);
    EXPECT_DOUBLE_EQ(metrics.top, Cards::shelfTop);
    EXPECT_DOUBLE_EQ(metrics.bleed, ttk::Theme::bleed);
    EXPECT_DOUBLE_EQ(metrics.gutter, ttk::Theme::gutter);
}

TEST(Cards, TheArtDarkensTowardsTheBottom) {
    EXPECT_GT(brightness(Cards::artTop), brightness(Cards::artMiddle));
    EXPECT_GT(brightness(Cards::artMiddle), brightness(Cards::artBottom));
}

TEST(Cards, TextOnTheArtStandsOutFromIt) {
    EXPECT_GT(draw::distance(Cards::artText.value, Cards::artMiddle.value), 128);
    EXPECT_GT(draw::distance(Cards::artText.value, Cards::artTop.value), 128);
}

TEST(Cards, EveryToneIsOpaque) {
    for (const BLRgba32 tone : {Cards::artTop,
                                Cards::artMiddle,
                                Cards::artBottom,
                                Cards::artEdge,
                                Cards::artText,
                                Cards::ember,
                                Cards::emberHigh,
                                Cards::steel}) {
        EXPECT_EQ(draw::channel(tone.value, 24), 255) << std::hex << tone.value;
    }
}

TEST(Cards, TheEmberHighlightIsLighterThanTheEmber) {
    EXPECT_GT(brightness(Cards::emberHigh), brightness(Cards::ember));
}

TEST(Cards, TheDragSlackIsSmallerThanAStep) {
    EXPECT_GT(Cards::dragSlack, 0.0);
    EXPECT_LT(Cards::dragSlack, Cards::step);
    EXPECT_GT(Cards::settling, 0.0);
}

TEST(Hues, ThereIsAToneForEveryRunStateAndNoTwoAlike) {
    const std::vector<BLRgba32> all = tones(Hues::statusTones(ttk::Theme::palette(ttk::Theme::Mode::Dark)));
    std::set<std::uint32_t> distinct;

    for (const BLRgba32 tone : all) {
        distinct.insert(tone.value);
    }

    EXPECT_EQ(all.size(), 4U);
    EXPECT_EQ(distinct.size(), 4U);
}

TEST(Hues, TheTonesAreNotNearOneAnother) {
    const std::vector<BLRgba32> all = tones(Hues::statusTones(ttk::Theme::palette(ttk::Theme::Mode::Dark)));

    for (size_t one = 0; one < all.size(); ++one) {
        for (size_t two = one + 1; two < all.size(); ++two) {
            EXPECT_GT(draw::distance(all[one].value, all[two].value), 40) << one << " and " << two;
        }
    }
}

TEST(Hues, TheTonesAreTheSameInLightAndDark) {
    const ttk::StatusIndicator::Tones light = Hues::statusTones(ttk::Theme::palette(ttk::Theme::Mode::Light));
    const ttk::StatusIndicator::Tones dark = Hues::statusTones(ttk::Theme::palette(ttk::Theme::Mode::Dark));

    for (size_t at = 0; at < 4; ++at) {
        EXPECT_EQ(tones(light)[at].value, tones(dark)[at].value) << at;
    }
}

TEST(Hues, TheTonesComeFromTheCardsAndTheDarkPalette) {
    const ttk::StatusIndicator::Tones set = Hues::statusTones(ttk::Theme::palette(ttk::Theme::Mode::Light));

    EXPECT_EQ(set.launching.value, Cards::emberHigh.value);
    EXPECT_EQ(set.idle.value, Cards::steel.value);
    EXPECT_EQ(set.running.value, ttk::Theme::palette(ttk::Theme::Mode::Dark).success.value);
}

}
