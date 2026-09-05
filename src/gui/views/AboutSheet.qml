import QtQuick
import Zdl
import Zdl.Components

// Who wrote this, what it is built on, and where its config file lives.
Item {
    id: sheet

    visible: false

    function show() {
        sheet.visible = true
    }

    function dismiss() {
        sheet.visible = false
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim

        /*
        A sheet covers the window, so nothing underneath it should react to
        anything. Without hoverEnabled the hover still goes through and the
        page below lights up under a pointer that is over a sheet, and a
        button this does not claim is a click the page below still gets.
        */
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onClicked: sheet.dismiss()
        }
    }

    Surface {
        anchors.centerIn: parent
        width: Math.min(520, parent.width - 48)
        height: content.implicitHeight + 44

        scale: sheet.visible ? 1 : 0.95
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22
            spacing: 16

            Row {
                width: parent.width
                spacing: 14

                Image {
                    width: 56
                    height: 56
                    source: "qrc:/qzdl-128.png"
                    sourceSize.width: 112
                    sourceSize.height: 112
                    smooth: true
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    width: parent.width - 56 - close.width - 2 * parent.spacing
                    spacing: 2
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: "ZDL"
                        color: Theme.text
                        font.pixelSize: Theme.fontDisplay
                        font.weight: Theme.headingWeight
                        textFormat: Text.PlainText
                    }

                    Text {
                        text: "Version " + App.version + " · Qt " + App.qtVersion
                        color: Theme.faint
                        font.pixelSize: Theme.fontSmall
                        textFormat: Text.PlainText
                    }

                    Text {
                        width: parent.width
                        text: "A launcher for ZDoom based Doom engine source ports."
                        color: Theme.muted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        textFormat: Text.PlainText
                    }
                }

                GlyphButton {
                    id: close
                    glyph: "cross"
                    size: 26
                    anchors.top: parent.top
                    onClicked: sheet.dismiss()
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.border
            }

            Column {
                width: parent.width
                spacing: 3

                SectionLabel { text: "COPYRIGHT" }

                Repeater {
                    model: [
                        "© 2023-2026 spacebub",
                        "© 2018-2019 Lcferrum",
                        "© 2004-2012 ZDL Software Foundation"
                    ]

                    delegate: Text {
                        required property string modelData

                        text: modelData
                        color: Theme.muted
                        font.pixelSize: Theme.fontSmall
                        textFormat: Text.PlainText
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 3

                SectionLabel { text: "THANKS" }

                Text {
                    width: parent.width
                    text: "BioHazard, for the original version. NeuralStunner, without whose help none "
                        + "of this would be possible. Blzut3, Risen, Enjay, DRDTeam.org and ZDoom.org."
                    color: Theme.muted
                    font.pixelSize: Theme.fontSmall
                    lineHeight: 1.3
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }
            }

            Column {
                width: parent.width
                spacing: 4

                SectionLabel { text: "CONFIGURATION FILE" }

                PathLabel {
                    width: parent.width
                    path: App.config.path
                    room: parent.width
                    clickable: true
                    opens: App.directoryOf(App.config.path)
                    font.pixelSize: Theme.fontSmall
                }
            }

            // Anchored rather than laid out: the two ends hold whatever the
            // labels measure.
            Item {
                width: parent.width
                height: Math.max(page.height, dismiss.height)

                AppButton {
                    id: page
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Project page"
                    variant: "ghost"
                    compact: true
                    onClicked: Qt.openUrlExternally("https://github.com/spacebub/qzdl")
                }

                AppButton {
                    id: dismiss
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Close"
                    variant: "primary"
                    compact: true
                    onClicked: sheet.dismiss()
                }
            }
        }
    }

    Keys.onEscapePressed: sheet.dismiss()
}
