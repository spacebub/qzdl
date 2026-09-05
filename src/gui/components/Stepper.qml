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
A small whole number, nudged rather than picked off a list: nine rows of a
dropdown are a poor way of saying "four". Where being unset means something,
stepping below the range clears it and stepping up brings it back.
*/
Item {
    id: control

    property string label: ""
    property string hint: ""
    property int from: 1
    property int to: 9
    property int value: 1

    // Whether it can be left unset, and what that is stored as.
    property bool clearable: false
    property int offValue: 0
    property string placeholder: "Off"

    readonly property bool unset: control.clearable && control.value < control.from

    signal stepped(int value)

    implicitWidth: 170
    implicitHeight: column.implicitHeight
    opacity: enabled ? 1 : 0.45

    function step(by) {
        if (control.unset) {
            if (by > 0) {
                control.stepped(control.from)
            }

            return
        }

        const next = control.value + by

        if (next < control.from) {
            if (control.clearable) {
                control.stepped(control.offValue)
            }
        } else if (next <= control.to) {
            control.stepped(next)
        }
    }

    Column {
        id: column

        width: parent.width
        spacing: 6

        SectionLabel {
            text: control.label.toUpperCase()
            visible: control.label !== ""
        }

        Rectangle {
            id: box

            width: parent.width
            height: Theme.control
            radius: Theme.radiusSmall
            color: Theme.field
            border.width: 1
            border.color: reach.hovered ? Theme.borderStrong : Theme.border

            Behavior on border.color { ColorAnimation { duration: 120 } }

            GlyphButton {
                glyph: "minus"
                size: 30
                enabled: !control.unset && (control.clearable || control.value > control.from)
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                onClicked: control.step(-1)
            }

            Text {
                anchors.centerIn: parent
                text: control.unset ? control.placeholder : control.value
                color: control.unset ? Theme.faint : Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: control.unset ? Font.Normal : Font.DemiBold
                font.family: control.unset ? font.family : Theme.mono
                textFormat: Text.PlainText
            }

            GlyphButton {
                glyph: "plus"
                size: 30
                enabled: control.unset || control.value < control.to
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                onClicked: control.step(1)
            }

            HoverHandler { id: reach }

            Hint {
                id: say

                text: control.hint
                visible: control.hint !== "" && reach.hovered

                // Over the pointer rather than the middle of something that
                // can be as wide as the page.
                onVisibleChanged: {
                    if (say.visible) {
                        say.at = reach.point.position.x
                    }
                }
            }
        }
    }
}
