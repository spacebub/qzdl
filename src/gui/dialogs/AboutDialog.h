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

#include "gui/toolkit/overlays/Dialog.h"

namespace dialogs {

class AboutDialog : public toolkit::Dialog {
public:
    AboutDialog();

    void sync() override;

protected:
    void paintOver(const toolkit::Painter &painter) override;

private:

    toolkit::Label *_version = nullptr;
    toolkit::Label *_path = nullptr;
};

}
