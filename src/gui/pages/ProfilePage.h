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

#include "gui/components/AddonsPanel.h"
#include "gui/components/CommandPanel.h"
#include "gui/components/NetPanel.h"
#include "gui/components/ProfileHead.h"
#include "gui/components/Reach.h"
#include "gui/components/ReplayPanel.h"
#include "gui/components/RunPanel.h"
#include "gui/components/SavesPanel.h"
#include "gui/toolkit/layout/Box.h"

namespace pages {

// What one profile launches, and everything that hangs off it.
class ProfilePage : public toolkit::Widget {
public:
    explicit ProfilePage(Reach *reach);

    void sync() const;

private:
    Reach *_reach;

    components::ProfileHead *_head = nullptr;
    components::AddonsPanel *_addons = nullptr;
    components::RunPanel *_run = nullptr;
    components::ReplayPanel *_replay = nullptr;
    components::SavesPanel *_saves = nullptr;
    components::NetPanel *_net = nullptr;
    components::CommandPanel *_command = nullptr;

    // Shown instead of everything when the config holds no profiles.
    toolkit::Box *_none = nullptr;
};

}
