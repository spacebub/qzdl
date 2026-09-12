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

#include "gui/toolkit/Widget.h"

namespace toolkit {

// A row that starts another line rather than run past its width.
class Wrap : public Widget {
public:
    Wrap *spacing(double across, double down);

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface &type, double width) override;

    void arrange(Typeface &type) override;

private:
    double lay(Typeface &type, double width, bool place);

    double _across = 8.0;
    double _down = 8.0;
};

}
