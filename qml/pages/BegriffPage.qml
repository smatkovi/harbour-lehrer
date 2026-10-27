import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "../stil.js" as Stil
import "../worte.js" as W

// Ein Glossareintrag: das Wort, die Zeichnung dazu, die Erklaerung und die
// Nachbarbegriffe.
//
// Die Seite liegt auf dem Stapel, nicht als Blase ueber der Lektion: Wer
// von "Spreizung" zu "Taupunkt" und von dort zur "Basis" weiterliest, soll
// mit Zurueck Schritt fuer Schritt denselben Weg zurueckgehen und am Ende
// wieder in seiner Lektion stehen.
Page {
    id: seite
    allowedOrientations: Orientation.All

    // Die Kennung kommt beim Oeffnen mit (pageStack.push mit Eigenschaften).
    property string kennung: ""
    property var eintrag: seite.leererEintrag()

    function leererEintrag() {
        return { leer: true, wort: "", definition: "", bild: "", siehe: [] }
    }

    function laden() {
        seite.eintrag = seite.kennung === ""
                        ? seite.leererEintrag()
                        : course.glossaryEntry(seite.kennung)
    }

    // Geladen wird in einer Funktion und nicht als Bindung: beim
    // Sprachwechsel aendert sich die Kennung nicht, wohl aber der Text,
    // und eine Bindung auf glossaryEntry(kennung) merkte das nicht.
    Component.onCompleted: seite.laden()
    onKennungChanged: seite.laden()

    Connections {
        target: course
        onChanged: seite.laden()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: spalte.height + Theme.paddingLarge

        Column {
            id: spalte
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: seite.eintrag.leer ? "" : seite.eintrag.wort
            }

            // Dieselbe Zeichnung wie in der Lektion, in denselben Massen:
            // Bild rechnet seine Hoehe aus der Breite und ausdruecklich
            // nicht aus der implicitHeight des Bildes.
            Bild {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                name: seite.eintrag.leer || seite.eintrag.bild === undefined
                      ? "" : seite.eintrag.bild
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: seite.eintrag.leer ? "" : Stil.reich(seite.eintrag.definition)
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                linkColor: Theme.highlightColor
                // Auch eine Definition verweist weiter -- dann einen
                // Eintrag tiefer auf den Stapel, wie von der Lektion aus.
                onLinkActivated: {
                    if (link.indexOf("begriff:") === 0)
                        pageStack.push(Qt.resolvedUrl("BegriffPage.qml"),
                                       {kennung: link.substring(8)})
                }
            }

            SectionHeader {
                text: W.w("Siehe auch", course.language)
                visible: !seite.eintrag.leer && seite.eintrag.siehe.length > 0
            }

            Repeater {
                model: seite.eintrag.leer ? [] : seite.eintrag.siehe
                Button {
                    x: Theme.horizontalPageMargin
                    width: spalte.width - 2 * Theme.horizontalPageMargin
                    text: modelData.wort
                    onClicked: pageStack.push(Qt.resolvedUrl("BegriffPage.qml"),
                                              {kennung: modelData.id})
                }
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        VerticalScrollDecorator { }
    }
}
