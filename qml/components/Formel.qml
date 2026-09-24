import QtQuick 2.0
import QtGraphicalEffects 1.0
import Sailfish.Silica 1.0
import "../stil.js" as Stil

// Eine Formel, zweimal: oben die Zeile so, wie sie im Beispielprogramm
// steht, darunter dieselbe Sache gesetzt.
//
// Das Gesetzte ist ein Bild, kein Text -- weder Silica noch Rich Text
// koennen einen Bruchstrich oder ein Summenzeichen mit Grenzen. Gesetzt
// wird beim Bauen (c-lehrer/tools/formeln.py), hier haengt nur ein PNG.
//
// Eingefaerbt wird es, statt die Farbe mitzubacken: Die Umgebung kann hell
// oder dunkel sein, und ein helles Bild waere auf hellem Grund unsichtbar.
// Das PNG traegt deshalb nur seinen Alphakanal zur Sache bei, die Farbe
// kommt aus dem Thema.
//
// Die Groesse steht im Kurs, in Punkten. Sie aus dem Bild zu lesen waere
// eine Bindungsschleife -- dieselbe Falle wie in Bild.qml.
Rectangle {
    id: rahmen

    property var formel: undefined
    property real bildBreite: formel === undefined ? 0 : (formel.breite || 0)
    property real bildHoehe: formel === undefined ? 0 : (formel.hoehe || 0)
    // Verkleinern, wenn zu breit; kleine Formeln bleiben klein.
    property real skala: bildBreite > 0
                         ? Math.min(Theme.pixelRatio,
                                    (width - 2 * Theme.paddingMedium) / bildBreite)
                         : 1.0

    visible: formel !== undefined && formel.code !== undefined
    color: Theme.rgba(Theme.highlightBackgroundColor, 0.08)
    border.color: Theme.rgba(Theme.highlightColor, 0.25)
    border.width: 1
    radius: Theme.paddingSmall
    height: visible ? spalte.height + 2 * Theme.paddingMedium : 0

    Column {
        id: spalte
        x: Theme.paddingMedium
        y: Theme.paddingMedium
        width: parent.width - 2 * Theme.paddingMedium
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            text: rahmen.formel === undefined ? "" : rahmen.formel.code
            font.family: "monospace"
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            textFormat: Text.PlainText
            truncationMode: TruncationMode.Fade
        }

        Item {
            visible: rahmen.bildBreite > 0
            width: Math.round(rahmen.bildBreite * rahmen.skala)
            height: Math.round(rahmen.bildHoehe * rahmen.skala)

            Image {
                id: gesetzt
                anchors.fill: parent
                source: rahmen.formel === undefined
                        || rahmen.formel.bild === undefined
                        ? "" : bilderPfad + rahmen.formel.bild + ".png"
                smooth: true
                asynchronous: true
                visible: false          // sichtbar wird die eingefaerbte Fassung
            }
            ColorOverlay {
                anchors.fill: parent
                source: gesetzt
                color: Theme.primaryColor
                cached: true
            }
        }

        Label {
            width: parent.width
            visible: text !== ""
            text: rahmen.formel === undefined ? "" : (rahmen.formel.untertitel || "")
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }

        // Was die Zeichen bedeuten -- der Kasten steht fuer sich.
        Label {
            width: parent.width
            visible: text !== ""
            text: rahmen.formel === undefined
                  ? "" : Stil.reich(rahmen.formel.zeichen || "")
            textFormat: Text.RichText
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }

        // Warum das dasteht -- woher der Faktor kommt und wo er aufhoert zu
        // gelten. Die Zeile darueber sagt nur, was es ist.
        Label {
            width: parent.width
            visible: text !== ""
            text: rahmen.formel === undefined
                  ? "" : Stil.reich(rahmen.formel.erklaerung || "")
            textFormat: Text.RichText
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }
    }
}
