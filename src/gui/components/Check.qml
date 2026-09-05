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
A box with a tick in it, for a row in a list. The switch is the control for a
setting that stands on its own; twenty switches down the side of a file list
would be twenty times wider than what they are switching, so a row gets this.
*/
Item {
    id: control

    property bool checked: false
    property string hint: ""

    signal toggled(bool value)

    implicitWidth: 18
    implicitHeight: 18

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: control.checked ? Theme.accent : Theme.field
        border.width: 1
        border.color: control.checked ? Theme.accent
            : area.containsMouse ? Theme.borderStrong
            : Theme.border

        Behavior on color { ColorAnimation { duration: 110 } }
        Behavior on border.color { ColorAnimation { duration: 110 } }
    }

    Glyph {
        anchors.centerIn: parent
        name: "check"
        weight: 0.85
        tone: Theme.accentText
        opacity: control.checked ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 110 } }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            control.checked = !control.checked
            control.toggled(control.checked)
        }
    }

    Hint {
        id: say

        text: control.hint
        visible: control.hint !== "" && area.containsMouse

        // Over the pointer rather than the middle of something that can be as
        // wide as the page.
        onVisibleChanged: {
            if (say.visible) {
                say.at = area.mouseX
            }
        }
    }
}
