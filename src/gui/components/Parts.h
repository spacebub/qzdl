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

#include <cstddef>
#include <memory>
#include <string>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Box.h"

#include "gui/model/ConfigBridge.h"
#include "gui/state/State.h"

// Page pieces more than one page builds.
namespace components {

// A monitor on its stand, drawn as the built-in glyphs are.
inline ttk::Glyphs::Glyph shortcutGlyph() {
    static const ttk::Glyphs::Glyph glyph = ttk::Glyphs::add({
            .outline = "M 3 2 L 11 2 A 1.5 1.5 0 0 1 12.5 3.5 L 12.5 8.5 A 1.5 1.5 0 0 1 11 10 L 3 10"
                       " A 1.5 1.5 0 0 1 1.5 8.5 L 1.5 3.5 A 1.5 1.5 0 0 1 3 2 Z"
                       " M 7 10 L 7 12.4 M 4.4 12.4 L 9.6 12.4",
            .solid = {},
            .box = 14.0F,
            .solidBox = 14.0F,
            .pen = 1.4F,
    });

    return glyph;
}

// Reads the text files of a row.
inline ttk::Glyphs::Glyph eyeGlyph() {
    static const ttk::Glyphs::Glyph glyph = ttk::Glyphs::add({
            .outline = "M 1.5 7 C 3.5 3.2 10.5 3.2 12.5 7 C 10.5 10.8 3.5 10.8 1.5 7 Z",
            .solid = "M 5 7 A 2 2 0 1 0 9 7 A 2 2 0 1 0 5 7 Z",
            .box = 14.0F,
            .solidBox = 14.0F,
            .pen = 1.4F,
    });

    return glyph;
}

inline ttk::Label *panelTitle(ttk::Box *into, const std::string &text) {
    ttk::Label *made = into->append(std::make_unique<ttk::Label>(text));

    made->font(ttk::Theme::palette().headingWeight, ttk::Theme::fontMedium)->tone(&ttk::Theme::Palette::text);

    return made;
}

inline std::unique_ptr<ttk::Toggle> optionToggle(ConfigBridge &config, const Options::Option &option) {
    return std::make_unique<ttk::Toggle>(std::string(option.label), [&config, &option](const bool on) {
        config.panels().setOption(option.feature, on);
    });
}

// Shows the control of the option where the port takes it, with what it does there, and checks a toggle.
inline void syncOption(ttk::Widget *control, const std::size_t at) {
    const State::Cfg &cfg = State::get().cfg;
    const State::OptionField &state = cfg.optionFields[at];

    control->set_visible(cfg.features.has(Options::ALL[at].feature));

    if (control->hint != state.hint) {
        control->hint = state.hint;
    }

    if (Options::ALL[at].kind == Options::Kind::toggle) {
        static_cast<ttk::Toggle *>(control)->set_checked(state.on);
    }
}

// "1 profile", "4 profiles".
inline std::string say(const size_t number, const std::string &thing) {
    return std::to_string(number) + " " + thing + (number == 1 ? "" : "s");
}

}
