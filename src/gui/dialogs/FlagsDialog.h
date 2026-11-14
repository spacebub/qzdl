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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ttk/toolkit/controls/Check.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/overlays/Dialog.h"

#include "core/launch/Flags.h"

namespace dialogs {

// The flag words of one family as check boxes, in groups, for the port the profile runs on.
class FlagsDialog : public ttk::Dialog {
public:
    // Only the words that changed, each as it goes back into the field. Empty leaves the word to the port.
    using Edited = std::vector<std::pair<Flags::Word, std::string>>;

    FlagsDialog(Flags::Family family,
                PortLine line,
                const std::array<std::string, Flags::WORDS> &typed,
                std::function<void(Edited)> accepted);

    void opened() override;

    void arrange(ttk::Typeface &type) override;

private:
    struct Held {
        Flags::Word word;
        std::string typed;
        std::uint32_t value;

        // A box was changed, so the value goes back as a number.
        bool touched{false};

        // Clear all asked for the word to be left to the port.
        bool cleared{false};
    };

    struct Control {
        const Flags::Option *option{nullptr};
        size_t held{0};
        ttk::Check *check{nullptr};
        ttk::MultistateSwitch *choice{nullptr};
    };

    void assign(size_t held, std::uint32_t value);
    void show(size_t held) const;
    void clear();
    void describe() const;
    void commit() const;

    PortLine _line;
    std::function<void(Edited)> _accepted;

    std::vector<Held> _held;
    std::vector<Control> _controls;

    ttk::Label *_values = nullptr;
    ttk::Label *_unknown = nullptr;
};

}
