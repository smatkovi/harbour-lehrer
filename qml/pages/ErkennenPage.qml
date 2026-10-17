import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "../stil.js" as Stil
import "../worte.js" as W

// Wolken erkennen: ein Foto, zwei Fragen, und das viele Male.
//
// Anders als die Einstufung antwortet diese Seite nach jedem Tipp. Beim
// Einstufen waere das falsch -- dort wird gemessen, und wer die Loesung
// sieht, lernt waehrend der Messung dazu. Hier ist das Lernen der Zweck:
// Ohne den Satz "daran haettest du es gesehen" bleibt vom Danebentippen
// nur das Danebentippen.
Page {
    id: seite
    allowedOrientations: Orientation.All

    property var frage: course.recognitionQuestion
    property var ergebnis: course.recognitionResult

    Connections {
        target: course
        onRecognitionChanged: {
            seite.frage = course.recognitionQuestion
            seite.ergebnis = course.recognitionResult
        }
    }

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: spalte.height + Theme.paddingLarge

        Column {
            id: spalte
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: course.recognitionDone
                       ? W.w("Durch", course.language)
                       : W.w("Bild ", course.language)
                         + (seite.frage.leer ? "" : seite.frage.nummer)
                         + W.w(" von ", course.language)
                         + (seite.frage.leer ? "" : seite.frage.gesamt)
            }

            // --- die Uebung ---------------------------------------------
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: !course.recognitionDone && !seite.frage.leer

                // Die Hoehe kommt aus den mitgelieferten Massen, nicht aus
                // implicitHeight -- das waere die Bindungsschleife aus
                // Bild.qml.
                Rectangle {
                    width: parent.width
                    height: (seite.frage.leer || seite.frage.foto === undefined
                             || !seite.frage.foto.breite)
                            ? Math.round(parent.width * 0.7)
                            : Math.round(parent.width * seite.frage.foto.hoehe
                                         / seite.frage.foto.breite)
                    color: Theme.rgba(Theme.highlightBackgroundColor, 0.06)
                    border.color: Theme.rgba(Theme.highlightColor, 0.2)
                    border.width: 1
                    radius: Theme.paddingSmall
                    clip: true

                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: (seite.frage.leer
                                 || seite.frage.foto === undefined)
                                ? "" : Stil.bilderPfad + seite.frage.foto.datei
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        asynchronous: true
                    }
                }

                // Urheber und Lizenz: Die Fotos stehen unter CC BY und
                // CC BY-SA, und beide verlangen genau diese Zeile.
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: (seite.frage.leer || seite.frage.foto === undefined)
                          ? "" : seite.frage.foto.autor + " · "
                                 + seite.frage.foto.lizenz
                                 + " · Wikimedia Commons"
                    font.pixelSize: Theme.fontSizeTiny
                    color: Theme.secondaryColor
                }

                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: seite.frage.leer ? ""
                          : (seite.frage.stufe === 0
                             ? W.w("Was für eine Wolke ist das?", course.language)
                             : W.w("Und was heißt sie fürs Segelfliegen?",
                                   course.language))
                    color: Theme.primaryColor
                }
            }

            Repeater {
                model: (!course.recognitionDone && !seite.frage.leer)
                       ? seite.frage.optionen : []

                ListItem {
                    width: spalte.width
                    contentHeight: wahl.height + Theme.paddingLarge
                    enabled: !seite.frage.geprueft

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.horizontalPageMargin
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.topMargin: Theme.paddingSmall / 2
                        anchors.bottomMargin: Theme.paddingSmall / 2
                        radius: Theme.paddingSmall
                        color: {
                            if (!seite.frage.geprueft)
                                return Theme.rgba(Theme.highlightBackgroundColor, 0.10)
                            if (index === seite.frage.antwort)
                                return Theme.rgba("#7ee787", 0.18)
                            if (index === seite.frage.gewaehlt)
                                return Theme.rgba("#ff6b6b", 0.18)
                            return Theme.rgba(Theme.highlightBackgroundColor, 0.10)
                        }
                        border.width: 1
                        border.color: {
                            if (seite.frage.geprueft
                                    && index === seite.frage.antwort)
                                return "#7ee787"
                            if (seite.frage.geprueft
                                    && index === seite.frage.gewaehlt)
                                return "#ff6b6b"
                            return Theme.rgba(Theme.highlightColor, 0.2)
                        }
                    }

                    Label {
                        id: wahl
                        x: Theme.horizontalPageMargin + Theme.paddingMedium
                        y: Theme.paddingMedium
                        width: parent.width - 2 * Theme.horizontalPageMargin
                               - 2 * Theme.paddingMedium
                        text: modelData
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.primaryColor
                    }

                    onClicked: course.answerRecognition(index)
                }
            }

            // --- woran man es sieht --------------------------------------
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingSmall
                visible: !course.recognitionDone && !seite.frage.leer
                         && seite.frage.geprueft === true

                Label {
                    text: seite.frage.richtig ? W.w("Richtig", course.language)
                                              : W.w("Daneben", course.language)
                    font.pixelSize: Theme.fontSizeSmall
                    color: seite.frage.richtig ? "#7ee787" : "#ff6b6b"
                }
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: seite.frage.geprueft === true ? seite.frage.name : ""
                    color: Theme.highlightColor
                }
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: seite.frage.geprueft === true
                          ? seite.frage.erklaerung : ""
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                }
                Button {
                    text: seite.frage.stufe === 0
                          ? W.w("Und was heißt das?", course.language)
                          : W.w("Nächstes Bild", course.language)
                    onClicked: {
                        course.nextRecognition()
                        flick.contentY = 0
                    }
                }
            }

            // --- das Ergebnis --------------------------------------------
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: course.recognitionDone

                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: W.w("Wolke erkannt: ", course.language)
                          + seite.ergebnis.art
                          + W.w(" von ", course.language)
                          + seite.ergebnis.gesamt + ". "
                          + W.w("Bedeutung gewusst: ", course.language)
                          + seite.ergebnis.sinn
                          + W.w(" von ", course.language)
                          + seite.ergebnis.gesamt + "."
                    color: Theme.primaryColor
                }

                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    visible: seite.ergebnis.schwach !== undefined
                             && seite.ergebnis.schwach.length > 0
                    text: W.w("Das sieh dir noch einmal an:", course.language)
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                }

                Repeater {
                    model: seite.ergebnis.schwach === undefined
                           ? [] : seite.ergebnis.schwach

                    Column {
                        width: parent.width
                        spacing: Theme.paddingSmall / 2

                        Label {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: modelData.name + "  (" + modelData.richtig
                                  + "/" + modelData.gesamt + ")"
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        Label {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: modelData.erkennen
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                        }
                    }
                }

                Button {
                    text: W.w("Noch einmal", course.language)
                    onClicked: course.startRecognition(seite.ergebnis.gesamt)
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
