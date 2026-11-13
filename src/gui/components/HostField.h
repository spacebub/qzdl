/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include "ttk/toolkit/controls/TextBox.h"
#include "ttk/toolkit/layout/Box.h"

namespace components {

// A host and a port in one frame, drawn as a field is. A host typed as host:port is split over the two.
class HostField : public ttk::Box {
public:
    using Edited = std::function<void(const std::string &)>;

    HostField(std::string caption, Edited host, Edited port);

    // Leaves a box alone while it already holds the text, so typing is not undone.
    void set(const std::string &host, const std::string &port) const;

    [[nodiscard]] ttk::TextBox *host() const { return _host; }
    [[nodiscard]] ttk::TextBox *port() const { return _port; }

    bool advance(double now) override;
    void arrange(ttk::Typeface &type) override;
    void paint(const ttk::Painter &painter) override;

private:
    // IPv6 keeps its colons: only a single one divides host and port.
    void split();

    Edited _hostEdited;
    Edited _portEdited;

    ttk::Box *_row = nullptr;
    ttk::TextBox *_host = nullptr;
    ttk::TextBox *_port = nullptr;

    BLRect _frame{};
    double _divider = 0.0;
    double _colon = 0.0;
    bool _splitting = false;
};

}
