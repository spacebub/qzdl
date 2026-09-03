import QtQuick
import Zdl

// A folder or a page, drawn rather than themed.
Canvas {
    id: glyph

    property bool folder: false
    property color tone: folder ? Theme.accent : Theme.faint

    readonly property real weight: 1.2

    width: 15 * weight
    height: 15 * weight
    renderStrategy: Canvas.Cooperative

    onToneChanged: requestPaint()
    onFolderChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.scale(glyph.weight, glyph.weight)
        ctx.fillStyle = tone
        ctx.strokeStyle = tone
        ctx.lineWidth = 1.2
        ctx.lineJoin = "round"

        if (folder) {
            ctx.beginPath()
            ctx.moveTo(1, 4)
            ctx.lineTo(5.5, 4)
            ctx.lineTo(7, 5.8)
            ctx.lineTo(14, 5.8)
            ctx.lineTo(14, 13)
            ctx.lineTo(1, 13)
            ctx.closePath()
            ctx.globalAlpha = 0.85
            ctx.fill()
        } else {
            ctx.beginPath()
            ctx.moveTo(3, 1.5)
            ctx.lineTo(9.5, 1.5)
            ctx.lineTo(12.5, 4.8)
            ctx.lineTo(12.5, 13.5)
            ctx.lineTo(3, 13.5)
            ctx.closePath()
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(9.3, 1.7)
            ctx.lineTo(9.3, 5)
            ctx.lineTo(12.3, 5)
            ctx.stroke()
        }
    }
}
