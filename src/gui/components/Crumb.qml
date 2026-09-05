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

// One piece of a path, clickable.
Item {
    id: crumb

    property string label: ""
    property bool last: false

    signal activated()

    width: text.implicitWidth + 14
    height: parent ? parent.height : 30

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        radius: Theme.radiusSmall - 2
        color: hover.hovered ? Theme.hover : Qt.alpha(Theme.hover, 0)

        Behavior on color { ColorAnimation { duration: 100 } }
    }

    Text {
        id: text
        anchors.centerIn: parent
        text: crumb.label
        color: crumb.last ? Theme.text : hover.hovered ? Theme.accent : Theme.muted
        font.pixelSize: Theme.fontSmall
        font.family: Theme.mono
        font.weight: crumb.last ? Font.DemiBold : Font.Normal
        textFormat: Text.PlainText
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler { onTapped: crumb.activated() }
}
