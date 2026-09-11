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

#include "gui/toolkit/Widget.h"

namespace components {

class TitleBar;
class LogDock;

// The window layout: the bar across the top, the page centred under it,
// and the run dock along the bottom when anything is docked.
class Frame : public toolkit::Widget {
public:
    Frame(TitleBar *bar, toolkit::Widget *pages, LogDock *dock)
        : _bar(bar), _pages(pages), _dock(dock) {}

    void arrange(Typeface &type) override;

private:
    TitleBar *_bar;
    toolkit::Widget *_pages;
    LogDock *_dock;
};

}
