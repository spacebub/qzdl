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

#include <functional>
#include <string>
#include <vector>

#include "gui/app/Shell.h"
#include "gui/model/ConfigBridge.h"
#include "gui/services/Engines.h"
#include "gui/services/FilePicker.h"
#include "gui/services/IwadArt.h"
#include "gui/services/Notifier.h"
#include "gui/services/Runs.h"

// What the views reach for, handed to each one when it is built.
//
// The window owns all of this and fills the struct in; a view holding it needs
// nothing back from the window, so neither has to know the other's type.
struct Reach {
    Shell &shell;
    ConfigBridge &config;
    Notifier &notify;
    Runs &runs;
    FilePicker &picker;
    Engines &engines;
    IwadArt &art;

    // Marks the interface for a sync at the next frame.
    std::function<void()> touch;

    std::function<void(State::Page page)> go;

    // The theme button: system, light, dark and round again.
    std::function<void()> cycleShade;

    // The dialogs, each told what to do rather than leaving an action to look up.
    std::function<void(const std::string &title, const std::string &body,
                       const std::string &accept, bool danger,
                       std::function<void()> accepted)> ask;

    std::function<void(const std::string &title, const std::string &label,
                       const std::string &value, const std::string &accept,
                       std::function<void(const std::string &)> accepted)> prompt;

    std::function<void(const std::string &title, const std::string &kind,
                       const std::vector<std::string> &filters, const std::string &remember,
                       const std::string &name, const std::string &file, bool offerDos,
                       bool dosbox,
                       std::function<void(const std::string &, const std::string &,
                                          bool)> accepted)> edit;

    std::function<void()> showAbout;
    std::function<void()> showCommand;
    std::function<void()> copyConfig;
};
