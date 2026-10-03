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

#include <blend2d/blend2d.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/StatusIndicator.h"

#include "gui/draw/Cards.h"

namespace Hues {

// The status pill sits on a dark tile in both modes, so its tones come from the dark palette in light mode too.
inline ttk::StatusIndicator::Tones statusTones(const ttk::Theme::Palette &) {
    const ttk::Theme::Palette &dark = ttk::Theme::palette(ttk::Theme::Mode::Dark);

    return {.launching = Cards::emberHigh,
            .running = dark.success,
            .failing = BLRgba32{0xffff6b80},
            .idle = Cards::steel};
}

}
