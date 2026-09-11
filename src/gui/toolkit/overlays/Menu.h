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
#include <vector>

#include "gui/toolkit/Widget.h"

namespace toolkit {

// A short list of things to do, at the pointer.
class Menu : public Widget {
public:
    struct Row {
        std::string action;
        std::string label;
        std::string glyph;
        bool danger = false;
        bool separator = false;
        bool disabled = false;
    };

    static Row item(std::string action, std::string label, std::string glyph,
                    const bool danger = false, const bool disabled = false) {
        Row row;

        row.action = std::move(action);
        row.label = std::move(label);
        row.glyph = std::move(glyph);
        row.danger = danger;
        row.disabled = disabled;

        return row;
    }

    static Row rule() {
        Row row;

        row.separator = true;

        return row;
    }

    static constexpr double WIDTH = 240.0;

    Menu(std::vector<Row> rows, std::function<void(const std::string &)> triggered);

    // How tall the rows come to, so the caller can place it.
    static double heightOf(const std::vector<Row> &rows);

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &at) override;
    void hover(const Pointer &at) override;
    void leave() override;

private:
    [[nodiscard]] int rowAt(double y) const;

    static constexpr double ROW = 32.0;
    static constexpr double RULE = 9.0;

    std::vector<Row> _rows;
    std::function<void(const std::string &)> _triggered;

    int _over = -1;
};

}
