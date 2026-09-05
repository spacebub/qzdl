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

// A card with a picture, a name and a line under it. Both shelves are drawn
// from this one; only what a plain click does differs.
Card {
    id: card

    property string title: ""
    property string subtitle: ""
    property string caption: ""

    // The game file the picture is read out of.
    property string artFile: ""

    // { text, tone, wash } each, drawn along the bottom of the card.
    property var badges: []

    // See ActionMenu.
    property var actions: []

    property string playHint: ""
    property bool playable: true

    // See RunPill.
    property string status: ""
    property string statusReason: ""

    // play | open -- what a click anywhere but the buttons does.
    property string primary: "open"

    signal played()
    signal opened()
    signal logRequested()
    signal triggered(string action)

    width: Theme.cardWidth
    height: Theme.cardArt + 94

    onClicked: card.primary === "play" ? card.played() : card.opened()

    // The pointer is still on the card while it is on one of these.
    claimed: play.hovered || more.hovered || menu.visible || state.hovered

    CardArt {
        id: art

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.cardArt
        rounding: card.rounding
        lit: card.hovered
        caption: card.caption
        file: card.artFile

        // The picture goes back as the play button comes forward.
        Rectangle {
            anchors.fill: parent
            color: Theme.artBottom
            topLeftRadius: card.rounding
            topRightRadius: card.rounding
            opacity: card.hovered && card.playable ? 0.35 : 0

            Behavior on opacity { NumberAnimation { duration: 170 } }
        }

        PlayBadge {
            id: play

            anchors.centerIn: parent
            shown: card.hovered && card.playable
            active: card.playable
            hint: card.playHint
            onClicked: card.played()
        }

        RunPill {
            id: state

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
            status: card.status
            reason: card.statusReason
            onClicked: card.logRequested()
        }
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: art.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 12
        spacing: 2

        Text {
            width: parent.width - 26
            text: card.title
            color: Theme.text
            font.pixelSize: Theme.fontMedium
            font.weight: Theme.headingWeight
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }

        Text {
            width: parent.width
            text: card.subtitle
            color: Theme.faint
            font.pixelSize: Theme.fontSmall
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        spacing: 6

        Repeater {
            model: card.badges

            delegate: Pill {
                required property var modelData

                text: modelData.text
                dot: modelData.dot === true
                tone: modelData.tone === undefined ? Theme.muted : modelData.tone
                wash: modelData.wash === undefined ? Theme.mutedSoft : modelData.wash
                height: 22
            }
        }
    }

    GlyphButton {
        id: more

        glyph: "dots"
        size: 26
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        opacity: card.hovered || menu.visible ? 1 : 0
        visible: card.actions.length > 0
        hint: "More"

        Behavior on opacity { NumberAnimation { duration: 140 } }

        onClicked: menu.visible ? menu.close() : menu.open()

        ActionMenu {
            id: menu

            y: -height - 4
            x: -width + parent.width

            items: card.actions

            onTriggered: function (action) { card.triggered(action) }
        }
    }
}
