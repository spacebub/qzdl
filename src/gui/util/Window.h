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

// The window itself, as the title bar's buttons see it. A view that reached for the
// whole Shell dragged SDL, the surface and the frame loop in behind it.
class Window {
public:
    Window() = default;
    virtual ~Window() = default;

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window(Window &&) = delete;
    Window &operator=(Window &&) = delete;

    virtual void minimize() const = 0;
    virtual void toggleMaximize() const = 0;

    [[nodiscard]] virtual bool maximized() const = 0;

    // Ends the frame loop, which closes the window.
    virtual void stop() = 0;
};
