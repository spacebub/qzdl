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

#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/overlays/Sheet.h"

class App;

namespace components {

class CommandSheet : public toolkit::Sheet {
public:
    explicit CommandSheet(std::function<void()> copied);

    void sync() override;

    void arrange(Typeface &type) override;

    void paintOver(const toolkit::Painter &painter) override;

private:
    std::function<void()> _copied;

    toolkit::GlyphButton *_shut = nullptr;
    toolkit::Scroll *_scroll = nullptr;
    toolkit::Button *_copy = nullptr;
    toolkit::Button *_close = nullptr;

    std::string _shown;
};

}
