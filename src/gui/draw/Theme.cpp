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

#include <algorithm>
#include <cmath>

#include "gui/draw/Theme.h"

namespace {

constexpr Theme::Palette DARK{
    .background = BLRgba32{0xff0a0d13},
    .surface = BLRgba32{0xff151b26},
    .raised = BLRgba32{0xff1d2431},
    .sunken = BLRgba32{0xff0e131c},
    .field = BLRgba32{0xff07090e},

    .hover = BLRgba32{0x16ffffff},
    .border = BLRgba32{0xff232c3a},
    .borderStrong = BLRgba32{0xff3b4759},

    .text = BLRgba32{0xffe4eaf5},
    .muted = BLRgba32{0xffa6b4cd},
    .faint = BLRgba32{0xff7a8cac},

    .accent = BLRgba32{0xffff7a1a},
    .accentHover = BLRgba32{0xffff9422},
    .accentText = BLRgba32{0xff1a0a02},
    .accentSoft = BLRgba32{0xff2b1607},

    .mutedSoft = BLRgba32{0xff212a37},
    .success = BLRgba32{0xff52d18b},
    .successSoft = BLRgba32{0xff0f2b1e},
    .warning = BLRgba32{0xffffc44d},
    .warningSoft = BLRgba32{0xff332609},
    .danger = BLRgba32{0xffff5470},
    .dangerSoft = BLRgba32{0xff35121c},

    .shadow = BLRgba32{0xa8000000},
    .scrim = BLRgba32{0xbe030509},

    .artTop = BLRgba32{0xff1d2431},
    .artMiddle = BLRgba32{0xff121722},
    .artBottom = BLRgba32{0xff0a0d13},
    .artEdge = BLRgba32{0xff0b0e14},
    .artText = BLRgba32{0xffe4eaf5},
    .ember = BLRgba32{0xffff5a00},
    .emberDeep = BLRgba32{0xff8f1b06},
    .emberHigh = BLRgba32{0xffff9422},
    .steel = BLRgba32{0xffa6b4cd},
    .artSuccess = BLRgba32{0xff52d18b},
    .artDanger = BLRgba32{0xffff6b80},

    .headingWeight = 700,
    .dark = true,
};

constexpr Theme::Palette LIGHT{
    .background = BLRgba32{0xffe8ecf4},
    .surface = BLRgba32{0xffffffff},
    .raised = BLRgba32{0xffffffff},
    .sunken = BLRgba32{0xffdee4ef},
    .field = BLRgba32{0xffdae1ed},

    .hover = BLRgba32{0x140d1628},
    .border = BLRgba32{0xffccd5e4},
    .borderStrong = BLRgba32{0xffa9b6c9},

    .text = BLRgba32{0xff101620},
    .muted = BLRgba32{0xff46536a},
    .faint = BLRgba32{0xff64728a},

    .accent = BLRgba32{0xffc9450a},
    .accentHover = BLRgba32{0xffa83606},
    .accentText = BLRgba32{0xffffffff},
    .accentSoft = BLRgba32{0xffffe9dc},

    .mutedSoft = BLRgba32{0xffdde4ef},
    .success = BLRgba32{0xff0a7d4e},
    .successSoft = BLRgba32{0xffe0f6ec},
    .warning = BLRgba32{0xff8a5a08},
    .warningSoft = BLRgba32{0xfffcf0d8},
    .danger = BLRgba32{0xffc22a45},
    .dangerSoft = BLRgba32{0xfffde7ec},

    .shadow = BLRgba32{0x5212203a},
    .scrim = BLRgba32{0x780c1420},

    .artTop = DARK.artTop,
    .artMiddle = DARK.artMiddle,
    .artBottom = DARK.artBottom,
    .artEdge = DARK.artEdge,
    .artText = DARK.artText,
    .ember = DARK.ember,
    .emberDeep = DARK.emberDeep,
    .emberHigh = DARK.emberHigh,
    .steel = DARK.steel,
    .artSuccess = DARK.artSuccess,
    .artDanger = DARK.artDanger,

    .headingWeight = 600,
    .dark = false,
};

// Every slot, so a tone kept from one shade can be looked up in the other.
constexpr BLRgba32 Theme::Palette::*SLOTS[] = {
    &Theme::Palette::text,        &Theme::Palette::muted,       &Theme::Palette::faint,
    &Theme::Palette::accent,      &Theme::Palette::accentHover, &Theme::Palette::accentText,
    &Theme::Palette::accentSoft,  &Theme::Palette::success,     &Theme::Palette::successSoft,
    &Theme::Palette::warning,     &Theme::Palette::warningSoft, &Theme::Palette::danger,
    &Theme::Palette::dangerSoft,  &Theme::Palette::mutedSoft,   &Theme::Palette::background,
    &Theme::Palette::surface,     &Theme::Palette::raised,      &Theme::Palette::sunken,
    &Theme::Palette::field,       &Theme::Palette::hover,       &Theme::Palette::border,
    &Theme::Palette::borderStrong, &Theme::Palette::shadow,     &Theme::Palette::scrim,
};

std::string &modeRef() {
    static std::string mode = "system";

    return mode;
}

bool &systemDark() {
    static bool dark = true;

    return dark;
}

}

