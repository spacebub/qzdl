import QtQuick
import Zdl

Column {
    id: empty

    property string title: ""
    property string body: ""

    spacing: 6

    Text {
        text: empty.title
        color: Theme.muted
        font.pixelSize: Theme.fontMedium
        font.weight: Font.DemiBold
        textFormat: Text.PlainText
    }

    Text {
        text: empty.body
        color: Theme.faint
        font.pixelSize: Theme.fontSmall
        width: empty.width
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }
}
