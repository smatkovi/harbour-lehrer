import QtQuick 2.0
import Sailfish.Silica 1.0

// Ein Stueck Programmtext. Feste Schriftbreite, eigener Grund, und -- wenn
// der Laeufer eine Zeilennummer gemeldet hat -- die fehlerhafte Zeile
// hervorgehoben. Das ist der Unterschied zwischen "irgendwo ein Fehler" und
// "hier".
Rectangle {
    id: block
    property string code: ""
    property int fehlerZeile: 0

    color: Theme.rgba(Theme.highlightBackgroundColor, 0.08)
    border.color: Theme.rgba(Theme.highlightColor, 0.25)
    border.width: 1
    radius: Theme.paddingSmall
    height: spalte.height + 2 * Theme.paddingMedium

    Column {
        id: spalte
        x: Theme.paddingMedium
        y: Theme.paddingMedium
        width: parent.width - 2 * Theme.paddingMedium

        Repeater {
            model: block.code.split("\n")
            Label {
                width: parent.width
                text: modelData.length === 0 ? " " : modelData
                font.family: "monospace"
                font.pixelSize: Theme.fontSizeExtraSmall
                color: index + 1 === block.fehlerZeile
                       ? Theme.errorColor : Theme.primaryColor
                textFormat: Text.PlainText
                wrapMode: Text.NoWrap
            }
        }
    }
}
