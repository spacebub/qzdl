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
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Panel.h"

#include "gui/components/Reach.h"

namespace components {

// The port, the game and the map, and how the game is played on them.
class RunPanel : public ttk::Panel {
public:
    static constexpr double WIDTH = 340.0;

    explicit RunPanel(Reach *reach);

    void sync() const;

private:
    Reach *_reach;

    ttk::Button *_addPort = nullptr;
    ttk::Select *_port = nullptr;
    ttk::Button *_addGame = nullptr;
    ttk::Select *_iwad = nullptr;
    ttk::Select *_map = nullptr;
    ttk::Select *_skill = nullptr;
    ttk::Select *_monsters = nullptr;
    ttk::Toggle *_capture = nullptr;
    ttk::Box *_dosPair = nullptr;
    ttk::Toggle *_fullscreen = nullptr;
    ttk::Toggle *_exit = nullptr;
    ttk::Toggle *_levelstat = nullptr;
    ttk::Toggle *_sharedConfig = nullptr;
    ttk::Fact *_directory = nullptr;
};

}
