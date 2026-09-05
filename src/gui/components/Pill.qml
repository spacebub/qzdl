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

// A small status badge: a dot and a word.
Rectangle {
    id: pill

    property string text: ""
    property color tone: Theme.accent
    property color wash: Theme.accentSoft
    property bool dot: true

    /*
    What the word is actually set in. A hue that carries a dark shade is
    light by definition, and laid on its own near-white tint in the light
    shade there is too little between the two to read, so there it is taken
    down until there is.
    */
    readonly property color ink: Theme.dark ? pill.tone : Qt.darker(pill.tone, 1.35)

    implicitWidth: row.implicitWidth + 22
    implicitHeight: 26
    radius: height / 2
    color: wash

    /*
    The tint alone does not always separate the pill from what it sits on:
    the neutral one is the same wash an inset card is painted in, and every
    one of them is faint over white. A hairline of its own ink gives it an
    edge wherever it lands.
    */
    border.width: 1
    border.color: Qt.alpha(pill.ink, 0.3)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            visible: pill.dot
            width: 8
            height: 8
            radius: 4
            color: pill.ink
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: pill.text
            color: pill.ink
            font.pixelSize: Theme.fontSmall
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
