import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "../stil.js" as Stil
import "../worte.js" as W

// Die Lektion: ein Gedanke, ein Beispiel, das wirklich laeuft, dann die
// Aufgaben. Das Beispiel laufen zu lassen, bevor irgendetwas gefragt wird,
// ist Absicht -- man soll die Maschine zuerst arbeiten sehen.
Page {
    id: seite
    allowedOrientations: Orientation.All

    property var lektion: course.lesson

    Connections {
        target: course
        onLessonChanged: seite.lektion = course.lesson
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: spalte.height + Theme.paddingLarge

        Column {
            id: spalte
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: seite.lektion.leer ? "" : seite.lektion.titel
                description: seite.lektion.leer ? "" : seite.lektion.kapitel
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: seite.lektion.leer ? "" : Stil.reich(seite.lektion.text)
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                linkColor: Theme.highlightColor
            }

            Bild {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                name: seite.lektion.leer || seite.lektion.bild === undefined
                      ? "" : seite.lektion.bild
            }

            SectionHeader {
                text: W.w("Beispiel", course.language)
                visible: !seite.lektion.leer && seite.lektion.beispiel !== ""
            }

            CodeBlock {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !seite.lektion.leer && seite.lektion.beispiel !== ""
                code: seite.lektion.leer ? "" : seite.lektion.beispiel
            }

            // Laeuft die Sprache auf diesem Geraet? C und Rust bringen ihre
            // Deuter mit; C++ und Python koennen hier nur gelesen werden,
            // und dann steht das auch da, statt einen Knopf anzubieten,
            // hinter dem nichts passiert.
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: !seite.lektion.leer && seite.lektion.beispiel !== ""
                         && seite.lektion.laeuft

                Button {
                    width: (parent.width - Theme.paddingMedium) * 0.62
                    text: course.running ? W.w("läuft …", course.language) : W.w("Ausführen", course.language)
                    enabled: !course.running
                    onClicked: course.runCode(seite.lektion.beispiel, 30)
                }
                Button {
                    width: (parent.width - Theme.paddingMedium) * 0.38
                    text: W.w("Stopp", course.language)
                    enabled: course.running
                    onClicked: course.stopRun()
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !seite.lektion.leer && seite.lektion.beispiel !== ""
                         && !seite.lektion.laeuft
                text: seite.lektion.sprache === "cpp"
                      ? W.w("C++ hat auf diesem Gerät keinen Übersetzer — diese Lektion wird gelesen und vorhergesagt.", course.language)
                      : (seite.lektion.sprache === "python"
                         ? W.w("Für dieses Kapitel fehlt NumPy auf dem Gerät — die Lektion bleibt lesbar.", course.language)
                         : W.w("Diese Sprache lässt sich hier nicht ausführen.", course.language))
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }

            OutputBlock {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: course.output === "" && course.error === "" && !course.running
                         && !seite.lektion.leer && seite.lektion.ausgabe !== ""
                text: W.w("Erwartet:", course.language) + "\n"
                      + (seite.lektion.leer ? "" : seite.lektion.ausgabe)
                font.family: "monospace"
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            Item { width: 1; height: Theme.paddingMedium }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !seite.lektion.leer && seite.lektion.aufgaben > 0
                text: W.w("Zu den Aufgaben (", course.language) + (seite.lektion.leer ? 0 : seite.lektion.aufgaben) + ")"
                onClicked: {
                    course.toExercises()
                    pageStack.push(Qt.resolvedUrl("ExercisePage.qml"))
                }
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        VerticalScrollDecorator { }
    }
}
