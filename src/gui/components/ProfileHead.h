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

#include <string>

#include "gui/components/ProfileChooser.h"
#include "gui/components/Reach.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/layout/Box.h"

namespace components {

// The row across the top of the profile page: the chooser, the buttons beside
// it, and the menu under the cog.
class ProfileHead : public toolkit::Box {
public:
    explicit ProfileHead(Reach *reach);

    void sync() const;

private:
    void showMenu() const;

    // What the chooser says under the name.
    [[nodiscard]] static std::string summary();

    Reach *_reach;

    ProfileChooser *_chooser = nullptr;
    toolkit::GlyphButton *_terminal = nullptr;
    toolkit::GlyphButton *_cog = nullptr;
    toolkit::Button *_launch = nullptr;
};

}
