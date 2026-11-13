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

#include <algorithm>
#include <cmath>
#include <utility>

#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/Painter.h"
#include "ttk/toolkit/Root.h"
#include "ttk/toolkit/controls/Label.h"

#include "gui/components/HostField.h"

using namespace ttk;

namespace components {

namespace {

// Room for a five digit port in the mono face.
constexpr double PORT_WIDE = 64.0;

constexpr double INSET = 12.0;

}

HostField::HostField(std::string caption, Edited host, Edited port)
    : Box(Flow::Column), _hostEdited(std::move(host)), _portEdited(std::move(port)) {
    spacing(6.0);

    minWidth = 240.0;

    append(std::make_unique<Label>(std::move(caption)))->section();

    _row = append(Box::row());
    _row->fixedHeight = Theme::control;

    // Split on the next frame: the box is still inside its own edit here.
    _host = _row->append(std::make_unique<TextBox>([this](const std::string &text) {
        if (_hostEdited) {
            _hostEdited(text);
        }

        if (text.contains(':')) {
            _splitting = true;
            wake();
        }
    }));

    _host->placeholder("A host name or an address")->mono();

    _port = _row->append(std::make_unique<TextBox>([this](const std::string &text) {
        if (_portEdited) {
            _portEdited(text);
        }
    }));

    _port->placeholder("Default")->mono();
    _port->hint = "Port. The default one when left empty";
}

void HostField::set(const std::string &host, const std::string &port) const {
    if (_host->text() != host) {
        _host->set_text(host);
    }

    if (_port->text() != port) {
        _port->set_text(port);
    }
}

bool HostField::advance(const double /*now*/) {
    if (_splitting) {
        _splitting = false;
        split();
    }

    return false;
}

void HostField::split() {
    const std::string text = _host->text();
    const size_t colon = text.find(':');

    if (colon == std::string::npos || text.find(':', colon + 1) != std::string::npos) {
        return;
    }

    const std::string host = text.substr(0, colon);
    const std::string port = text.substr(colon + 1);

    _host->set_text(host);

    if (_hostEdited) {
        _hostEdited(host);
    }

    // A colon typed on its own moves on to the port without wiping it.
    if (!port.empty()) {
        _port->set_text(port);

        if (_portEdited) {
            _portEdited(port);
        }
    }

    if (root() != nullptr) {
        root()->focus(_port);
    }

    _port->select_all();
}

void HostField::arrange(Typeface &type) {
    Box::arrange(type);

    const BLRect row = _row->box();
    const double inner = Theme::control - 12.0;
    const double top = row.y + ((row.h - inner) / 2.0);

    _colon = type.width(type.at(Typeface::mono, Theme::fontBody), ":");
    _frame = row;

    // On a whole pixel, or the rule smears across two at partial strength.
    _divider = std::round(_frame.x + _frame.w - (INSET + PORT_WIDE + 6.0 + _colon + 9.0));

    _host->place(BLRect{_frame.x + INSET, top, std::max(0.0, _divider - 9.0 - (_frame.x + INSET)), inner}, type);
    _port->place(BLRect{_frame.x + _frame.w - INSET - PORT_WIDE, top, PORT_WIDE, inner}, type);
}

void HostField::paint(const Painter &painter) {
    const Theme::Palette &palette = Theme::palette();
    const bool focused = _host->focused() || _port->focused();
    const BLFont &face = painter.font(Typeface::mono, Theme::fontBody);

    painter.round(_frame, Theme::radiusSmall, palette.field);
    painter.outline(_frame, Theme::radiusSmall, 1.0, focused ? palette.accent : palette.borderStrong);
    painter.fill(BLRect{_divider, _frame.y + 6.0, 1.0, _frame.h - 12.0}, palette.borderStrong);
    painter.label(face, BLRect{_divider + 9.0, _frame.y, _colon + 2.0, _frame.h}, Align::Start, ":", palette.faint);

    Box::paint(painter);
}

}
