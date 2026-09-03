import QtQuick
import QtQuick.Window
import Zdl

/*
The window wears its own decoration, and since it is there anyway it carries
the navigation too. Dragging and the window buttons go through the window
manager, so a tiling one that ignores all three is not left with a bar that
pretends otherwise.
*/
Rectangle {
    id: bar

    property Window target: null
    property var pages: []
    property string current: ""
    property string trailing: ""
    property bool compact: false

    signal selected(string key)

    implicitHeight: 56
    color: Theme.surface

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    // Everything the controls do not claim is the drag handle.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: bar.target.startSystemMove()
        onDoubleClicked: bar.toggleMaximized()
    }

    Row {
        id: brand

        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 9

        Image {
            width: 22
            height: 22
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/qzdl-128.png"
            sourceSize.width: 44
            sourceSize.height: 44
            smooth: true
        }

        /*
        The name sits beside the icon rather than announcing itself, so it is
        weighted to be read and not to be shouted, which a near black bold at
        this size is on a white bar.
        */
        Text {
            visible: !bar.compact
            text: "ZDL"
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.Medium
            font.letterSpacing: 0.2
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
        }
    }

    /*
    Over the middle of the window, where the pages themselves are, rather than
    off in the left corner: a page is centred and only so wide, so a tab in the
    corner is the far end of a trip across the screen from anything it opens.
    A narrow window has no middle to spare, and there it falls back to sitting
    beside the name and keeps clear of the window buttons.
    */
    Row {
        id: tabs

        spacing: 2
        anchors.verticalCenter: parent.verticalCenter
        x: Math.max(brand.x + brand.width + 22,
                    Math.min((bar.width - tabs.width) / 2, controls.x - tabs.width - 16))

        Repeater {
            model: bar.pages

            delegate: Item {
                id: tab

                required property var modelData

                readonly property bool active: bar.current === tab.modelData.key

                width: label.implicitWidth + 28
                height: 34

                Wash {
                    anchors.fill: parent
                    selected: tab.active
                    hovered: hover.hovered
                }

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: tab.modelData.label
                    color: tab.active ? Theme.accent : Theme.muted
                    font.pixelSize: Theme.fontBody
                    font.weight: tab.active ? Font.DemiBold : Font.Normal
                    textFormat: Text.PlainText
                }

                Rectangle {
                    visible: tab.modelData.badge === true && !tab.active
                    width: 6
                    height: 6
                    radius: 3
                    color: Theme.accent
                    anchors.right: parent.right
                    anchors.rightMargin: 7
                    anchors.top: parent.top
                    anchors.topMargin: 6
                }

                HoverHandler { id: hover }

                TapHandler { onTapped: bar.selected(tab.modelData.key) }
            }
        }
    }

    Row {
        id: controls

        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Text {
            visible: !bar.compact && bar.trailing !== ""
            text: bar.trailing
            color: Theme.faint
            font.pixelSize: Theme.fontSmall
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
        }

        // Which shade the interface is painted in, and the one control for it.
        GlyphButton {
            glyph: Theme.mode
            anchors.verticalCenter: parent.verticalCenter
            hint: Theme.mode === "system" ? "Following the desktop. Click for the light theme"
                : Theme.mode === "light" ? "Light theme. Click for the dark one"
                : "Dark theme. Click to follow the desktop again"
            onClicked: Theme.cycle()
        }

        Row {
            spacing: 2
            anchors.verticalCenter: parent.verticalCenter

            GlyphButton {
                glyph: "minimize"
                onClicked: bar.target.showMinimized()
            }

            GlyphButton {
                glyph: bar.target && bar.target.visibility === Window.Maximized ? "restore" : "maximize"
                onClicked: bar.toggleMaximized()
            }

            GlyphButton {
                glyph: "close"
                hoverWash: Theme.danger
                hoverTone: "#ffffff"
                onClicked: bar.target.close()
            }
        }
    }

    function toggleMaximized() {
        if (!bar.target) {
            return
        }

        bar.target.visibility = bar.target.visibility === Window.Maximized
            ? Window.Windowed
            : Window.Maximized
    }
}
