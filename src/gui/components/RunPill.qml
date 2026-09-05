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

// What a game ZDL started is doing, said on the card it was started from.
Rectangle {
    id: pill

    // launching | running | closed | failed. Empty is nothing to say.
    property string status: ""

    property string reason: ""

    signal clicked()

    readonly property color tone: pill.status === "launching" ? Theme.emberHigh
                                : pill.status === "running" ? Theme.artSuccess
                                : pill.status === "failed" ? Theme.artDanger
                                : Theme.steel

    readonly property alias hovered: reach.hovered

    readonly property string label: pill.status === "launching" ? "Launching"
                                  : pill.status === "running" ? "Running"
                                  : pill.status === "failed" ? "Failed"
                                  : "Closed"

    readonly property string hint: pill.status === "launching"
            ? "It has been started and is loading."
        : pill.status === "running" ? "It is up."
        : pill.status === "failed"
            ? (pill.reason === "" ? "It did not start." : pill.reason)
        : "It has been closed."

    visible: pill.status !== ""
    implicitWidth: row.implicitWidth + 20
    implicitHeight: 24
    radius: height / 2

    // Painted for the card's tile, which is the same dark ground in both
    // shades, rather than for the theme.
    color: Qt.rgba(0, 0, 0, 0.62)
    border.width: 1
    border.color: Qt.alpha(pill.tone, 0.5)

    Row {
        id: row

        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            id: dot

            property real pulse: 1

            width: 7
            height: 7
            radius: width / 2
            color: pill.tone
            anchors.verticalCenter: parent.verticalCenter

            opacity: pill.status === "launching" ? dot.pulse : 1

            SequentialAnimation on pulse {
                running: pill.status === "launching"
                loops: Animation.Infinite

                NumberAnimation { to: 0.2; duration: 620; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1; duration: 620; easing.type: Easing.InOutQuad }
            }
        }

        Text {
            text: pill.label
            color: pill.tone
            font.pixelSize: Theme.fontTiny
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
        }
    }

    HoverHandler {
        id: reach
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: pill.clicked()
    }

    Hint {
        text: pill.hint + " Click to see what it printed."
        visible: reach.hovered
    }
}
