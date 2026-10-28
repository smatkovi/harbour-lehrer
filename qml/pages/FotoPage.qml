import QtQuick 2.5
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import "../worte.js" as W

// Foto deuten: ein eigenes Himmelsfoto, erst der eigene Tipp, dann das
// Netz im Geraet -- und auf Wunsch Claude dazu.
//
// Die Reihenfolge ist der Zweck der Seite. Das Netz ist meist schon
// fertig, bevor der Leser die Ratetafel ueberhaupt gelesen hat; stuende
// sein Ergebnis da, tippte niemand mehr selbst. Also bleibt es verborgen,
// bis ein Tipp steht (course.photo.geraten), und erst dann erscheinen
// beide Urteile nebeneinander, das eigene ausdruecklich darunter.
//
// QtQuick 2.5 statt 2.0 wie sonst: erst dort hat Image autoTransform, und
// ohne das steht jedes hochkant geschossene Foto der Jolla auf der Seite.
//
// Alles, was entschieden wird, entscheidet Course: Die Seite liest
// course.photo und ruft classifyPhoto, guessPhoto, askOnline.
Page {
    id: seite
    allowedOrientations: Orientation.All

    property var foto: course.photo
    // Die Hinweise zum Fotografieren; fehlt die Eigenschaft im Programm,
    // bleibt die Liste leer statt die Seite zu verlieren.
    property var tipps: course.classifierTips === undefined
                        ? [] : course.classifierTips
    // Die Kamera liegt in einer eigenen Datei, weil ihr Import nur dort
    // haengt: Laedt KameraPage.qml nicht, fehlt die Taste, nicht die Seite.
    property var kameraBauplan: null
    property bool kameraDa: false
    // Woran man es sieht, aufgeklappt?
    property bool mehr: false
    // Die Tafel zum Richtigstellen, aufgeklappt?
    property bool richtig: false

    Connections {
        target: course
        onPhotoChanged: seite.foto = course.photo
    }

    Component.onCompleted: {
        var k = Qt.createComponent(Qt.resolvedUrl("KameraPage.qml"))
        if (k.status === Component.Ready) {
            seite.kameraBauplan = k
            seite.kameraDa = true
        }
    }

    // Das Foto gehoert zu dieser Seite; wer zurueckgeht, faengt beim
    // naechsten Mal leer an.
    Component.onDestruction: course.clearPhoto()

    function sicherheitsWort(s) {
        if (s === "hoch") return W.w("hoch", course.language)
        if (s === "mittel") return W.w("mittel", course.language)
        if (s === "niedrig") return W.w("niedrig", course.language)
        return s === undefined ? "" : s
    }

    Component {
        id: bildwahl
        ImagePickerPage {
            onSelectedContentPropertiesChanged: {
                seite.mehr = false
                course.classifyPhoto(selectedContentProperties.filePath)
            }
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

            PageHeader { title: course.classifierTitle }

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium

                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    visible: seite.foto.leer
                    text: course.classifierSubtitle
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }

                // --- Foto holen ---------------------------------------
                Row {
                    width: parent.width
                    spacing: Theme.paddingMedium

                    Button {
                        width: seite.kameraDa
                               ? (parent.width - Theme.paddingMedium) / 2
                               : parent.width
                        text: W.w("Foto wählen", course.language)
                        onClicked: pageStack.push(bildwahl)
                    }
                    Button {
                        width: (parent.width - Theme.paddingMedium) / 2
                        visible: seite.kameraDa
                        text: W.w("Foto aufnehmen", course.language)
                        onClicked: {
                            seite.mehr = false
                            pageStack.push(seite.kameraBauplan)
                        }
                    }
                }

                // --- so gelingt das Foto (nur solange keins da ist) ----
                Column {
                    width: parent.width
                    spacing: Theme.paddingSmall
                    visible: seite.foto.leer && seite.tipps.length > 0

                    SectionHeader {
                        text: W.w("So gelingt das Foto", course.language)
                    }
                    Repeater {
                        model: seite.tipps
                        Label {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: "• " + modelData
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.secondaryColor
                        }
                    }
                }

                // --- das Foto -----------------------------------------
                // Klein dekodiert (sourceSize 640): Die Kamera liefert
                // acht Millionen Bildpunkte. Die Hoehe kommt aus den
                // Massen des geladenen Bildes, nicht aus implicitHeight --
                // das waere die Bindungsschleife aus Bild.qml.
                Rectangle {
                    width: parent.width
                    height: (bild.status === Image.Ready && bild.sourceSize.width > 0)
                            ? Math.round(width * bild.sourceSize.height
                                         / bild.sourceSize.width)
                            : Math.round(width * 0.75)
                    visible: !seite.foto.leer
                    color: Theme.rgba(Theme.highlightBackgroundColor, 0.06)
                    border.color: Theme.rgba(Theme.highlightColor, 0.2)
                    border.width: 1
                    radius: Theme.paddingSmall
                    clip: true

                    Image {
                        id: bild
                        anchors.fill: parent
                        anchors.margins: 1
                        source: seite.foto.leer ? "" : "file://" + seite.foto.pfad
                        sourceSize.width: 640
                        autoTransform: true
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        asynchronous: true
                    }

                    BusyIndicator {
                        anchors.centerIn: parent
                        size: BusyIndicatorSize.Medium
                        running: bild.status === Image.Loading
                    }
                }

                // --- Fehler beim Laden oder Rechnen -------------------
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    visible: !seite.foto.leer && seite.foto.fehler !== ""
                    text: seite.foto.fehler === undefined ? "" : seite.foto.fehler
                    color: "#ff6b6b"
                }

                // --- der eigene Tipp ----------------------------------
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    visible: tafel.visible
                    text: W.w("Was siehst du?", course.language)
                    color: Theme.primaryColor
                }

                Grid {
                    id: tafel
                    width: parent.width
                    columns: 2
                    spacing: Theme.paddingSmall
                    visible: !seite.foto.leer && seite.foto.geraten === ""
                             && seite.foto.fehler === ""

                    Repeater {
                        model: tafel.visible ? course.classifierClasses : []
                        Rectangle {
                            width: (tafel.width - Theme.paddingSmall) / 2
                            height: wahl.height + 2 * Theme.paddingMedium
                            radius: Theme.paddingSmall
                            color: druck.pressed
                                   ? Theme.rgba(Theme.highlightBackgroundColor, 0.3)
                                   : Theme.rgba(Theme.highlightBackgroundColor, 0.10)
                            border.width: 1
                            border.color: Theme.rgba(Theme.highlightColor, 0.2)

                            Label {
                                id: wahl
                                x: Theme.paddingMedium
                                y: Theme.paddingMedium
                                width: parent.width - 2 * Theme.paddingMedium
                                wrapMode: Text.WordWrap
                                text: modelData.name
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.primaryColor
                            }
                            MouseArea {
                                id: druck
                                anchors.fill: parent
                                onClicked: course.guessPhoto(modelData.id)
                            }
                        }
                    }
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: tafel.visible
                    text: W.w("Weiß nicht", course.language)
                    onClicked: course.guessPhoto("")
                }

                // Das Netz rechnet noch -- dezent, der Tipp geht vor.
                Row {
                    spacing: Theme.paddingMedium
                    visible: !seite.foto.leer && seite.foto.laeuft === true
                    BusyIndicator {
                        size: BusyIndicatorSize.Small
                        running: parent.visible
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: W.w("Das Netz rechnet …", course.language)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }
                }

                // --- das Netz sagt ------------------------------------
                Column {
                    id: netzBlock
                    width: parent.width
                    spacing: Theme.paddingSmall
                    visible: !seite.foto.leer && seite.foto.geraten !== ""
                             && seite.foto.fertig === true
                             && seite.foto.fehler === ""

                    SectionHeader { text: W.w("Das Netz sagt", course.language) }

                    Repeater {
                        model: (seite.foto.ergebnis === undefined || !netzBlock.visible)
                               ? [] : seite.foto.ergebnis
                        Column {
                            width: parent.width
                            spacing: Theme.paddingSmall / 2

                            Row {
                                width: parent.width
                                Label {
                                    width: parent.width - prozent.width
                                    wrapMode: Text.WordWrap
                                    text: modelData.name
                                    font.bold: index === 0
                                    color: index === 0 ? Theme.highlightColor
                                                       : Theme.primaryColor
                                }
                                Label {
                                    id: prozent
                                    width: Theme.itemSizeSmall
                                    horizontalAlignment: Text.AlignRight
                                    text: modelData.prozent + " %"
                                    font.bold: index === 0
                                    color: index === 0 ? Theme.highlightColor
                                                       : Theme.secondaryColor
                                }
                            }
                            Rectangle {
                                width: parent.width
                                height: Theme.paddingSmall
                                radius: height / 2
                                color: Theme.rgba(Theme.highlightBackgroundColor, 0.15)
                                Rectangle {
                                    width: Math.round(parent.width * modelData.prozent / 100)
                                    height: parent.height
                                    radius: height / 2
                                    color: index === 0 ? Theme.highlightColor
                                                       : Theme.secondaryHighlightColor
                                }
                            }
                            // Die Bedeutung fuers Fliegen, nur beim ersten.
                            Label {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                visible: index === 0 && modelData.sinn !== undefined
                                         && modelData.sinn !== ""
                                text: "→ " + modelData.sinn
                                color: "#7ee787"
                            }
                        }
                    }

                    // Woran man es sieht und was es heisst: aufklappbar,
                    // weil beide Texte lang sind.
                    BackgroundItem {
                        width: parent.width
                        height: Theme.itemSizeExtraSmall
                        onClicked: seite.mehr = !seite.mehr
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: seite.mehr
                                  ? W.w("Weniger", course.language)
                                  : W.w("Woran erkennt man das?", course.language)
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.highlightColor
                        }
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.mehr
                        text: (seite.foto.ergebnis === undefined
                               || seite.foto.ergebnis.length === 0)
                              ? "" : seite.foto.ergebnis[0].erkennen
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.mehr
                        text: (seite.foto.ergebnis === undefined
                               || seite.foto.ergebnis.length === 0)
                              ? "" : seite.foto.ergebnis[0].bedeutet
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                    }

                    // Das Gelernte hat die Reihenfolge gedreht -- sonst
                    // waere nicht zu sehen, dass es etwas tut.
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.foto.gelernt !== undefined
                                 && seite.foto.gelernt.korrigiert === true
                        text: W.w("Das Gelernte hat dieses Urteil geändert.", course.language)
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.highlightColor
                    }

                    // Rueckmeldung zum eigenen Tipp. Bei "weiss nicht"
                    // gibt es nichts zu bewerten.
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.foto.geraten !== "?"
                        text: seite.foto.treffer === true
                              ? W.w("Richtig geraten", course.language)
                              : W.w("Du hattest auf ", course.language)
                                + seite.foto.geratenName
                                + W.w(" getippt", course.language) + "."
                        font.bold: true
                        color: seite.foto.treffer === true ? "#7ee787" : "#ffb14e"
                    }
                }

                // --- Claude dazu --------------------------------------
                // Nur wenn es einen Weg ins Netz gibt; ohne Schluessel
                // steht davor das Feld dafuer. Der Schluessel geht an
                // Course und nirgendwohin sonst.
                Column {
                    width: parent.width
                    spacing: Theme.paddingSmall
                    visible: course.onlineAvailable && !seite.foto.leer
                             && seite.foto.geraten !== ""
                             && seite.foto.fertig === true
                             && seite.foto.fehler === ""
                             && seite.foto.online.laeuft !== true
                             && (seite.foto.online.fertig !== true
                                 || seite.foto.online.ok !== true)

                    TextField {
                        id: schluessel
                        width: parent.width
                        visible: !course.hasApiKey
                        echoMode: TextInput.Password
                        label: W.w("Anthropic-Schlüssel", course.language)
                        placeholderText: label
                        inputMethodHints: Qt.ImhNoPredictiveText
                                          | Qt.ImhNoAutoUppercase
                        EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                        EnterKey.onClicked: {
                            if (text.length > 0) {
                                course.setApiKey(text)
                                text = ""
                            }
                        }
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: !course.hasApiKey
                        enabled: schluessel.text.length > 0
                        text: W.w("Speichern", course.language)
                        onClicked: {
                            course.setApiKey(schluessel.text)
                            schluessel.text = ""
                        }
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: course.hasApiKey
                        text: W.w("Claude fragen", course.language)
                        onClicked: course.askOnline()
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        visible: seite.foto.online.fertig !== true
                        text: W.w("Das Foto wird an Anthropic geschickt.", course.language)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }
                }

                Column {
                    width: parent.width
                    spacing: Theme.paddingSmall
                    visible: !seite.foto.leer && seite.foto.geraten !== ""
                             && (seite.foto.online.laeuft === true
                                 || seite.foto.online.fertig === true)

                    SectionHeader { text: W.w("Claude sagt", course.language) }

                    Row {
                        spacing: Theme.paddingMedium
                        visible: seite.foto.online.laeuft === true
                        BusyIndicator {
                            size: BusyIndicatorSize.Small
                            running: parent.visible
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: W.w("Claude schaut …", course.language)
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                        }
                    }

                    // Fertig und gut: Klasse, Sicherheit, Begruendung,
                    // Bedeutung.
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.foto.online.fertig === true
                                 && seite.foto.online.ok === true
                        text: (seite.foto.online.name === undefined
                               || seite.foto.online.name === "")
                              ? W.w("Keine der Klassen", course.language)
                              : seite.foto.online.name
                        font.bold: true
                        color: Theme.highlightColor
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.foto.online.fertig === true
                                 && seite.foto.online.ok === true
                        text: W.w("Sicherheit: ", course.language)
                              + seite.sicherheitsWort(seite.foto.online.sicherheit)
                              + (seite.foto.online.modell === undefined || seite.foto.online.modell === ""
                                 ? "" : " · " + seite.foto.online.modell)
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryColor
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.foto.online.fertig === true
                                 && seite.foto.online.ok === true
                        text: seite.foto.online.begruendung === undefined
                              ? "" : seite.foto.online.begruendung
                        /* Der Text kommt vom Server: als PlainText, nicht als
                           AutoText -- sonst deutet Qt ein <img src="http://...">
                           darin als Auszeichnung und holt das Bild wirklich ab. */
                        textFormat: Text.PlainText
                        color: Theme.primaryColor
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.foto.online.fertig === true
                                 && seite.foto.online.ok === true
                                 && seite.foto.online.sinn !== undefined
                                 && seite.foto.online.sinn !== ""
                        text: "→ " + seite.foto.online.sinn
                        color: "#7ee787"
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.foto.online.fertig === true
                                 && seite.foto.online.ok === true
                        text: seite.foto.online.einig === true
                              ? W.w("Netz und Claude sind sich einig.", course.language)
                              : W.w("Claude sieht es anders als das Netz.", course.language)
                        font.pixelSize: Theme.fontSizeSmall
                        color: seite.foto.online.einig === true ? "#7ee787" : "#ffb14e"
                    }

                    // Fertig und schief: der Grund.
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.foto.online.fertig === true
                                 && seite.foto.online.ok !== true
                        text: seite.foto.online.fehler === undefined
                              ? "" : seite.foto.online.fehler
                        /* Der Text kommt vom Server: als PlainText, nicht als
                           AutoText -- sonst deutet Qt ein <img src="http://...">
                           darin als Auszeichnung und holt das Bild wirklich ab. */
                        textFormat: Text.PlainText
                        color: "#ff6b6b"
                    }
                }

                // --- Dazulernen ---------------------------------------
                // Stimmen Claudes Urteil und der eigene Tipp ueberein, hat
                // Course das Foto schon gemerkt (GELERNTES.md). Gehen sie
                // auseinander, entscheidet hier der Leser.
                Column {
                    width: parent.width
                    spacing: Theme.paddingSmall
                    visible: seite.foto.gelernt !== undefined
                             && seite.foto.gelernt.merkbar === true
                             && seite.foto.geraten !== ""

                    SectionHeader { text: W.w("Dazulernen", course.language) }

                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: seite.foto.gelernt.gemerkt !== ""
                        text: W.w("Gemerkt als ", course.language)
                              + seite.foto.gelernt.gemerktName + "."
                        color: "#7ee787"
                    }

                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        visible: zweiTasten.visible
                        text: W.w("Wessen Urteil soll das Netz lernen?", course.language)
                        color: Theme.primaryColor
                    }
                    Row {
                        id: zweiTasten
                        width: parent.width
                        spacing: Theme.paddingMedium
                        visible: seite.foto.gelernt.gemerkt === ""
                                 && seite.foto.online.fertig === true
                                 && seite.foto.online.ok === true
                                 && seite.foto.online.einig !== true
                                 && seite.foto.online.klasse !== undefined
                                 && seite.foto.online.klasse !== ""
                        Button {
                            width: (parent.width - Theme.paddingMedium) / 2
                            text: W.w("Claude", course.language)
                            onClicked: course.rememberPhoto(seite.foto.online.klasse, 1)
                        }
                        Button {
                            width: (parent.width - Theme.paddingMedium) / 2
                            visible: seite.foto.geraten !== "?"
                            text: W.w("Mein Tipp", course.language)
                            onClicked: course.rememberPhoto(seite.foto.geraten, 2)
                        }
                    }

                    // Richtigstellen, auch ohne Claude.
                    BackgroundItem {
                        width: parent.width
                        height: Theme.itemSizeExtraSmall
                        onClicked: seite.richtig = !seite.richtig
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: seite.richtig
                                  ? W.w("Weniger", course.language)
                                  : W.w("Es ist etwas anderes", course.language)
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.highlightColor
                        }
                    }
                    Grid {
                        id: richtigTafel
                        width: parent.width
                        columns: 2
                        spacing: Theme.paddingSmall
                        visible: seite.richtig

                        Repeater {
                            model: seite.richtig ? course.classifierClasses : []
                            Rectangle {
                                width: (richtigTafel.width - Theme.paddingSmall) / 2
                                height: richtigWahl.height + 2 * Theme.paddingMedium
                                radius: Theme.paddingSmall
                                color: richtigDruck.pressed
                                       ? Theme.rgba(Theme.highlightBackgroundColor, 0.3)
                                       : Theme.rgba(Theme.highlightBackgroundColor, 0.08)
                                border.width: 1
                                border.color: Theme.rgba(Theme.highlightColor, 0.2)
                                Label {
                                    id: richtigWahl
                                    x: Theme.paddingMedium
                                    y: Theme.paddingMedium
                                    width: parent.width - 2 * Theme.paddingMedium
                                    wrapMode: Text.WordWrap
                                    text: modelData.name
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.primaryColor
                                }
                                MouseArea {
                                    id: richtigDruck
                                    anchors.fill: parent
                                    onClicked: {
                                        course.rememberPhoto(modelData.id, 2)
                                        seite.richtig = false
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: seite.foto.gelernt.wirksam === true
                              ? W.w("Gelernt: ", course.language)
                                + seite.foto.gelernt.anzahl
                                + W.w(" Fotos — das Netz zieht sie mit ein.", course.language)
                              : W.w("Gelernt: ", course.language)
                                + seite.foto.gelernt.anzahl
                                + W.w(" Fotos. Ab ", course.language)
                                + (seite.foto.gelernt.anzahl + seite.foto.gelernt.fehlende)
                                + W.w(" zieht das Netz sie mit ein.", course.language)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: seite.foto.gelernt.anzahl > 0
                        text: W.w("Gelerntes ansehen", course.language)
                        onClicked: pageStack.push(Qt.resolvedUrl("GelerntesPage.qml"))
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
