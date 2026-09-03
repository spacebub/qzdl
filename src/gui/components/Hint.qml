import QtQuick
import QtQuick.Controls.Basic
import Zdl

// A word about what a control does, for the ones where it is not obvious.
ToolTip {
    id: hint

    delay: 450
    padding: 10

    background: Rectangle {
        color: Theme.raised
        radius: Theme.radiusSmall
        border.width: 1
        border.color: Theme.borderStrong
    }

    contentItem: Text {
        text: hint.text
        color: Theme.text
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }
}
