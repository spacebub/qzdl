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

#include <array>
#include <string>

#include "ttk/toolkit/Widget.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Wrap.h"

#include "core/config/Options.h"
#include "core/launch/Flags.h"
#include "gui/components/Reach.h"
#include "gui/components/SummaryPanel.h"

namespace components {

// The rules the port plays the profile by: the options, the complevel and the flag words.
class GameplayPanel : public SummaryPanel {
public:
    explicit GameplayPanel(Reach *reach);

    void sync();

private:
    void buildRules(ttk::Box *body);
    void buildCompatibility(ttk::Box *body);
    void buildWords();

    void syncRules();
    void syncCompatibility();
    void syncWords();

    [[nodiscard]] static std::string summary();

    Reach *_reach;

    ttk::Box *_rules = nullptr;
    ttk::Wrap *_switches = nullptr;

    // The option numbers, then the gameplay words.
    ttk::Wrap *_entries = nullptr;

    ttk::Box *_compatibility = nullptr;
    ttk::Select *_level = nullptr;
    ttk::Label *_needed = nullptr;
    ttk::Wrap *_compatibilityEntries = nullptr;
    ttk::Label *_kept = nullptr;

    std::array<ttk::Field *, Flags::WORDS> _wordFields{};
    std::array<bool, Flags::WORDS> _wordBad{};

    // A toggle or a field for each rule, as its kind says. Null for the other options.
    std::array<ttk::Widget *, Options::ALL.size()> _options{};

    std::array<std::string, Options::ALL.size()> _badges{};
};

}
