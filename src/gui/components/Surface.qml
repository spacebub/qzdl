/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick
import Zdl

// The one card in the interface: a flat panel with a hairline border.
Rectangle {
    id: surface

    property bool inset: false
    property bool hoverable: false
    property bool hovered: false

    color: inset ? Theme.sunken : Theme.surface
    radius: Theme.radius
    border.width: 1
    border.color: hovered ? Theme.borderStrong : Theme.border

    Behavior on border.color { ColorAnimation { duration: 120 } }
    Behavior on color { ColorAnimation { duration: 120 } }

    // A border on its own is easy to miss on something this large, so a panel
    // that answers the pointer lightens under it as well. Behind whatever the
    // panel is given, which comes after it.
    Rectangle {
        anchors.fill: parent
        radius: surface.radius
        color: Theme.hover
        opacity: surface.hoverable && surface.hovered ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 120 } }
    }
}
