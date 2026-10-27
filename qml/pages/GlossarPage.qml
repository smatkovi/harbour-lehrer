import QtQuick 2.0
import Sailfish.Silica 1.0
import "../worte.js" as W

// Das Glossar: jedes Fachwort des Kurses an einer Stelle, alphabetisch.
//
// Gebraucht wird es mitten in einer Lektion ("was war Spreizung?"), und
// dann hilft kein Scrollen durch hundert Woerter -- darum das Suchfeld
// gleich unter der Kopfzeile.
Page {
    id: seite
    allowedOrientations: Orientation.All

    // Was im Suchfeld steht. Als Eigenschaft der Seite und nicht als
    // Kennung im Kopf der Liste: der Kopf ist eine eigene Komponente, und
    // von aussen kommt man an seine Kennungen nicht heran.
    property string suche: ""

    // Die Liste kommt uebersetzt und sortiert aus Course::glossary(); hier
    // wird sie nur gefiltert. Sie liegt in einem ListModel und nicht als
    // JS-Feld am Modell, weil section.property einen Rollennamen braucht --
    // bei einem Feld aus Objekten gibt es nur modelData. Die Rolle heisst
    // "kennung" und nicht "id": "id" ist in einem Delegate schon vergeben.
    ListModel { id: gefundene }

    // Umlaute zaehlen unter ihrem Grundbuchstaben, damit "Überentwicklung"
    // bei U steht und nicht in einem eigenen Abschnitt hinter Z -- dieselbe
    // Rechnung wie beim Sortieren in Course::glossary().
    function anfangsbuchstabe(wort) {
        var z = wort.substring(0, 1).toUpperCase()
        if (z === "Ä") return "A"
        if (z === "Ö") return "O"
        if (z === "Ü") return "U"
        return z
    }

    // Gesucht wird nach Wortanfang UND Teilwort: wer "spreiz" tippt, will
    // die Spreizung, und wer "nimbus" tippt, will auch Cumulonimbus
    // finden. Die Reihenfolge bleibt alphabetisch -- sonst sprengen die
    // Abschnittsbuchstaben auseinander und stehen zweimal da.
    function fuellen() {
        var alle = course.glossary
        var gesucht = seite.suche.toLowerCase()
        gefundene.clear()
        for (var i = 0; i < alle.length; i++) {
            var wort = alle[i].wort
            if (gesucht !== "" && wort.toLowerCase().indexOf(gesucht) < 0)
                continue
            gefundene.append({ kennung: alle[i].id, wort: wort,
                               buchstabe: seite.anfangsbuchstabe(wort) })
        }
    }

    Component.onCompleted: seite.fuellen()

    // Beim Sprachwechsel sind die Woerter andere und anders sortiert.
    Connections {
        target: course
        onChanged: seite.fuellen()
    }

    SilicaListView {
        id: liste
        anchors.fill: parent
        model: gefundene

        header: Column {
            width: liste.width

            PageHeader {
                title: W.w("Glossar", course.language)
                description: course.glossary.length + " "
                             + W.w("Begriffe", course.language)
            }

            SearchField {
                width: parent.width
                placeholderText: W.w("Wort suchen", course.language)
                onTextChanged: {
                    seite.suche = text
                    seite.fuellen()
                }
            }
        }

        section.property: "buchstabe"
        section.delegate: SectionHeader { text: section }

        delegate: ListItem {
            width: liste.width
            contentHeight: zeile.height + Theme.paddingMedium

            Label {
                id: zeile
                x: Theme.horizontalPageMargin
                y: Theme.paddingSmall
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: wort
                wrapMode: Text.WordWrap
                color: highlighted ? Theme.highlightColor : Theme.primaryColor
            }

            onClicked: pageStack.push(Qt.resolvedUrl("BegriffPage.qml"),
                                      {kennung: kennung})
        }

        ViewPlaceholder {
            enabled: gefundene.count === 0
            text: W.w("Nichts gefunden", course.language)
        }

        VerticalScrollDecorator { }
    }
}
