import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "../stil.js" as Stil
import "../worte.js" as W

// Die Durchsicht nach der Einstufung. Eine Einstufung, die nur eine Zahl
// nennt, lehrt nichts -- hier steht zu jeder Frage, was richtig war und
// warum.
Page {
    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent
        model: course.placementReview()

        header: PageHeader { title: W.w("Durchsicht", course.language) }

        delegate: Column {
            width: parent.width
            spacing: Theme.paddingSmall

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: modelData.thema + " "
                      + W.w("· Stufe ", course.language) + modelData.stufe
                font.pixelSize: Theme.fontSizeExtraSmall
                color: modelData.korrekt ? "#7ee787" : Theme.errorColor
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: Stil.reich(modelData.frage)
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
            }

            CodeBlock {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: modelData.code !== ""
                code: modelData.code
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: W.w("Richtig: ", course.language) + modelData.optionen[modelData.richtig]
                      + (modelData.korrekt ? ""
                         : "\n" + W.w("Deine Wahl: ", course.language)
                           + modelData.optionen[modelData.gewaehlt])
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: Stil.reich(modelData.warum)
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }

            // `bottomPadding` gibt es erst ab QtQuick 2.6, und mit ihm lud
            // die ganze Seite nicht ("not available in QtQuick 2.0").
            Item { width: 1; height: Theme.paddingLarge }
        }

        VerticalScrollDecorator { }
    }
}
