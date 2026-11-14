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
#include <string>

#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/TextEdit.h"
#include "ttk/toolkit/layout/Panel.h"
#include "ttk/toolkit/overlays/Dialog.h"

namespace dialogs {

// The custom command with room to read and edit a long one.
class EditCommandDialog : public ttk::Dialog {
public:
    EditCommandDialog(std::string command, std::function<void(const std::string &)> accepted);

    void opened() override;

    void arrange(ttk::Typeface &type) override;

protected:
    void paint_over(const ttk::Painter &painter) override;

private:
    void commit() const;

    std::string _command;
    std::function<void(const std::string &)> _accepted;

    ttk::GlyphButton *_shut = nullptr;
    ttk::Panel *_well = nullptr;
    ttk::TextEdit *_editor = nullptr;
    ttk::Widget *_tokens = nullptr;
    ttk::Button *_cancel = nullptr;
    ttk::Button *_save = nullptr;
};

}
