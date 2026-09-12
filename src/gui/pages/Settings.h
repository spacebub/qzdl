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

#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Segmented.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Scroll.h"

class App;

namespace pages {

// How launching behaves, where the config is kept, and what has
// been downloaded.
class SettingsPage : public toolkit::Widget {
public:
    explicit SettingsPage(App *app);

    void sync();

private:
    // The DOSBox field's badge: none | missing | detected | custom.
    static std::string dosboxKind(const std::string &path);

    App *_app;

    toolkit::Scroll *_scroll = nullptr;
    toolkit::Box *_body = nullptr;

    toolkit::Field *_always = nullptr;
    toolkit::Field *_dosbox = nullptr;

    toolkit::Toggle *_closing = nullptr;
    toolkit::Toggle *_paths = nullptr;
    toolkit::Toggle *_atOnce = nullptr;
    toolkit::Toggle *_perProfile = nullptr;
    toolkit::Toggle *_ignoreUser = nullptr;

    toolkit::Segmented *_startView = nullptr;

    toolkit::Fact *_configFile = nullptr;
    toolkit::Button *_adopt = nullptr;

    toolkit::Fact *_downloads = nullptr;
    toolkit::Label *_kept = nullptr;
    toolkit::Button *_empty = nullptr;

    toolkit::Label *_version = nullptr;
    toolkit::Label *_blurb = nullptr;

    // The footer's mark.
    BLImage _mark;

    bool _measured = false;
};

}
