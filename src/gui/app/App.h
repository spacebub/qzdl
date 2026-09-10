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
#include <vector>

#include "main.h"
#include "gui/bridge/ConfigBridge.h"
#include "gui/components/IwadArt.h"
#include "gui/components/Notifier.h"
#include "gui/components/Runs.h"
#include "gui/views/Engines.h"
#include "gui/views/Picker.h"

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

    void persist();

    void picked(const std::string &action, const std::vector<std::string> &paths, bool option);

    static constexpr size_t HISTORY = 24;

    slint::ComponentHandle<ui::Zdl> _window;

    IwadArt _art;
    Notifier _notifier;
    Runs _runs;
    ConfigBridge _config;
    Picker _picker;
    Engines _engines;

    std::vector<std::string> _history;
    std::vector<std::string> _ahead;
};
