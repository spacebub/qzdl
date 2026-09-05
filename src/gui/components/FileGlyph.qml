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
