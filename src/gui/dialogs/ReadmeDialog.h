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
#include <string>
#include <vector>

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/TextView.h"
#include "ttk/toolkit/layout/Panel.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/overlays/Dialog.h"

#include "core/wad/Readmes.h"

namespace dialogs {

// The text files of an add-on: the one there is, or a list to pick from.
class ReadmeDialog : public ttk::Dialog {
public:
    ReadmeDialog(std::string addon, std::vector<Readmes::File> texts);

    void arrange(ttk::Typeface &type) override;

protected:
    void paint_over(const ttk::Painter &painter) override;

private:
    class Rows;

    static constexpr size_t LIST = static_cast<size_t>(-1);

    void show(size_t which);

    std::string _addon;
    std::vector<Readmes::File> _texts;
    size_t _shown = LIST;

    ttk::GlyphButton *_shut = nullptr;
    ttk::Panel *_well = nullptr;
    ttk::Scroll *_scroll = nullptr;
    ttk::TextView *_view = nullptr;
    Rows *_rows = nullptr;
    ttk::Button *_back = nullptr;
    ttk::Button *_close = nullptr;
};

}
