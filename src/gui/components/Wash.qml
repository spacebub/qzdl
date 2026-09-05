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

/*
What is painted behind a row, a tab or an entry: a tint for the one that is
picked and a lighter wash for the one under the pointer.

They are two rectangles fading on their own opacity rather than one that
changes colour, because the two washes have very different alpha and a
colour animation between them runs the alpha up while the colour is still
near the one it started from. Half way across a click that lands on a row
is a half-opaque near-white over the surface, which is lighter than either
end and reads as a flash before the tint settles.
*/
Item {
    id: wash

    property bool selected: false
    property bool hovered: false
    property color tint: Theme.accentSoft
    property color pointer: Theme.hover
    property int rounding: Theme.radiusSmall

    Rectangle {
        anchors.fill: parent
        radius: wash.rounding
        color: wash.tint
        opacity: wash.selected ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 110 } }
    }

    Rectangle {
        anchors.fill: parent
        radius: wash.rounding
        color: wash.pointer
        opacity: wash.hovered && !wash.selected ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 110 } }
    }
}
