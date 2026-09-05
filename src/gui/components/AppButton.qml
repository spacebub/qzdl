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

// Every button in the application. Four weights: the primary action, a plain
// one, one that reads as a warning and one that is only text.
Item {
    id: control

    property string text: ""
    property string glyph: ""            // optional, drawn ahead of the label
    property string hint: ""
    property string variant: "default"   // default | primary | danger | ghost
    property bool busy: false
    property bool compact: false
    property alias hovered: area.containsMouse

    signal clicked()

    /*
    A full sized one is at least as wide as every other full sized one, so a
    pair of them reads as a pair rather than as two buttons that happen to
    have been given labels of different lengths. A compact one sits in a row
    of its fellows where the space it takes is worth more than the tidiness,
    so that one is only as wide as what it says.
    */
    implicitWidth: compact
        ? content.implicitWidth + 26
        : Math.max(content.implicitWidth + 38, Theme.buttonWidth)
    implicitHeight: compact ? Theme.controlSmall : Theme.control
    opacity: enabled ? 1 : 0.45

    Rectangle {
        id: body
        anchors.fill: parent
        radius: Theme.radiusSmall
        border.width: control.variant === "primary" ? 0 : 1

        /*
        A button carries a face of its own rather than borrowing the card's.
        Most of them sit on an inset panel, and the plain surface is close
        enough to that shade to leave nothing but the label; the raised one
        stands off both it and a card. The one that removes something used
        to be bare until hovered, which on the same panel was a hairline in
        the border colour and effectively nothing at all, so it now keeps a
        face and states what it is in the colour of its edge.
        */
        color: {
            if (control.variant === "primary")
                return area.pressed ? Qt.darker(Theme.accent, 1.15)
                     : area.containsMouse ? Theme.accentHover : Theme.accent
            if (control.variant === "ghost")
                return area.containsMouse ? Theme.accentSoft : Qt.alpha(Theme.accentSoft, 0)
            if (control.variant === "danger")
                return area.pressed ? Qt.darker(Theme.dangerSoft, 1.08)
                     : area.containsMouse ? Theme.dangerSoft : Theme.raised
            return area.pressed ? Theme.sunken
                 : area.containsMouse ? Qt.tint(Theme.raised, Theme.hover)
                 : Theme.raised
        }

        border.color: control.variant === "danger"
            ? (area.containsMouse ? Theme.danger : Qt.alpha(Theme.danger, 0.5))
            : control.variant === "ghost" ? Qt.alpha(Theme.accentSoft, 0)
            : area.containsMouse ? Theme.accent : Theme.borderStrong

        Behavior on color { ColorAnimation { duration: 110 } }
        Behavior on border.color { ColorAnimation { duration: 110 } }

        scale: area.pressed && control.enabled ? 0.985 : 1
        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 7
        opacity: control.busy ? 0 : 1

        readonly property color ink: control.variant === "primary" ? Theme.accentText
                                   : control.variant === "danger" ? Theme.danger
                                   : control.variant === "ghost" ? Theme.accent
                                   : Theme.text

        Glyph {
            visible: control.glyph !== ""
            name: control.glyph
            tone: content.ink
            weight: control.compact ? 1.1 : 1.2
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: label
            text: control.text
            font.pixelSize: control.compact ? Theme.fontSmall : Theme.fontBody
            font.weight: Font.DemiBold
            color: content.ink
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: 5
        visible: control.busy

        Repeater {
            model: 3

            Rectangle {
                id: dot

                required property int index

                width: 6; height: 6; radius: 3
                color: control.variant === "primary" ? Theme.accentText : Theme.accent

                SequentialAnimation on opacity {
                    running: control.busy
                    loops: Animation.Infinite
                    PauseAnimation { duration: dot.index * 130 }
                    NumberAnimation { to: 0.25; duration: 320 }
                    NumberAnimation { to: 1.0; duration: 320 }
                    PauseAnimation { duration: (2 - dot.index) * 130 }
                }
            }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        enabled: control.enabled && !control.busy
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
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
