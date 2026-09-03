import QtQuick
import Zdl
import Zdl.Components

// Anything that cannot be taken back is confirmed here, in the window rather
// than in a box of its own.
Item {
    id: sheet

    property string title: ""
    property string body: ""
    property string acceptText: "Continue"
    property bool danger: false
    property var accepted: null

    visible: false

    function ask(title, body, acceptText, danger, onAccepted) {
        sheet.title = title
        sheet.body = body
        sheet.acceptText = acceptText
        sheet.danger = danger === true
        sheet.accepted = onAccepted
        sheet.visible = true
    }

    function dismiss() {
        sheet.visible = false
        sheet.accepted = null
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim

        MouseArea {
            anchors.fill: parent
            onClicked: sheet.dismiss()
        }
    }

    Surface {
        id: card
        anchors.centerIn: parent
        width: Math.min(460, parent.width - 48)
        height: content.implicitHeight + 44

        scale: sheet.visible ? 1 : 0.95
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22
            spacing: 10

            Text {
                width: parent.width
                text: sheet.title
                color: Theme.text
                font.pixelSize: Theme.fontLarge
                font.weight: Theme.headingWeight
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }

            Text {
                width: parent.width
                text: sheet.body
                color: Theme.muted
                font.pixelSize: Theme.fontBody
                lineHeight: 1.35
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }

            Item {
                width: parent.width
                height: 8
            }

            Row {
                anchors.right: parent.right
                spacing: 8

                AppButton {
                    text: "Cancel"
                    onClicked: sheet.dismiss()
                }

                AppButton {
                    text: sheet.acceptText
                    variant: sheet.danger ? "danger" : "primary"
                    onClicked: {
                        const callback = sheet.accepted
                        sheet.dismiss()

                        if (callback) {
                            callback()
                        }
                    }
                }
            }
        }
    }

    Keys.onEscapePressed: sheet.dismiss()
}
