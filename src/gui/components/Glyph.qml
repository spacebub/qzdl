import QtQuick
import Zdl

/*
Every icon the interface draws. They are drawn rather than typed so they do
not depend on what the system font happens to carry, and they are all here so
a button and a label ask for the same one the same way.

They were designed in a twelve pixel square and are small enough to squint at
at that size, so they are still drawn in the space they were designed for and
scaled on the way out: one number is the size of every one of them.
*/
Item {
    id: glyph

    property string name: ""      // close | minimize | maximize | restore | refresh | plus | cross
                                  // download | extract | trash | edit | folder | up | down | check
                                  // system | light | dark | cog | play | dots | search | grip
                                  // terminal | minus
    property color tone: Theme.muted
    property real weight: 1.2

    implicitWidth: 12 * weight
    implicitHeight: 12 * weight

    Rectangle {
        // The same bar either way: one takes a window down, the other a number.
        visible: glyph.name === "minimize" || glyph.name === "minus"
        anchors.centerIn: parent
        width: 11 * glyph.weight; height: 1.5 * glyph.weight; radius: 1
        color: glyph.tone
    }

    Rectangle {
        visible: glyph.name === "maximize"
        anchors.centerIn: parent
        width: 10 * glyph.weight; height: 10 * glyph.weight; radius: 2
        color: "transparent"
        border.width: 1.5 * glyph.weight
        border.color: glyph.tone
    }

    Item {
        visible: glyph.name === "restore"
        anchors.centerIn: parent
        width: 12 * glyph.weight; height: 12 * glyph.weight

        Rectangle {
            x: 3 * glyph.weight; y: 0
            width: 9 * glyph.weight; height: 9 * glyph.weight; radius: 2
            color: "transparent"; border.width: 1.5 * glyph.weight; border.color: glyph.tone
        }
        Rectangle {
            x: 0; y: 3 * glyph.weight
            width: 9 * glyph.weight; height: 9 * glyph.weight; radius: 2
            color: Theme.surface; border.width: 1.5 * glyph.weight; border.color: glyph.tone
        }
    }

    Item {
        visible: glyph.name === "close" || glyph.name === "cross"
        anchors.centerIn: parent
        width: 12 * glyph.weight; height: 12 * glyph.weight

        Rectangle {
            anchors.centerIn: parent
            width: 13 * glyph.weight; height: 1.5 * glyph.weight; radius: 1
            color: glyph.tone
            rotation: 45
        }
        Rectangle {
            anchors.centerIn: parent
            width: 13 * glyph.weight; height: 1.5 * glyph.weight; radius: 1
            color: glyph.tone
            rotation: -45
        }
    }

    Item {
        visible: glyph.name === "plus"
        anchors.centerIn: parent
        width: 12 * glyph.weight; height: 12 * glyph.weight

        Rectangle {
            anchors.centerIn: parent
            width: 12 * glyph.weight; height: 1.6 * glyph.weight; radius: 1; color: glyph.tone
        }
        Rectangle {
            anchors.centerIn: parent
            width: 1.6 * glyph.weight; height: 12 * glyph.weight; radius: 1; color: glyph.tone
        }
    }

    Canvas {
        visible: glyph.name === "download"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.6
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(7, 1)
            ctx.lineTo(7, 9.5)
            ctx.moveTo(3.2, 6)
            ctx.lineTo(7, 9.8)
            ctx.lineTo(10.8, 6)
            ctx.moveTo(2, 12.6)
            ctx.lineTo(12, 12.6)
            ctx.stroke()
        }
    }

    Canvas {
        visible: glyph.name === "extract"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            // A box with what was in it coming out to the right.
            ctx.beginPath()
            ctx.moveTo(6.5, 1.5)
            ctx.lineTo(1.5, 1.5)
            ctx.lineTo(1.5, 12.5)
            ctx.lineTo(6.5, 12.5)
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(5, 7)
            ctx.lineTo(12.5, 7)
            ctx.moveTo(9.2, 3.8)
            ctx.lineTo(12.6, 7)
            ctx.lineTo(9.2, 10.2)
            ctx.stroke()
        }
    }

    Canvas {
        visible: glyph.name === "trash"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(1.5, 3.5)
            ctx.lineTo(12.5, 3.5)
            ctx.moveTo(5.2, 3.3)
            ctx.lineTo(5.2, 1.6)
            ctx.lineTo(8.8, 1.6)
            ctx.lineTo(8.8, 3.3)
            ctx.moveTo(3, 3.8)
            ctx.lineTo(3.7, 12.6)
            ctx.lineTo(10.3, 12.6)
            ctx.lineTo(11, 3.8)
            ctx.moveTo(5.8, 6)
            ctx.lineTo(6, 10.4)
            ctx.moveTo(8.2, 6)
            ctx.lineTo(8, 10.4)
            ctx.stroke()
        }
    }

    Canvas {
        visible: glyph.name === "edit"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(2, 12)
            ctx.lineTo(2, 9.4)
            ctx.lineTo(9.4, 2)
            ctx.lineTo(12, 4.6)
            ctx.lineTo(4.6, 12)
            ctx.closePath()
            ctx.stroke()
        }
    }

    Canvas {
        visible: glyph.name === "folder"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.fillStyle = ink
            ctx.globalAlpha = 0.9
            ctx.beginPath()
            ctx.moveTo(1, 3.5)
            ctx.lineTo(5.5, 3.5)
            ctx.lineTo(7, 5.4)
            ctx.lineTo(13, 5.4)
            ctx.lineTo(13, 12)
            ctx.lineTo(1, 12)
            ctx.closePath()
            ctx.fill()
        }
    }

    Canvas {
        visible: glyph.name === "up"
        anchors.centerIn: parent
        width: 13 * glyph.weight
        height: 13 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.6
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(6.5, 12)
            ctx.lineTo(6.5, 1.8)
            ctx.moveTo(2, 6.2)
            ctx.lineTo(6.5, 1.8)
            ctx.lineTo(11, 6.2)
            ctx.stroke()
        }
    }

    Canvas {
        visible: glyph.name === "down"
        anchors.centerIn: parent
        width: 13 * glyph.weight
        height: 13 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.6
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(6.5, 1.8)
            ctx.lineTo(6.5, 12)
            ctx.moveTo(2, 7.6)
            ctx.lineTo(6.5, 12)
            ctx.lineTo(11, 7.6)
            ctx.stroke()
        }
    }

    // Half filled: the shade is whatever the desktop is set to.
    Canvas {
        visible: glyph.name === "system"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.fillStyle = ink
            ctx.lineWidth = 1.5

            ctx.beginPath()
            ctx.arc(7, 7, 4.9, 0, Math.PI * 2)
            ctx.stroke()

            ctx.beginPath()
            ctx.arc(7, 7, 4.9, Math.PI * 0.5, Math.PI * 1.5)
            ctx.closePath()
            ctx.fill()
        }
    }

    Canvas {
        visible: glyph.name === "light"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"

            ctx.beginPath()
            ctx.arc(7, 7, 3, 0, Math.PI * 2)
            ctx.stroke()

            // Eight rays, struck outwards from the same centre.
            ctx.beginPath()

            for (let ray = 0; ray < 8; ++ray) {
                const angle = ray * Math.PI / 4

                ctx.moveTo(7 + Math.cos(angle) * 4.8, 7 + Math.sin(angle) * 4.8)
                ctx.lineTo(7 + Math.cos(angle) * 6.3, 7 + Math.sin(angle) * 6.3)
            }

            ctx.stroke()
        }
    }

    // A crescent, cut out of one circle by another laid over its corner.
    Canvas {
        visible: glyph.name === "dark"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.5
            ctx.lineJoin = "round"

            ctx.beginPath()
            ctx.arc(7, 7, 5, -1.161, 2.732)
            ctx.arc(4.4, 4.4, 5, 1.979, -0.408, true)
            ctx.closePath()
            ctx.stroke()
        }
    }

    Canvas {
        visible: glyph.name === "check"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.8
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(2, 7.4)
            ctx.lineTo(5.6, 11)
            ctx.lineTo(12, 3.2)
            ctx.stroke()
        }
    }

    Canvas {
        visible: glyph.name === "refresh"
        anchors.centerIn: parent
        width: 13 * glyph.weight; height: 13 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.6
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(6.5, 6.5, 5, Math.PI * 0.35, Math.PI * 1.85)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(9.8, 1.6)
            ctx.lineTo(10.6, 5.4)
            ctx.lineTo(6.9, 4.4)
            ctx.closePath()
            ctx.fillStyle = ink
            ctx.fill()
        }
    }

    Canvas {
        visible: glyph.name === "cog"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"

            for (let tooth = 0; tooth < 8; ++tooth) {
                const angle = tooth * Math.PI / 4
                ctx.beginPath()
                ctx.moveTo(7 + Math.cos(angle) * 3.9, 7 + Math.sin(angle) * 3.9)
                ctx.lineTo(7 + Math.cos(angle) * 6.2, 7 + Math.sin(angle) * 6.2)
                ctx.stroke()
            }

            ctx.beginPath()
            ctx.arc(7, 7, 3.9, 0, Math.PI * 2)
            ctx.stroke()
        }
    }

    Canvas {
        visible: glyph.name === "play"
        anchors.centerIn: parent
        width: 13 * glyph.weight
        height: 13 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.fillStyle = ink
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(3.4, 1.6)
            ctx.lineTo(11.6, 6.5)
            ctx.lineTo(3.4, 11.4)
            ctx.closePath()
            ctx.fill()
        }
    }

    Row {
        visible: glyph.name === "dots"
        anchors.centerIn: parent
        spacing: 2 * glyph.weight

        Repeater {
            model: 3

            Rectangle {
                width: 2.6 * glyph.weight
                height: 2.6 * glyph.weight
                radius: width / 2
                color: glyph.tone
            }
        }
    }

    Canvas {
        visible: glyph.name === "terminal"
        anchors.centerIn: parent
        width: 14 * glyph.weight
        height: 14 * glyph.weight
        renderStrategy: Canvas.Cooperative

        property color ink: glyph.tone
        onInkChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(glyph.weight, glyph.weight)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.4
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            ctx.beginPath()
            ctx.roundedRect(1, 2, 12, 10, 2, 2)
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(3.6, 5.6)
            ctx.lineTo(5.8, 7.4)
            ctx.lineTo(3.6, 9.2)
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(7.4, 9.4)
            ctx.lineTo(10.6, 9.4)
            ctx.stroke()
        }
    }

    // What says a row can be picked up and put down somewhere else.
    Row {
        visible: glyph.name === "grip"
        anchors.centerIn: parent
        spacing: 2.6 * glyph.weight

        Repeater {
            model: 2

            Column {
                spacing: 2.6 * glyph.weight

                Repeater {
                    model: 3

                    Rectangle {
                        width: 2.2 * glyph.weight
                        height: 2.2 * glyph.weight
                        radius: width / 2
                        color: glyph.tone
                    }
                }
            }
        }
    }

    Item {
        visible: glyph.name === "search"
        anchors.centerIn: parent
        width: 12 * glyph.weight; height: 12 * glyph.weight

        Rectangle {
            x: 0.5 * glyph.weight; y: 0.5 * glyph.weight
            width: 8.5 * glyph.weight; height: 8.5 * glyph.weight
            radius: width / 2
            color: "transparent"
            border.width: 1.5 * glyph.weight
            border.color: glyph.tone
        }

        Rectangle {
            x: 7.6 * glyph.weight; y: 8.4 * glyph.weight
            width: 4.4 * glyph.weight; height: 1.5 * glyph.weight
            radius: height / 2
            color: glyph.tone
            transformOrigin: Item.Left
            rotation: 45
        }
    }

}
