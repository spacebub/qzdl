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

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Fact.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Scroll.h"

#include "gui/components/Reach.h"

namespace pages {

// How launching behaves, where the config is kept, and what has
// been downloaded.
class SettingsPage : public ttk::Widget {
public:
    explicit SettingsPage(Reach *reach);

    void sync();

private:
    // The DOSBox field's badge: none | missing | detected | custom.
    // Where the DOSBox the settings point at came from.
    enum class Dosbox : std::uint8_t {
        None,
        Missing,
        Detected,
        Custom,
    };

    static Dosbox dosboxKind(const std::string &path);

    Reach *_reach;

    ttk::Scroll *_scroll = nullptr;
    ttk::Box *_body = nullptr;

    ttk::Field *_always = nullptr;
    ttk::Field *_dosbox = nullptr;

    ttk::Toggle *_closing = nullptr;
    ttk::Toggle *_paths = nullptr;
    ttk::Toggle *_atOnce = nullptr;
    ttk::Toggle *_perProfile = nullptr;
    ttk::Toggle *_ignoreUser = nullptr;

    ttk::MultistateSwitch *_startView = nullptr;

    ttk::Fact *_configFile = nullptr;
    ttk::Button *_adopt = nullptr;

    ttk::Fact *_downloads = nullptr;
    ttk::Label *_kept = nullptr;
    ttk::Button *_empty = nullptr;

    ttk::Label *_version = nullptr;
    ttk::Label *_blurb = nullptr;

    // The footer's mark.
    BLImage _mark;

    bool _measured = false;
};

}
