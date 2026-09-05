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
import QtQuick.Shapes
import Zdl

// The picture at the top of a card: the game's own title screen where the file
// has one, and the mark's own tile where it has not.
Item {
    id: art

    property bool lit: false
    property string caption: ""

    // The game file to read a title screen out of.
    property string file: ""

    readonly property bool drawn: shot.status === Image.Ready

    // Clipping in Qt Quick is rectangular, so every layer carries the curve
    // itself rather than being cut to it.
    property int rounding: Theme.radius
    property int bottomRounding: 0

    clip: true

    Rectangle {
        anchors.fill: parent
        topLeftRadius: art.rounding
        topRightRadius: art.rounding
        bottomLeftRadius: art.bottomRounding
        bottomRightRadius: art.bottomRounding

        gradient: Gradient {
            GradientStop { position: 0; color: Theme.artTop }
            GradientStop { position: 0.55; color: Theme.artMiddle }
            GradientStop { position: 1; color: Theme.artBottom }
        }
    }

    // Lit from the top left, as the mark is.
    Rectangle {
        anchors.fill: parent
        topLeftRadius: art.rounding
        topRightRadius: art.rounding
        bottomLeftRadius: art.bottomRounding
        bottomRightRadius: art.bottomRounding

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: Qt.rgba(1, 1, 1, 0.055) }
            GradientStop { position: 0.7; color: Qt.rgba(1, 1, 1, 0) }
        }
    }

    // The ember, low and left where the mark puts it. A radial fall-off is the
    // one thing a plain rectangle cannot do.
    Shape {
        anchors.fill: parent
        opacity: art.lit ? 1 : 0.6
        preferredRendererType: Shape.CurveRenderer

        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        ShapePath {
            strokeWidth: -1

            fillGradient: RadialGradient {
                centerX: art.width * 0.3
                centerY: art.height * 1.05
                centerRadius: Math.max(art.width, art.height) * 0.95
                focalX: centerX
                focalY: centerY

                GradientStop { position: 0; color: Qt.alpha(Theme.ember, 0.38) }
                GradientStop { position: 0.45; color: Qt.alpha(Theme.ember, 0.12) }
                GradientStop { position: 1; color: Qt.alpha(Theme.ember, 0) }
            }

            startX: 0
            startY: art.rounding
            PathArc {
                x: art.rounding
                y: 0
                radiusX: art.rounding
                radiusY: art.rounding
                direction: PathArc.Clockwise
            }
            PathLine { x: art.width - art.rounding; y: 0 }
            PathArc {
                x: art.width
                y: art.rounding
                radiusX: art.rounding
                radiusY: art.rounding
                direction: PathArc.Clockwise
            }
            PathLine { x: art.width; y: art.height - art.bottomRounding }
            PathArc {
                x: art.width - art.bottomRounding
                y: art.height
                radiusX: art.bottomRounding
                radiusY: art.bottomRounding
                direction: PathArc.Clockwise
            }
            PathLine { x: art.bottomRounding; y: art.height }
            PathArc {
                x: 0
                y: art.height - art.bottomRounding
                radiusX: art.bottomRounding
                radiusY: art.bottomRounding
                direction: PathArc.Clockwise
            }
            PathLine { x: 0; y: art.rounding }
        }
    }

    /*
    Clipping is rectangular, so the picture is drawn away from the screen and
    put back through a mask that has the corners in it. The crop is why it goes
    the long way round rather than straight into the effect.
    */
    Item {
        id: frame

        anchors.fill: parent
        layer.enabled: true
        visible: false

        Image {
            id: shot

            anchors.fill: parent
            source: art.file === "" ? "" : App.artFor(art.file)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            mipmap: true
        }
    }

    Rectangle {
        id: corners

        anchors.fill: parent
        layer.enabled: true
        visible: false
        color: "white"
        topLeftRadius: art.rounding
        topRightRadius: art.rounding
        bottomLeftRadius: art.bottomRounding
        bottomRightRadius: art.bottomRounding
    }

    MultiEffect {
        id: picture

        anchors.fill: parent
        source: frame
        maskEnabled: true
        maskSource: corners
        opacity: art.drawn ? 1 : 0
        visible: picture.opacity > 0

        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    }

    // Held back from full strength so it reads as a placeholder.
    Image {
        id: mark

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: art.caption === "" ? 0 : -9

        width: Math.round(Math.min(parent.height * 0.44, 64))
        height: width
        sourceSize.width: 256
        sourceSize.height: 256
        source: "qrc:/qzdl-256.png"
        smooth: true
        opacity: art.drawn ? 0 : art.lit ? 0.95 : 0.72
        visible: mark.opacity > 0

        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    // A title screen is a picture before it is a background, so the name over
    // it needs a ground of its own to stay readable.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 46
        visible: art.caption !== "" && art.drawn
        bottomLeftRadius: art.bottomRounding
        bottomRightRadius: art.bottomRounding

        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0) }
            GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.7) }
        }
    }

    Text {
        visible: art.caption !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        width: parent.width - 24
        text: art.caption
        color: art.drawn ? Theme.text : Theme.steel
        opacity: art.drawn ? 0.95 : 0.75
        font.pixelSize: Theme.fontTiny
        font.letterSpacing: 1.4
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        textFormat: Text.PlainText
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: art.rounding
        anchors.rightMargin: art.rounding
        height: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }
}
