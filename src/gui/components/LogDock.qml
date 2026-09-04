import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic
import Zdl

/*
The running games, along the bottom of the window. Each has a tab; opening one
folds away whichever was open, so only ever one log is being drawn and the rest
are not told about their lines at all until somebody asks for them.
*/
Item {
    id: dock

    /** What is asked before a tab takes a running game with it. */
    property var confirm: null

    readonly property string open: App.runs.showing
    readonly property var log: dock.open === "" ? null : App.runs.log(dock.open)

    /** Only the open one, and only while the window is in front of somebody. */
    property var watching: null

    readonly property int panelHeight: 300

    visible: App.runs.docked.length > 0

    implicitHeight: strip.height + (dock.open === "" ? 0 : dock.panelHeight + 8)

    function refresh() {
        const wanted = dock.open === "" || !Window.active ? null : dock.log

        if (dock.watching === wanted) {
            return
        }

        if (dock.watching) {
            dock.watching.active = false
        }

        dock.watching = wanted

        if (dock.watching) {
            dock.watching.active = true
        }
    }

    onOpenChanged: dock.refresh()
    Component.onCompleted: dock.refresh()

    Connections {
        target: Window.window

        function onActiveChanged() { dock.refresh() }
    }

    Surface {
        id: panel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: strip.top
        anchors.bottomMargin: 8
        height: dock.panelHeight
        visible: dock.open !== ""

        Row {
            id: head

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 8

            Text {
                width: parent.width - buttons.width - 8
                text: App.runs.title(dock.open)
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Theme.headingWeight
                elide: Text.ElideRight
                textFormat: Text.PlainText
                anchors.verticalCenter: parent.verticalCenter
            }

            Row {
                id: buttons
                spacing: 1
                anchors.verticalCenter: parent.verticalCenter

                GlyphButton {
                    glyph: "extract"
                    size: 26
                    hint: "Copy all of it"
                    enabled: dock.log !== null && dock.log.count > 0
                    onClicked: {
                        App.copyToClipboard(dock.log.text())
                        App.notify.success("The output is on the clipboard.")
                    }
                }

                GlyphButton {
                    glyph: "down"
                    size: 26
                    hint: "Fold it away"
                    onClicked: App.runs.hide()
                }
            }
        }

        Surface {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: head.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 12
            anchors.topMargin: 10
            inset: true

            ListView {
                id: lines

                // Whether the view is following the end. It stops as soon as
                // somebody scrolls back, and follows again at the end.
                property bool pinned: true

                anchors.fill: parent
                anchors.margins: 8
                clip: true
                model: dock.log
                cacheBuffer: 0
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds

                onCountChanged: if (lines.pinned) { Qt.callLater(lines.positionViewAtEnd) }
                onMovementEnded: lines.pinned = lines.atYEnd

                ScrollBar.vertical: ScrollBar {
                    id: bar
                    visible: bar.size < 1
                    width: 8

                    contentItem: Rectangle {
                        radius: 4
                        color: Theme.borderStrong
                        opacity: bar.pressed ? 0.9 : 0.45
                    }
                }

                delegate: Text {
                    required property string line
                    required property bool own

                    width: lines.width - (bar.visible ? bar.width + 4 : 0)
                    text: line
                    color: own ? Theme.accent : Theme.muted
                    font.family: Theme.mono
                    font.pixelSize: Theme.fontTiny
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }
            }
        }

        AppButton {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 22
            visible: !lines.pinned && dock.log !== null && dock.log.count > 0
            text: "Latest"
            compact: true
            glyph: "down"

            onClicked: {
                lines.pinned = true
                lines.positionViewAtEnd()
            }
        }
    }

    Row {
        id: strip

        anchors.left: parent.left
        anchors.bottom: parent.bottom
        spacing: 8
        height: 34

        Repeater {
            model: App.runs.docked

            delegate: Rectangle {
                id: tab

                required property string modelData

                readonly property bool open: App.runs.showing === tab.modelData
                readonly property string state: App.runs.states[tab.modelData] || ""

                width: Math.min(240, label.implicitWidth + 74)
                height: strip.height
                radius: Theme.radiusSmall
                color: tab.open ? Theme.raised : hover.hovered ? Theme.surface : Theme.sunken
                border.width: 1
                border.color: tab.open ? Theme.accent : Theme.border

                Behavior on color { ColorAnimation { duration: 120 } }
                Behavior on border.color { ColorAnimation { duration: 120 } }

                // The same mark the card carries, so a tab and a card agree.
                Rectangle {
                    id: dot

                    property real pulse: 1

                    width: 7
                    height: 7
                    radius: width / 2
                    anchors.left: parent.left
                    anchors.leftMargin: 11
                    anchors.verticalCenter: parent.verticalCenter
                    opacity: tab.state === "launching" ? dot.pulse : 1

                    color: tab.state === "launching" ? Theme.accent
                         : tab.state === "running" ? Theme.success
                         : tab.state === "failed" ? Theme.danger
                         : Theme.faint

                    SequentialAnimation on pulse {
                        running: tab.state === "launching"
                        loops: Animation.Infinite

                        NumberAnimation { to: 0.2; duration: 620; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1; duration: 620; easing.type: Easing.InOutQuad }
                    }
                }

                Text {
                    id: label

                    anchors.left: dot.right
                    anchors.leftMargin: 8
                    anchors.right: shut.left
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    text: App.runs.title(tab.modelData)
                    color: tab.open ? Theme.text : Theme.muted
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }

                GlyphButton {
                    id: shut

                    glyph: "cross"
                    size: 22
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    hoverTone: Theme.danger
                    hint: App.runs.alive(tab.modelData)
                        ? "Stop the game and take the tab away"
                        : "Take the tab away"

                    onClicked: dock.shut(tab.modelData)
                }

                HoverHandler {
                    id: hover
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler { onTapped: App.runs.toggle(tab.modelData) }
            }
        }
    }

    function shut(key) {
        if (!App.runs.alive(key) || !dock.confirm) {
            App.runs.close(key)

            return
        }

        dock.confirm.ask("Stop " + App.runs.title(key) + "?",
                         "The game is still running. Closing this takes it with it, and anything "
                         + "it has not saved goes.",
                         "Stop it", true,
                         function () { App.runs.close(key) })
    }
}
