import QtQuick 2.0
import Sailfish.Silica 1.0
import "../worte.js" as W

// Die erste Seite: wo man steht, und der eine Knopf, der weitermacht.
Page {
    id: seite
    allowedOrientations: Orientation.All

    property var werte: course.stats
    property var weiter: course.nextLesson()

    onStatusChanged: {
        if (status === PageStatus.Active) {
            seite.werte = course.stats
            seite.weiter = course.nextLesson()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: spalte.height + Theme.paddingLarge

        PullDownMenu {
            // Nur bei einem zweisprachigen Kurs. Ein Schalter, der nichts
            // zu schalten hat, ist schlimmer als keiner.
            MenuItem {
                visible: course.languages.length > 1
                text: course.language === "de" ? "English" : "Deutsch"
                onClicked: course.language =
                    (course.language === "de" ? "en" : "de")
            }
            MenuItem {
                text: W.w("Spielwiese", course.language)
                onClicked: pageStack.push(Qt.resolvedUrl("PlaygroundPage.qml"))
            }
            MenuItem {
                text: W.w("Einstufung", course.language)
                onClicked: {
                    course.startPlacement()
                    pageStack.push(Qt.resolvedUrl("PlacementPage.qml"))
                }
            }
            MenuItem {
                text: W.w("Karteikarten", course.language)
                onClicked: {
                    course.startCards()
                    pageStack.push(Qt.resolvedUrl("CardPage.qml"))
                }
            }
        }

        Column {
            id: spalte
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: course.courseTitle
            }

            // Der Untertitel steht als eigene Zeile statt in der Kopfzeile:
            // dort wuerde er abgeschnitten, und er ist der Satz, der sagt,
            // worum es in diesem Kurs ueberhaupt geht.
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: course.courseSubtitle
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }

            Item { width: 1; height: Theme.paddingSmall }

            // Der Stand in einer Zeile: Stufe, erledigte Lektionen, faellige
            // Wiederholungen.
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingLarge

                Column {
                    width: (parent.width - 2 * Theme.paddingLarge) / 3
                    Label {
                        text: seite.werte.level
                        font.pixelSize: Theme.fontSizeLarge
                        color: Theme.highlightColor
                    }
                    Label {
                        text: W.w("Stufe", course.language)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }
                }
                Column {
                    width: (parent.width - 2 * Theme.paddingLarge) / 3
                    Label {
                        text: seite.werte.fertig + "/" + seite.werte.gesamt
                        font.pixelSize: Theme.fontSizeLarge
                        color: Theme.highlightColor
                    }
                    Label {
                        text: W.w("Lektionen", course.language)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }
                }
                Column {
                    width: (parent.width - 2 * Theme.paddingLarge) / 3
                    Label {
                        text: "" + seite.werte.faellig
                        font.pixelSize: Theme.fontSizeLarge
                        color: seite.werte.faellig > 0 ? Theme.highlightColor
                                                       : Theme.secondaryColor
                    }
                    Label {
                        text: W.w("fällig", course.language)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }
                }
            }

            Item { width: 1; height: Theme.paddingMedium }

            // Wer noch nie eingestuft wurde, faengt damit an -- sonst steht
            // hier die naechste Lektion.
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: course.needsPlacement ? W.w("Einstufung beginnen", course.language) : W.w("Weiterlernen", course.language)
                onClicked: {
                    if (course.needsPlacement) {
                        course.startPlacement()
                        pageStack.push(Qt.resolvedUrl("PlacementPage.qml"))
                    } else if (!seite.weiter.leer) {
                        course.startLesson(seite.weiter.id)
                        pageStack.push(Qt.resolvedUrl("LessonPage.qml"))
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !course.needsPlacement && !seite.weiter.leer
                text: seite.weiter.leer ? ""
                      : seite.weiter.kapitel + " · " + seite.weiter.titel
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            SectionHeader { text: W.w("Kurs", course.language) }

            Repeater {
                model: course.chapters()
                ListItem {
                    width: spalte.width
                    contentHeight: kapitelSpalte.height + Theme.paddingMedium

                    Column {
                        id: kapitelSpalte
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        y: Theme.paddingSmall

                        Label {
                            width: parent.width
                            text: modelData.titel
                            color: modelData.stand === "fertig"
                                   ? Theme.secondaryColor : Theme.primaryColor
                            truncationMode: TruncationMode.Fade
                        }
                        Label {
                            width: parent.width
                            text: W.w("Stufe ", course.language) + modelData.stufe + " · "
                                  + modelData.lektionen.length + " Lektionen"
                                  + (modelData.stand === "fertig" ? " · fertig" : "")
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                        }
                    }

                    onClicked: pageStack.push(Qt.resolvedUrl("ChaptersPage.qml"),
                                              {kapitelId: modelData.id})
                }
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        VerticalScrollDecorator { }
    }
}
