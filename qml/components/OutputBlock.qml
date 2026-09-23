import QtQuick 2.0
import Sailfish.Silica 1.0

// Was der Lauf gesagt hat: Text, Fehler, Bild. Das Bild kommt aus dem
// C++-Teil ueber den Bildlieferanten "plot"; die Nummer dahinter wechselt
// nach jedem Lauf, damit QML nicht das alte aus dem Zwischenspeicher zeigt.
Column {
    id: ausgabe
    spacing: Theme.paddingMedium

    Label {
        width: parent.width
        visible: course.error !== ""
        text: course.error
        font.family: "monospace"
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.errorColor
        wrapMode: Text.Wrap
    }

    Rectangle {
        width: parent.width
        visible: course.output !== ""
        height: text.height + 2 * Theme.paddingMedium
        radius: Theme.paddingSmall
        color: Theme.rgba(Theme.highlightBackgroundColor, 0.12)

        Label {
            id: text
            x: Theme.paddingMedium
            y: Theme.paddingMedium
            width: parent.width - 2 * Theme.paddingMedium
            text: course.output
            font.family: "monospace"
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.primaryColor
            wrapMode: Text.Wrap
        }
    }

    Image {
        width: parent.width
        visible: course.hasPlot
        fillMode: Image.PreserveAspectFit
        cache: false
        source: course.hasPlot ? "image://plot/" + course.plotRevision : ""
    }

    Label {
        width: parent.width
        visible: course.seconds > 0
        text: course.seconds.toFixed(2) + " s"
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
    }
}
