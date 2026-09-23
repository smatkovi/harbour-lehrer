import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "../stil.js" as Stil

// Die Aufgaben einer Lektion. Sechs Arten, eine Seite: lesen und vorhersagen,
// auswaehlen, Luecke fuellen, Zeilen ordnen, eine Zahl schaetzen, selbst
// schreiben. Sie steigen in dieser Reihenfolge im Aufwand -- das Beispiel
// wird ausgeblendet, nicht weggenommen.
Page {
    id: seite
    allowedOrientations: Orientation.All

    property var aufgabe: course.exercise
    property var loesungsZeilen: []
    property var vorratsZeilen: []
    property string editorText: ""
    property bool editorGefuellt: false

    function uebernehmen() {
        seite.aufgabe = course.exercise
        if (seite.aufgabe.leer)
            return
        if (seite.aufgabe.art === "parsons" && seite.loesungsZeilen.length === 0)
            seite.vorratsZeilen = seite.aufgabe.gemischt
        if (seite.aufgabe.art === "code" && !seite.editorGefuellt) {
            seite.editorText = seite.aufgabe.vorlage
            seite.editorGefuellt = true
        }
    }

    // Was "die richtige Loesung" heisst, haengt an der Art der Aufgabe:
    // bei der Auswahl die richtige Moeglichkeit, bei der Vorhersage die
    // Ausgabe, bei der Luecke das fehlende Stueck, beim Ordnen die
    // Reihenfolge.
    function loesungstext() {
        var a = seite.aufgabe
        if (a.leer)
            return ""
        if (a.art === "mc")
            return "Richtig: " + a.optionen[a.antwort]
        if (a.art === "predict")
            return "Richtig wäre:\n" + (a.antwort === undefined ? "" : a.antwort)
        if (a.art === "blank")
            return "Richtig: " + (a.antworten === undefined ? "" : a.antworten.join("   "))
        if (a.art === "parsons")
            return "Richtige Reihenfolge:\n"
                   + (a.zeilen === undefined ? "" : a.zeilen.join("\n"))
        return ""
    }

    // Vor einem zweiten Anlauf wird geleert, was sonst die alte Antwort
    // stehen laesst; die zusammengebauten Zeilen bleiben, dort will man
    // umstellen.
    function zuruecksetzen() {
        if (seite.aufgabe.art === "predict")
            vorhersage.text = ""
    }

    function weiter() {
        course.nextExercise()
        seite.loesungsZeilen = []
        seite.vorratsZeilen = []
        seite.editorGefuellt = false
        seite.editorText = ""
        uebernehmen()
    }

    Component.onCompleted: uebernehmen()

    Connections {
        target: course
        onLessonChanged: seite.uebernehmen()
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
                title: seite.aufgabe.leer ? "Geschafft"
                       : "Aufgabe " + seite.aufgabe.nummer + " von " + seite.aufgabe.gesamt
            }

            // --- Fertig -------------------------------------------------
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: seite.aufgabe.leer
                wrapMode: Text.WordWrap
                color: Theme.primaryColor
                text: seite.aufgabe.leer && seite.aufgabe.beantwortet > 0
                      ? seite.aufgabe.richtigGesamt + " von " + seite.aufgabe.beantwortet
                        + " Aufgaben auf Anhieb richtig. Die Lektion kommt zur "
                        + "Wiederholung wieder — in ein paar Tagen, je nachdem, "
                        + "wie es lief."
                      : "Diese Lektion ist durch."
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: seite.aufgabe.leer
                text: "Zurück zum Kurs"
                onClicked: pageStack.pop(pageStack.find(function (p) {
                    return p.objectName === "startPage"
                }) || null)
            }

            // --- Frage --------------------------------------------------
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !seite.aufgabe.leer
                text: seite.aufgabe.leer ? "" : Stil.reich(seite.aufgabe.frage)
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                color: Theme.primaryColor
            }

            Bild {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                name: seite.aufgabe.leer || seite.aufgabe.bild === undefined
                      ? "" : seite.aufgabe.bild
            }

            CodeBlock {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !seite.aufgabe.leer && seite.aufgabe.art !== "code"
                         && seite.aufgabe.art !== "parsons"
                         && seite.aufgabe.code !== undefined && seite.aufgabe.code !== ""
                code: seite.aufgabe.leer || seite.aufgabe.code === undefined
                      ? "" : seite.aufgabe.code
            }

            // --- Auswahl ------------------------------------------------
            Column {
                width: parent.width
                spacing: Theme.paddingSmall
                visible: !seite.aufgabe.leer && seite.aufgabe.art === "mc"

                Repeater {
                    model: (!seite.aufgabe.leer && seite.aufgabe.art === "mc")
                           ? seite.aufgabe.optionen : []

                    ListItem {
                        width: spalte.width
                        contentHeight: wahlText.height + Theme.paddingLarge
                        enabled: !seite.aufgabe.geprueft
                        highlighted: down

                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.horizontalPageMargin
                            anchors.rightMargin: Theme.horizontalPageMargin
                            anchors.topMargin: Theme.paddingSmall / 2
                            anchors.bottomMargin: Theme.paddingSmall / 2
                            radius: Theme.paddingSmall
                            // Die richtige Moeglichkeit wird erst gruen,
                            // wenn die Loesung aufgedeckt ist -- sonst waere
                            // der zweite Anlauf keiner. Die eigene falsche
                            // Wahl steht dagegen sofort da.
                            color: {
                                if (!seite.aufgabe.geprueft)
                                    return Theme.rgba(Theme.highlightBackgroundColor, 0.10)
                                if ((seite.aufgabe.loesungZeigen || seite.aufgabe.richtig)
                                        && index === seite.aufgabe.antwort)
                                    return Theme.rgba("#4caf50", 0.25)
                                if (index === seite.aufgabe.gewaehlt && !seite.aufgabe.richtig)
                                    return Theme.rgba(Theme.errorColor, 0.2)
                                return Theme.rgba(Theme.highlightBackgroundColor, 0.06)
                            }
                            border.width: 1
                            border.color: Theme.rgba(Theme.highlightColor, 0.2)
                        }

                        Label {
                            id: wahlText
                            x: Theme.horizontalPageMargin + Theme.paddingMedium
                            y: Theme.paddingMedium
                            width: parent.width - 2 * Theme.horizontalPageMargin
                                   - 2 * Theme.paddingMedium
                            text: modelData
                            wrapMode: Text.WordWrap
                            color: Theme.primaryColor
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        onClicked: course.answerChoice(index)
                    }
                }
            }

            // --- Vorhersage ---------------------------------------------
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: !seite.aufgabe.leer && seite.aufgabe.art === "predict"

                TextArea {
                    id: vorhersage
                    width: parent.width
                    height: Math.max(Theme.itemSizeLarge, implicitHeight)
                    label: "Was schreibt das Programm?"
                    placeholderText: "Ausgabe"
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSizeSmall
                    enabled: !seite.aufgabe.geprueft
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                }

                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium

                    Button {
                        width: (parent.width - Theme.paddingMedium) / 2
                        text: "Prüfen"
                        enabled: !seite.aufgabe.geprueft
                        onClicked: course.answerText(vorhersage.text)
                    }
                    Button {
                        width: (parent.width - Theme.paddingMedium) / 2
                        text: course.running ? "läuft …" : "Laufen lassen"
                        enabled: seite.aufgabe.geprueft && !course.running
                                 && seite.aufgabe.zeichnet !== undefined
                        onClicked: course.runCode(seite.aufgabe.code, 30)
                    }
                }
            }

            // --- Zahl ---------------------------------------------------
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: !seite.aufgabe.leer && seite.aufgabe.art === "zahl"

                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium

                    TextField {
                        id: zahlenfeld
                        width: parent.width * 0.6
                        placeholderText: "Zahl"
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        enabled: !seite.aufgabe.geprueft
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: seite.aufgabe.einheit === undefined ? "" : seite.aufgabe.einheit
                        color: Theme.secondaryColor
                    }
                }

                Button {
                    text: "Prüfen"
                    enabled: !seite.aufgabe.geprueft && zahlenfeld.text !== ""
                    onClicked: course.answerNumber(parseFloat(zahlenfeld.text.replace(",", ".")))
                }

                Label {
                    width: parent.width
                    visible: seite.aufgabe.geprueft && !seite.aufgabe.richtig
                             && seite.aufgabe.loesungZeigen === true
                    text: "Richtig wäre etwa " + seite.aufgabe.antwort + " "
                          + (seite.aufgabe.einheit === undefined ? "" : seite.aufgabe.einheit)
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            // --- Luecke -------------------------------------------------
            Column {
                id: luecken
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: !seite.aufgabe.leer && seite.aufgabe.art === "blank"

                property var felder: []

                Repeater {
                    id: lueckenFelder
                    model: (!seite.aufgabe.leer && seite.aufgabe.art === "blank")
                           ? seite.aufgabe.luecken : 0

                    TextField {
                        width: luecken.width
                        placeholderText: "fehlendes Stück " + (index + 1)
                        font.family: "monospace"
                        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                        enabled: !seite.aufgabe.geprueft
                    }
                }

                Button {
                    text: "Prüfen"
                    enabled: !seite.aufgabe.geprueft
                    onClicked: {
                        var werte = []
                        for (var i = 0; i < lueckenFelder.count; i++)
                            werte.push(lueckenFelder.itemAt(i).text)
                        course.answerBlanks(werte)
                    }
                }
            }

            // --- Zeilen ordnen ------------------------------------------
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingSmall
                visible: !seite.aufgabe.leer && seite.aufgabe.art === "parsons"

                Label {
                    text: "Deine Lösung — antippen legt eine Zeile zurück"
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }

                Repeater {
                    model: seite.loesungsZeilen
                    BackgroundItem {
                        width: parent.width
                        height: Theme.itemSizeExtraSmall * 0.8
                        enabled: !seite.aufgabe.geprueft
                        Rectangle {
                            anchors.fill: parent
                            color: Theme.rgba(Theme.highlightBackgroundColor, 0.12)
                            radius: Theme.paddingSmall
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            x: Theme.paddingMedium
                            text: modelData
                            font.family: "monospace"
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.primaryColor
                        }
                        onClicked: {
                            var l = seite.loesungsZeilen.slice()
                            var v = seite.vorratsZeilen.slice()
                            v.push(l[index])
                            l.splice(index, 1)
                            seite.loesungsZeilen = l
                            seite.vorratsZeilen = v
                        }
                    }
                }

                Label {
                    text: "Bausteine"
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }

                Repeater {
                    model: seite.vorratsZeilen
                    BackgroundItem {
                        width: parent.width
                        height: Theme.itemSizeExtraSmall * 0.8
                        enabled: !seite.aufgabe.geprueft
                        Rectangle {
                            anchors.fill: parent
                            color: Theme.rgba(Theme.highlightBackgroundColor, 0.06)
                            border.color: Theme.rgba(Theme.highlightColor, 0.2)
                            border.width: 1
                            radius: Theme.paddingSmall
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            x: Theme.paddingMedium
                            text: modelData
                            font.family: "monospace"
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.primaryColor
                        }
                        onClicked: {
                            var l = seite.loesungsZeilen.slice()
                            var v = seite.vorratsZeilen.slice()
                            l.push(v[index])
                            v.splice(index, 1)
                            seite.loesungsZeilen = l
                            seite.vorratsZeilen = v
                        }
                    }
                }

                Button {
                    text: "Prüfen"
                    enabled: !seite.aufgabe.geprueft && seite.loesungsZeilen.length > 0
                    onClicked: course.answerParsons(seite.loesungsZeilen)
                }
            }

            // --- Selbst schreiben ---------------------------------------
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: !seite.aufgabe.leer && seite.aufgabe.art === "code"

                TextArea {
                    id: editor
                    width: parent.width
                    height: Math.max(Theme.itemSizeHuge * 2, implicitHeight)
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSizeExtraSmall
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    text: seite.editorText
                    onTextChanged: seite.editorText = text
                }

                Row {
                    width: parent.width
                    spacing: Theme.paddingSmall

                    Repeater {
                        model: {
                            var s = seite.aufgabe.leer ? "c" : course.lesson.sprache
                            if (s === "rust")
                                return ["{", "}", "(", ")", ";", "&", "!", "    "]
                            if (s === "python")
                                return [":", "(", ")", "[", "]", "=", "%", "    "]
                            return ["{", "}", "(", ")", ";", "*", "[", "]"]
                        }
                        BackgroundItem {
                            width: (parent.width - 7 * Theme.paddingSmall) / 8
                            height: Theme.itemSizeExtraSmall * 0.7
                            Rectangle {
                                anchors.fill: parent
                                radius: Theme.paddingSmall
                                color: Theme.rgba(Theme.highlightBackgroundColor, 0.12)
                            }
                            Label {
                                anchors.centerIn: parent
                                text: modelData === "    " ? "␣␣" : modelData
                                font.family: "monospace"
                                color: Theme.primaryColor
                            }
                            onClicked: {
                                var stelle = editor.cursorPosition
                                editor.text = editor.text.substring(0, stelle) + modelData
                                            + editor.text.substring(stelle)
                                editor.cursorPosition = stelle + modelData.length
                            }
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium

                    Button {
                        width: (parent.width - Theme.paddingMedium) * 0.62
                        text: course.running ? "läuft …" : "Ausführen"
                        enabled: !course.running && course.canRun
                        onClicked: {
                            course.keepCode(editor.text)
                            course.runCode(editor.text,
                                           seite.aufgabe.sekunden === undefined
                                           ? 10 : seite.aufgabe.sekunden)
                        }
                    }
                    Button {
                        width: (parent.width - Theme.paddingMedium) * 0.38
                        text: "Stopp"
                        enabled: course.running
                        onClicked: course.stopRun()
                    }
                }

                Button {
                    width: parent.width
                    text: "Als Antwort prüfen"
                    enabled: !course.running && course.output !== ""
                    onClicked: course.checkRun()
                }

                Label {
                    width: parent.width
                    visible: seite.aufgabe.erwartet !== undefined
                             && seite.aufgabe.erwartet !== ""
                    text: "Erwartet: " + (seite.aufgabe.erwartet === undefined
                                          ? "" : seite.aufgabe.erwartet)
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    wrapMode: Text.Wrap
                }
            }

            OutputBlock {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
            }

            // --- Rueckmeldung -------------------------------------------
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: !seite.aufgabe.leer && seite.aufgabe.geprueft

                Label {
                    text: seite.aufgabe.richtig ? "Richtig" : "Noch nicht"
                    color: seite.aufgabe.richtig ? "#7ee787" : Theme.errorColor
                    font.pixelSize: Theme.fontSizeLarge
                }

                Label {
                    width: parent.width
                    text: seite.aufgabe.leer ? "" : Stil.reich(seite.aufgabe.warum)
                    textFormat: Text.RichText
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.primaryColor
                }

                // Die richtige Loesung steht nicht von selbst da: Wer sie
                // sofort liest, denkt nicht mehr nach. Auf Anfrage aber
                // gehoert sie hin -- eine falsche Antwort ohne Aufloesung
                // ist nur eine Niederlage.
                // Der zweite Anlauf steht vor der Aufloesung: Wer die
                // richtige Antwort liest und nickt, behaelt deutlich weniger
                // als wer sie nach einem Fehlschlag noch einmal selbst sucht.
                Button {
                    text: "Nochmal versuchen"
                    visible: !seite.aufgabe.richtig && seite.aufgabe.versuche < 2
                    onClicked: {
                        seite.zuruecksetzen()
                        course.retryExercise()
                    }
                }

                Button {
                    text: "Lösung anzeigen"
                    visible: !seite.aufgabe.richtig && !seite.aufgabe.loesungZeigen
                    onClicked: course.showSolution()
                }

                Label {
                    width: parent.width
                    visible: seite.aufgabe.loesungZeigen === true
                             && !seite.aufgabe.richtig
                             && seite.aufgabe.art !== "code"
                             && seite.aufgabe.art !== "zahl"
                    text: seite.loesungstext()
                    font.family: seite.aufgabe.art === "mc" ? Theme.fontFamily : "monospace"
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.highlightColor
                    wrapMode: Text.Wrap
                }

                CodeBlock {
                    width: parent.width
                    visible: seite.aufgabe.loesungZeigen === true
                             && seite.aufgabe.art === "code"
                    code: seite.aufgabe.loesung === undefined ? "" : seite.aufgabe.loesung
                }
            }

            Item { width: 1; height: Theme.paddingMedium }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !seite.aufgabe.leer && seite.aufgabe.geprueft
                text: seite.aufgabe.nummer === seite.aufgabe.gesamt
                      ? "Lektion abschließen" : "Nächste Aufgabe"
                onClicked: seite.weiter()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !seite.aufgabe.leer && !seite.aufgabe.geprueft
                text: "Überspringen"
                onClicked: seite.weiter()
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        VerticalScrollDecorator { }
    }
}
