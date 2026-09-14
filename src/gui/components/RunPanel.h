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

#include "gui/components/Reach.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Panel.h"

namespace components {

// The port, the game and the map, and how the game is played on them.
class RunPanel : public toolkit::Panel {
public:
    static constexpr double WIDTH = 340.0;

    explicit RunPanel(Reach *reach);

    void sync() const;

private:
    Reach *_reach;

    toolkit::Button *_addPort = nullptr;
    toolkit::Select *_port = nullptr;
    toolkit::Button *_addGame = nullptr;
    toolkit::Select *_iwad = nullptr;
    toolkit::Select *_map = nullptr;
    toolkit::Select *_skill = nullptr;
    toolkit::Select *_monsters = nullptr;
    toolkit::Toggle *_capture = nullptr;
    toolkit::Toggle *_fullscreen = nullptr;
    toolkit::Toggle *_levelstat = nullptr;
    toolkit::Toggle *_sharedConfig = nullptr;
    toolkit::Fact *_directory = nullptr;
};

}
