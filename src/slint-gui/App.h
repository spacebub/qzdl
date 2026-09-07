/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
#include <vector>

#include "main.h"
#include "slint-gui/Browse.h"
#include "slint-gui/ConfigBridge.h"
#include "slint-gui/IwadArt.h"
#include "slint-gui/Notifier.h"
#include "slint-gui/Picker.h"
#include "slint-gui/Runs.h"

class App {
public:
    App();

    void run();

private:
    void bindSystem();
    void bindTheme();

    void go(const std::string &page);
    void back();
    void forward();

    void restoreGeometry() const;
    void rememberGeometry() const;

    // Everything that has to reach the disk before the window goes away.
    void persist();

    // Dispatched on the action the pick was started with.
    void picked(const std::string &action, const std::vector<std::string> &paths, bool option);

    // Long enough to walk back through a session, short enough not to grow all day.
    static constexpr size_t HISTORY = 24;

    slint::ComponentHandle<ui::Zdl> _window;

    IwadArt _art;
    Notifier _notifier;
    Runs _runs;
    ConfigBridge _config;
    Picker _picker;
    Browse _browse;

    std::vector<std::string> _history;
    std::vector<std::string> _ahead;
};
