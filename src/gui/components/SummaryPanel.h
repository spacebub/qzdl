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
#include <string_view>

#include "ttk/toolkit/layout/Collapsible.h"

namespace components {

// A profile page panel with a summary in its heading, which it works out on every sync.
class SummaryPanel : public ttk::CollapsiblePanel {
public:
    using CollapsiblePanel::CollapsiblePanel;

    // set_said repaints the heading even when nothing changed, so it is only called on a change.
    void summarise(const std::string_view text, const bool warning) {
        if (_summarised && _warning == warning && _summary == text) {
            return;
        }

        _summarised = true;
        _warning = warning;
        _summary = text;

        set_said(text, warning);
    }

private:
    std::string _summary;
    bool _warning{false};
    bool _summarised{false};
};

}
