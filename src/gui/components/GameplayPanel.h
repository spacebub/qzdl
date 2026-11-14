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

#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/Toggle.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Collapsible.h"
#include "ttk/toolkit/layout/Wrap.h"

#include "core/launch/Flags.h"
#include "core/launch/Rules.h"
#include "gui/components/Reach.h"

namespace components {

// The rules the port plays the profile by: pistol start, cheats, the rule switches, the complevel and the flag words.
class GameplayPanel : public ttk::CollapsiblePanel {
public:
    explicit GameplayPanel(Reach *reach);

    void sync();

private:
    [[nodiscard]] static std::string summary();

    Reach *_reach;

    ttk::Box *_rules = nullptr;
    ttk::Wrap *_switches = nullptr;
    ttk::Toggle *_pistolStart = nullptr;
    ttk::Toggle *_cheats = nullptr;
    ttk::Wrap *_gameplayWords = nullptr;

    ttk::Box *_compatibility = nullptr;
    ttk::Select *_level = nullptr;
    ttk::Label *_needed = nullptr;
    ttk::Wrap *_compatibilityWords = nullptr;
    ttk::Label *_kept = nullptr;

    // In Flags::Word order.
    std::array<ttk::Field *, Flags::WORDS> _fields{};
    std::array<bool, Flags::WORDS> _bad{};

    // In Rules::Rule order, a toggle or a field each.
    std::array<ttk::Toggle *, Rules::RULES> _ruleToggles{};
    std::array<ttk::Field *, Rules::RULES> _ruleFields{};
    // The badge each field shows, so it is set only when it changes.
    std::array<std::string, Rules::RULES> _ruleBadges{};
};

}
