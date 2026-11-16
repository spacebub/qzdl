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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ttk/toolkit/controls/Check.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/layout/Box.h"
#include "ttk/toolkit/layout/Wrap.h"
#include "ttk/toolkit/overlays/Dialog.h"

#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/util/Sparse.h"

namespace dialogs {

// The flag words of one family as check boxes, in groups, for the port the profile runs on.
class FlagsDialog : public ttk::Dialog {
public:
    FlagsDialog(Flags::Family family,
                const Dialect::Port &port,
                const Sparse<Flags::Word> &typed,
                std::function<void(Flags::Edits)> accepted);

    void opened() override;

    void arrange(ttk::Typeface &type) override;

private:
    // What Save writes back for a word.
    enum class Edit : std::uint8_t {
        none,
        number,

        // Clear all leaves the word to the port.
        cleared,
    };

    struct Held {
        std::string typed;
        std::uint32_t value;
        Flags::Word word;
        Edit edit{Edit::none};
    };

    struct Control {
        const Flags::Option *option{nullptr};
        ttk::Check *check{nullptr};
        ttk::MultistateSwitch *choice{nullptr};
        size_t held{0};
    };

    void hold(Flags::Family family, const Sparse<Flags::Word> &typed);
    void addOptions(ttk::Box *groups);
    void addCheck(ttk::Wrap *rows, const Flags::Option &option, size_t held);
    void addChoice(ttk::Wrap *rows, const Flags::Option &option, size_t held);
    void addFooter(ttk::Box *column, Flags::Family family);

    void assign(size_t held, std::uint32_t value);
    void show(size_t held) const;
    void clear();
    void describe() const;
    void commit() const;

    Dialect::Port _port;
    std::function<void(Flags::Edits)> _accepted;

    std::vector<Held> _held;
    std::vector<Control> _controls;

    ttk::Label *_values = nullptr;
    ttk::Label *_unknown = nullptr;
};

}
