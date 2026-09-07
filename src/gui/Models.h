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

#include <algorithm>
#include <vector>

#include <slint.h>

namespace Models {

// Updates a model in place, reporting only what moved. A repeater handed a model
// it has not seen rebuilds every item, so the library would be rebuilt to tick
// one box and the engine grid twelve times a second to move a progress bar.
template <typename Row>
void reconcile(slint::VectorModel<Row> &model, const std::vector<Row> &rows) {
    const size_t held = model.row_count();
    const size_t shared = std::min(held, rows.size());

    for (size_t index = 0; index < shared; ++index) {
        if (model.row_data(index) != rows[index]) {
            model.set_row_data(index, rows[index]);
        }
    }

    for (size_t index = held; index > rows.size(); --index) {
        model.erase(index - 1);
    }

    for (size_t index = held; index < rows.size(); ++index) {
        model.push_back(rows[index]);
    }
}

}
