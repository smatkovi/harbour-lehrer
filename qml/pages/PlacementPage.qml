import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "../stil.js" as Stil

// Die Einstufung: in etwa zwanzig Fragen herausfinden, wo man anfaengt.
// Angepasst statt fest -- wer die ersten drei Fragen richtig hat, wird nicht
// durch zwoelf weitere gefuehrt, um zu hoeren, dass er nicht bei null steht.
Page {
    id: seite
    allowedOrientations: Orientation.All

    property var frage: course.placementQuestion

    Connections {
        target: course
        onPlacementChanged: seite.frage = course.placementQuestion
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: spalte.height + Theme.paddingLarge

        Column {
            id: spalte
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: course.placementDone ? "Einstufung" : "Frage " + (seite.frage.leer ? "" : seite.frage.nummer)
            }

            // --- Ergebnis -----------------------------------------------
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: course.placementDone

                Label {
                    width: parent.width
                    text: "Stufe " + (course.placementResult.leer ? ""
                                      : course.placementResult.stufe)
                    font.pixelSize: Theme.fontSizeExtraLarge
                    color: Theme.highlightColor
                }

                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    text: course.placementResult.leer ? ""
                          : course.placementResult.richtig + " von "
                            + course.placementResult.gesamt + " richtig. Weiter geht es "
                            + "mit: " + course.placementResult.weiterKapitel + " — "
                            + course.placementResult.weiterLektion
                }

                Label {
                    width: parent.width
                    visible: !course.placementResult.leer
                             && course.placementResult.schwach !== undefined
                             && course.placementResult.schwach.length > 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: course.placementResult.leer ? ""
                          : "Wackelig: " + course.placementResult.schwach.join(", ")
                }

                Button {
                    text: "Antworten durchsehen"
                    onClicked: pageStack.push(Qt.resolvedUrl("ReviewPage.qml"))
                }

                Button {
                    text: "Dort anfangen"
                    onClicked: {
                        if (!course.placementResult.leer) {
                            course.startLesson(course.placementResult.weiterId)
                            pageStack.replace(Qt.resolvedUrl("LessonPage.qml"))
                        }
                    }
                }
            }

            // --- Frage --------------------------------------------------
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !course.placementDone && !seite.frage.leer
                text: seite.frage.leer ? "" : Stil.reich(seite.frage.frage)
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                color: Theme.primaryColor
            }

            Bild {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                name: seite.frage.leer || seite.frage.bild === undefined
                      ? "" : seite.frage.bild
            }

            CodeBlock {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !course.placementDone && !seite.frage.leer
                         && seite.frage.code !== undefined && seite.frage.code !== ""
                code: seite.frage.leer || seite.frage.code === undefined
                      ? "" : seite.frage.code
            }

            Repeater {
                model: (!course.placementDone && !seite.frage.leer)
                       ? seite.frage.optionen : []

                ListItem {
                    width: spalte.width
                    contentHeight: wahl.height + Theme.paddingLarge

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.horizontalPageMargin
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.topMargin: Theme.paddingSmall / 2
                        anchors.bottomMargin: Theme.paddingSmall / 2
                        radius: Theme.paddingSmall
                        color: Theme.rgba(Theme.highlightBackgroundColor, 0.10)
                        border.width: 1
                        border.color: Theme.rgba(Theme.highlightColor, 0.2)
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

                    onClicked: course.answerPlacement(index)
                }
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        VerticalScrollDecorator { }
    }
}
