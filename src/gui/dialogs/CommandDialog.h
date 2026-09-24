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

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/TextView.h"
#include "ttk/toolkit/layout/Panel.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/overlays/Dialog.h"

namespace dialogs {

class CommandDialog : public ttk::Dialog {
public:
    explicit CommandDialog(std::function<void()> copied);

    void sync() override;

    void arrange(ttk::Typeface &type) override;

protected:
    void paint_over(const ttk::Painter &painter) override;

private:
    std::function<void()> _copied;

    ttk::GlyphButton *_shut = nullptr;
    ttk::Panel *_well = nullptr;
    ttk::Scroll *_scroll = nullptr;
    ttk::TextView *_view = nullptr;
    ttk::Button *_copy = nullptr;
    ttk::Button *_close = nullptr;

    std::string _shown;
};

}