namespace Theme {

bool dark() {
    const std::string &wanted = modeRef();

    if (wanted == "light") {
        return false;
    }

    if (wanted == "dark") {
        return true;
    }

    return systemDark();
}

const Palette &of() {
    return dark() ? DARK : LIGHT;
}

const std::string &mode() {
    return modeRef();
}

void setMode(const std::string &mode) {
    modeRef() = mode;
}

void setSystemDark(const bool dark) {
    systemDark() = dark;
}

std::string nextMode() {
    const std::string &wanted = modeRef();

    if (wanted == "system") {
        return "light";
    }

    return wanted == "light" ? "dark" : "system";
}

BLRgba32 restated(const BLRgba32 tone, const bool wasDark) {
    if (wasDark == dark()) {
        return tone;
    }

    const Palette &was = wasDark ? DARK : LIGHT;
    const Palette &now = of();

    for (BLRgba32 Palette::*const slot : SLOTS) {
        if ((was.*slot).value == tone.value) {
            return now.*slot;
        }
    }

    return tone;
}

BLRgba32 alpha(const BLRgba32 tone, const double fraction) {
    const double clamped = std::clamp(fraction, 0.0, 1.0);

    return BLRgba32{tone.r(), tone.g(), tone.b(),
                    static_cast<uint32_t>(std::lround(tone.a() * clamped))};
}

BLRgba32 darker(const BLRgba32 tone, const double factor) {
    const double scale = 1.0 / (1.0 + std::max(0.0, factor));

    return BLRgba32{static_cast<uint32_t>(std::lround(tone.r() * scale)),
                    static_cast<uint32_t>(std::lround(tone.g() * scale)),
                    static_cast<uint32_t>(std::lround(tone.b() * scale)), tone.a()};
}

BLRgba32 lighter(const BLRgba32 tone, const double factor) {
    const double scale = 1.0 + std::max(0.0, factor);
    const auto lift = [scale](const uint32_t channel) {
        return static_cast<uint32_t>(std::clamp(std::lround(channel * scale), 0L, 255L));
    };

    return BLRgba32{lift(tone.r()), lift(tone.g()), lift(tone.b()), tone.a()};
}

BLRgba32 mix(const BLRgba32 under, const BLRgba32 over, const double amount) {
    const double weight = std::clamp(amount, 0.0, 1.0) * (over.a() / 255.0);
    // Both sides are widened first: a channel that falls wraps around if the
    // difference is taken between two unsigned ones.
    const auto blend = [weight](const uint32_t below, const uint32_t above) {
        const double from = below;
        const double to = above;

        return static_cast<uint32_t>(
            std::clamp(std::lround(from + ((to - from) * weight)), 0L, 255L));
    };

    return BLRgba32{blend(under.r(), over.r()), blend(under.g(), over.g()),
                    blend(under.b(), over.b()), under.a()};
}

}
