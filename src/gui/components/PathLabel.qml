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

/*
A path that has to fit in less room than it wants. It gives up whole leading
directories rather than cutting a name in half, and the whole thing is a
hover away. It can also be a way into what it names.
*/
Text {
    id: control

    property string path: ""
    property real room: 0        // what it may take up; zero falls back to eliding
    property bool clickable: false
    property string opens: ""    // what a click opens; the path itself when empty

    /*
    What the hover says instead of the path. Whatever holds one of these has
    only the one hover to spend, so it says its piece through this rather
    than putting a second tooltip over the top of this one.
    */
    property string hint: ""

    readonly property string pretty: App.prettyPath(control.path)
    readonly property bool trimmed: text !== pretty
    readonly property string destination: control.opens === "" ? control.path : control.opens

    /*
    Measuring writes to the metrics it reads, which a binding would call a
    loop, so the text is worked out whenever what it depends on changes. The
    pretty form is worked out here as well: read through the binding it would
    still be the path this label held a moment ago.
    */
    onPathChanged: control.refit()
    onRoomChanged: control.refit()
    Component.onCompleted: control.refit()

    function refit() {
        text = control.fit(App.prettyPath(control.path), control.room)
    }

    function open() {
        if (control.destination === "" || !App.reveal(control.destination)) {
            App.notify.warning("Nothing on this system offered to open it.")
        }
    }
    color: control.clickable && reach.hovered ? Theme.accent : Theme.faint
    font.family: Theme.mono
    font.pixelSize: Theme.fontTiny
    elide: Text.ElideLeft
    textFormat: Text.PlainText

    Behavior on color { ColorAnimation { duration: 120 } }

    TextMetrics {
        id: metrics
        font: control.font
    }

    function fit(value, room) {
        if (room <= 0) {
            return value
        }

        metrics.text = value

        if (metrics.width <= room) {
            return value
        }

        const parts = value.split("/")

        for (let index = 1; index < parts.length; ++index) {
            const candidate = "…/" + parts.slice(index).join("/")

            metrics.text = candidate

            if (metrics.width <= room) {
                return candidate
            }
        }

        return value
    }

    HoverHandler { id: reach }

    // A cursor of its own only when there is something to click.
    HoverHandler {
        enabled: control.clickable
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        enabled: control.clickable
        onTapped: control.open()
    }

    Hint {
        id: say

        text: control.hint !== "" ? control.hint
            : control.clickable ? "Open " + App.prettyPath(control.destination)
            : control.path
        visible: reach.hovered
            && (control.hint !== "" || control.clickable || control.trimmed)

        // A path can be as wide as the page, so this one points at where the
        // pointer came in and stays there rather than following it about.
        onVisibleChanged: {
            if (say.visible) {
                say.at = reach.point.position.x
            }
        }
    }
}
