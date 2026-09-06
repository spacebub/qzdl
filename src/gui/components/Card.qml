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
import QtQuick.Effects
import Zdl

// A card that leans towards the pointer. The shadow does not lean with it: it
// says how high the card is, not which way it is facing.
Item {
    id: card

    property int rounding: Theme.radius

    // Set by a card whose own buttons hold the pointer. Without it, moving onto
    // a button reads as leaving the card and the two flicker at each other.
    property bool claimed: false

    // How far the near edge comes round, in degrees, at the very corner.
    property real lean: 7

    // Set by a shelf that reorders its cards: the card can be pulled out of
    // its place and dropped on another.
    property bool draggable: false

    readonly property bool dragging: area.drag.active

    // What a drop is handed. The card itself, unless the shelf it is on puts
    // the place the card left forward instead.
    property Item origin: card

    readonly property real depth: -0.0022

    readonly property bool hovered: area.containsMouse || card.claimed
    readonly property bool pressed: area.pressed

    default property alias content: face.data

    signal clicked()

    // Where the pointer is across the card, from -1 to 1.
    readonly property real reachX: card.hovered && card.width > 0
        ? Math.max(-1, Math.min(1, (area.mouseX / card.width - 0.5) * 2))
        : 0

    readonly property real reachY: card.hovered && card.height > 0
        ? Math.max(-1, Math.min(1, (area.mouseY / card.height - 0.5) * 2))
        : 0

    // Trailing the pointer rather than bound to it, so the card has some weight
    // and falls flat again instead of snapping.
    property real leanX: card.reachX
    property real leanY: card.reachY

    Behavior on leanX { NumberAnimation { duration: 160; easing.type: Easing.OutQuad } }
    Behavior on leanY { NumberAnimation { duration: 160; easing.type: Easing.OutQuad } }

    z: card.hovered ? 2 : 0

    Drag.active: card.dragging
    Drag.source: card.origin

    // Under everything, so the card's own buttons take the click first.
    MouseArea {
        id: area

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        drag.target: card.draggable ? card : null
        onClicked: card.clicked()

        // Where it is dropped is where the pointer is, not where the middle of
        // the card is.
        onPressed: function (mouse) {
            card.Drag.hotSpot.x = mouse.x
            card.Drag.hotSpot.y = mouse.y
        }
    }

    RectangularShadow {
        id: cast

        property real rise: card.hovered ? 1 : 0

        Behavior on rise { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }

        anchors.fill: parent
        radius: card.rounding
        color: Theme.shadow
        blur: 18 + cast.rise * 16
        spread: cast.rise
        offset: Qt.vector2d(0, 6 + cast.rise * 8)
    }

    Item {
        id: lift

        anchors.fill: parent
        scale: card.pressed ? 0.99 : card.hovered ? 1.02 : 1

        Behavior on scale { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }

        // Centred, turned about both axes, thrown at the vanishing point, put
        // back. The list runs in the order it is applied.
        transform: [
            Translate {
                x: -card.width / 2
                y: -card.height / 2
            },
            Rotation {
                axis { x: 1; y: 0; z: 0 }
                angle: card.leanY * card.lean
            },
            Rotation {
                axis { x: 0; y: 1; z: 0 }
                angle: -card.leanX * card.lean
            },
            Matrix4x4 {
                matrix: Qt.matrix4x4(1, 0, 0, 0,
                                     0, 1, 0, 0,
                                     0, 0, 1, 0,
                                     0, 0, card.depth, 1)
            },
            Translate {
                x: card.width / 2
                y: card.height / 2
            }
        ]

        Rectangle {
            id: face

            anchors.fill: parent
            radius: card.rounding
            color: Theme.surface
            clip: true

            border.width: 1
            border.color: card.hovered ? Theme.borderStrong : Theme.border

            Behavior on border.color { ColorAnimation { duration: 140 } }
        }

        // The light the card catches as it turns, from whichever side the
        // pointer is on.
        Item {
            anchors.fill: parent
            opacity: card.hovered ? 1 : 0

            Behavior on opacity { NumberAnimation { duration: 170 } }

            Rectangle {
                anchors.fill: parent
                radius: card.rounding
                opacity: Math.max(0, -card.leanX)

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: Qt.rgba(1, 1, 1, 0.07) }
                    GradientStop { position: 0.75; color: Qt.rgba(1, 1, 1, 0) }
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: card.rounding
                opacity: Math.max(0, card.leanX)

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.25; color: Qt.rgba(1, 1, 1, 0) }
                    GradientStop { position: 1; color: Qt.rgba(1, 1, 1, 0.07) }
                }
            }
        }
    }
}
