import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "../stil.js" as Stil

// Karteikarten: die Begriffe aus erledigten Lektionen, in wachsenden
// Abstaenden wiederholt. Eine Karte zeigt erst die Frage, dann -- auf
// Tippen -- die Antwort; bewertet wird selbst, denn nur der Lernende weiss,
// ob er es wusste oder geraten hat.
Page {
    id: seite
    allowedOrientations: Orientation.All

    property var karte: course.card
    property bool aufgedeckt: false

    Connections {
        target: course
        onCardChanged: {
            seite.karte = course.card
            seite.aufgedeckt = false
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: spalte.height + Theme.paddingLarge

        Column {
            id: spalte
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Karteikarten"
                description: course.cardStats.offen + " offen · "
                             + course.cardStats.erledigt + " heute"
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: course.cardsDone
                wrapMode: Text.WordWrap
                color: Theme.primaryColor
                text: "Für heute ist nichts mehr fällig. Die nächsten Karten "
                      + "kommen von selbst wieder — in ein paar Tagen, je nachdem, "
                      + "wie sicher sie saßen."
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !seite.karte.leer
                text: seite.karte.leer ? "" : Stil.reich(seite.karte.frage)
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                color: Theme.primaryColor
            }

            Bild {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                name: seite.karte.leer || seite.karte.bild === undefined
                      ? "" : seite.karte.bild
            }

            CodeBlock {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !seite.karte.leer && seite.karte.code !== undefined
                         && seite.karte.code !== ""
                code: seite.karte.leer || seite.karte.code === undefined
                      ? "" : seite.karte.code
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !seite.karte.leer && !seite.aufgedeckt
                text: "Umdrehen"
                onClicked: seite.aufgedeckt = true
            }

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: !seite.karte.leer && seite.aufgedeckt

                Label {
                    width: parent.width
                    text: seite.karte.leer || seite.karte.antwort === undefined
                          ? "" : Stil.reich("" + seite.karte.antwort)
                    textFormat: Text.RichText
                    wrapMode: Text.WordWrap
                    color: Theme.highlightColor
                }

                Label {
                    width: parent.width
                    visible: seite.karte.warum !== undefined && seite.karte.warum !== ""
                    text: seite.karte.leer ? "" : Stil.reich(seite.karte.warum)
                    textFormat: Text.RichText
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }

                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium

                    Button {
                        width: (parent.width - Theme.paddingMedium) / 2
                        text: "Wusste ich nicht"
                        onClicked: {
                            course.answerCard(false)
                            seite.aufgedeckt = false
                        }
                    }
                    Button {
                        width: (parent.width - Theme.paddingMedium) / 2
                        text: "Gewusst"
                        onClicked: {
                            course.answerCard(true)
                            seite.aufgedeckt = false
                        }
                    }
                }
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        VerticalScrollDecorator { }
    }
}
