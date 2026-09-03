import QtQuick
import Zdl

/*
A message buzzed into the corner. Anything that is not an error counts
itself down, and the bar along the bottom edge is that countdown. Pointing
at it holds the countdown where it is.
*/
Item {
    id: buzz

    property int severity: 0        // Notifier.Info | Success | Warning | Error
    property string title: ""
    property string body: ""
    property int duration: 0        // zero: stays until dismissed

    signal dismissed()

    readonly property color tone: severity === 3 ? Theme.danger
                                : severity === 2 ? Theme.warning
                                : severity === 1 ? Theme.success
                                : Theme.accent

    readonly property color wash: severity === 3 ? Theme.dangerSoft
                                : severity === 2 ? Theme.warningSoft
                                : severity === 1 ? Theme.successSoft
                                : Theme.accentSoft

    width: 392
    height: card.height
    opacity: 0
    scale: 0.96

    Component.onCompleted: {
        opacity = 1
        scale = 1
    }

    Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }

    function close() {
        opacity = 0
        scale = 0.96
        exit.start()
    }

    Timer {
        id: exit
        interval: 170
        onTriggered: buzz.dismissed()
    }

    Surface {
        id: card

        width: parent.width
        height: layout.implicitHeight + 26
        clip: true

        // The card takes the colour of what it is saying, border included.
        color: buzz.wash
        border.color: Qt.rgba(buzz.tone.r, buzz.tone.g, buzz.tone.b, 0.42)

        Column {
            id: layout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 16
            anchors.rightMargin: 40
            anchors.topMargin: 13
            spacing: 3

            Row {
                width: parent.width
                visible: buzz.title !== ""
                spacing: 7

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: buzz.tone
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    width: parent.width - 15
                    text: buzz.title
                    color: buzz.tone
                    font.pixelSize: Theme.fontSmall
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Row {
                width: parent.width
                spacing: 7

                // An untitled message carries the severity here instead.
                Rectangle {
                    visible: buzz.title === ""
                    width: 8
                    height: 8
                    radius: 4
                    color: buzz.tone
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    width: parent.width - (buzz.title === "" ? 15 : 0)
                    text: buzz.body
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        GlyphButton {
            glyph: "cross"
            size: 24
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 8
            anchors.topMargin: 9
            tone: Theme.faint
            hoverTone: Theme.text
            hoverWash: "transparent"
            onClicked: buzz.close()
        }

        /*
        The countdown is the bottom edge of the card. Clipping in Qt Quick is
        rectangular and cannot follow a radius, so what gets clipped is a
        rectangle shaped exactly like the card, shifted up until only its
        bottom sliver shows: the ends of the bar are then the card's own
        corners. The window in front of it is what drains.
        */
        Item {
            id: countdown

            visible: buzz.duration > 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 1
            height: 3
            clip: true

            Item {
                id: window

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: countdown.width
                clip: true

                Rectangle {
                    width: countdown.width
                    height: card.height - 2
                    y: -(height - countdown.height)
                    radius: card.radius - 1
                    color: buzz.tone
                }

                NumberAnimation on width {
                    running: buzz.duration > 0 && countdown.width > 0
                    paused: hover.hovered
                    from: countdown.width
                    to: 0
                    duration: Math.max(1, buzz.duration)
                    onFinished: buzz.close()
                }
            }
        }

        HoverHandler {
            id: hover
        }
    }
}
