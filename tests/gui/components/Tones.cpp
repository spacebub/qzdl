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

#include <utility>

#include <gtest/gtest.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Pill.h"
#include "ttk/toolkit/controls/StatusIndicator.h"

#include "gui/components/Tones.h"
#include "gui/state/State.h"

namespace {

using Badge = State::BadgeKind;
using Kind = ttk::Pill::Kind;
using Run = State::RunState;
using Shown = ttk::StatusIndicator::Status;

constexpr std::pair<Badge, Kind> KINDS[] = {
        {Badge::None, Kind::None},
        {Badge::Muted, Kind::Muted},
        {Badge::Success, Kind::Success},
        {Badge::Warning, Kind::Warning},
        {Badge::Danger, Kind::Danger},
};

constexpr std::pair<Run, Shown> STATES[] = {
        {Run::None, Shown::Empty},
        {Run::Launching, Shown::Launching},
        {Run::Running, Shown::Running},
        {Run::Stopping, Shown::Stopping},
        {Run::Closed, Shown::Closed},
        {Run::Failed, Shown::Failed},
};

static_assert(components::kindOf(Badge::Danger) == Kind::Danger);
static_assert(components::statusOf(Run::Running) == Shown::Running);

class TonesTest : public ::testing::Test {
protected:
    void TearDown() override { ttk::Theme::set_mode(ttk::Theme::Mode::System); }
};

TEST(Tones, EveryBadgeKindIsThePillKindOfTheSameName) {
    for (const auto &[badge, kind] : KINDS) {
        EXPECT_EQ(components::kindOf(badge), kind) << static_cast<int>(badge);
    }
}

TEST(Tones, EveryRunStateIsTheIndicatorStatusOfTheSameName) {
    for (const auto &[run, shown] : STATES) {
        EXPECT_EQ(components::statusOf(run), shown) << static_cast<int>(run);
    }
}

TEST(Tones, ANoneBadgeIsTheAccent) {
    EXPECT_EQ(components::toneOf(Badge::None).value, ttk::Theme::palette().accent.value);
}

TEST(Tones, ADangerBadgeIsDanger) {
    EXPECT_EQ(components::toneOf(Badge::Danger).value, ttk::Theme::palette().danger.value);
    EXPECT_EQ(components::washOf(Badge::Danger).value, ttk::Theme::palette().dangerSoft.value);
}

TEST(Tones, ToneAndWashAreThePills) {
    for (const auto &[badge, kind] : KINDS) {
        EXPECT_EQ(components::toneOf(badge).value, ttk::Pill::tone_of(kind).value) << static_cast<int>(badge);
        EXPECT_EQ(components::washOf(badge).value, ttk::Pill::wash_of(kind).value) << static_cast<int>(badge);
    }
}

TEST(Tones, EveryKindHasAWashApartFromItsTone) {
    for (const auto &[badge, kind] : KINDS) {
        EXPECT_NE(components::toneOf(badge).value, components::washOf(badge).value) << static_cast<int>(badge);
    }
}

TEST_F(TonesTest, TonesFollowTheShade) {
    ttk::Theme::set_mode(ttk::Theme::Mode::Light);

    EXPECT_EQ(components::washOf(Badge::Muted).value, ttk::Theme::palette().mutedSoft.value);

    ttk::Theme::set_mode(ttk::Theme::Mode::Dark);

    EXPECT_EQ(components::washOf(Badge::Muted).value, ttk::Theme::palette().mutedSoft.value);
    EXPECT_EQ(components::toneOf(Badge::Success).value, ttk::Theme::palette().success.value);
}

}
