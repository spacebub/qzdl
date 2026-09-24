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
#include "ttk/toolkit/layout/ReorderGrid.h"

namespace Cards {

constexpr BLRgba32 artTop{0xff1d2431};
constexpr BLRgba32 artMiddle{0xff121722};
constexpr BLRgba32 artBottom{0xff0a0d13};
constexpr BLRgba32 artEdge{0xff0b0e14};
constexpr BLRgba32 artText{0xffe4eaf5};
constexpr BLRgba32 ember{0xffff5a00};
constexpr BLRgba32 emberHigh{0xffff9422};
constexpr BLRgba32 steel{0xffa6b4cd};

inline void keepStatusTones() {
    ttk::Theme::Setup setup{
        .dark = ttk::Theme::palette(ttk::Theme::Mode::Dark),
        .light = ttk::Theme::palette(ttk::Theme::Mode::Light),
        .mode = ttk::Theme::mode(),
    };

    for (ttk::Theme::Palette *each : {&setup.dark, &setup.light}) {
        each->statusLaunching = emberHigh;
        each->statusRunning = BLRgba32{0xff52d18b};
        each->statusFailing = BLRgba32{0xffff6b80};
        each->statusIdle = steel;
    }

    ttk::Theme::configure(setup);
}

constexpr double width = 244.0;
constexpr double art = 138.0;

constexpr double step = 8.0;

constexpr double rowHeight = art + 94.0;

constexpr double settling = 0.19;

constexpr double dragSlack = 6.0;
constexpr double shelfTop = 10.0;

inline ttk::ReorderGrid::Metrics shelfMetrics() {
    return {.bleed = ttk::Theme::bleed,
            .gutter = ttk::Theme::gutter,
            .top = shelfTop,
            .narrowest = width,
            .row_height = rowHeight,
            .step = step};
}

}
