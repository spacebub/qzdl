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
#pragma once

#include "gui/state/State.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/controls/StatusIndicator.h"

// The one place the state tree's vocabulary meets the toolkit's. The toolkit cannot
// see State, so the two sets of names are kept in step here and nowhere else; the
// asserts fail the build if either grows a member the other has not.
namespace components {

constexpr toolkit::Pill::Kind kindOf(const State::BadgeKind kind) {
    using Shown = toolkit::Pill::Kind;

    static_assert(static_cast<int>(State::BadgeKind::None) == static_cast<int>(Shown::None));
    static_assert(static_cast<int>(State::BadgeKind::Muted) == static_cast<int>(Shown::Muted));
    static_assert(static_cast<int>(State::BadgeKind::Success) == static_cast<int>(Shown::Success));
    static_assert(static_cast<int>(State::BadgeKind::Warning) == static_cast<int>(Shown::Warning));
    static_assert(static_cast<int>(State::BadgeKind::Danger) == static_cast<int>(Shown::Danger));

    return static_cast<Shown>(kind);
}

constexpr toolkit::StatusIndicator::Status statusOf(const State::RunState status) {
    using Shown = toolkit::StatusIndicator::Status;

    static_assert(static_cast<int>(State::RunState::None) == static_cast<int>(Shown::Empty));
    static_assert(static_cast<int>(State::RunState::Launching) == static_cast<int>(Shown::Launching));
    static_assert(static_cast<int>(State::RunState::Running) == static_cast<int>(Shown::Running));
    static_assert(static_cast<int>(State::RunState::Stopping) == static_cast<int>(Shown::Stopping));
    static_assert(static_cast<int>(State::RunState::Closed) == static_cast<int>(Shown::Closed));
    static_assert(static_cast<int>(State::RunState::Failed) == static_cast<int>(Shown::Failed));

    return static_cast<Shown>(status);
}

inline BLRgba32 toneOf(const State::BadgeKind kind) {
    return toolkit::Pill::toneOf(kindOf(kind));
}

inline BLRgba32 washOf(const State::BadgeKind kind) {
    return toolkit::Pill::washOf(kindOf(kind));
}

}
