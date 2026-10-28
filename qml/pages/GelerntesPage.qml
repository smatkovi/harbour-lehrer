import QtQuick 2.0
import Sailfish.Silica 1.0
import "../worte.js" as W

// Was die App dazugelernt hat -- und wie man es loswird oder mitnimmt.
//
// Jeder Eintrag ist ein Foto, das der Leser deuten liess, mit der Klasse,
// auf die sich Claude und sein eigener Tipp geeinigt haben. Wer nachsieht,
// was das Netz gelernt hat, findet auch die Fotos, die es falsch gelernt
// hat, und kann sie einzeln wegnehmen. Format und Wirkung stehen in
// segelflug/tools/GELERNTES.md.
Page {
    id: seite
    allowedOrientations: Orientation.All

    property var gelernt: course.learned
    property string meldung: ""

    Connections {
        target: course
        onLearnedChanged: seite.gelernt = course.learned
    }

    function quellenWort(q) {
        if (q === 0) return W.w("Claude und ich", course.language)
        if (q === 1) return W.w("Claude", course.language)
        return W.w("von mir richtiggestellt", course.language)
    }

    SilicaListView {
        id: liste
        anchors.fill: parent
        model: seite.gelernt.eintraege === undefined ? [] : seite.gelernt.eintraege

        header: Column {
            width: liste.width
            spacing: Theme.paddingSmall

            PageHeader { title: W.w("Gelerntes", course.language) }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                text: seite.gelernt.wirksam === true
                      ? seite.gelernt.anzahl
                        + W.w(" Fotos, die das Netz mit einbezieht.", course.language)
                      : seite.gelernt.anzahl
                        + W.w(" Fotos. Ab ", course.language)
                        + (seite.gelernt.anzahl + seite.gelernt.fehlende)
                        + W.w(" zieht das Netz sie mit ein.", course.language)
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
            }
            // Ab vierzig Eintraegen wird die letzte Schicht des Netzes
            // selbst nachtrainiert -- dann steht es hier.
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                visible: seite.gelernt.anzahl > 0
                text: seite.gelernt.kopf === true
                      ? W.w("Die letzte Schicht des Netzes ist nachtrainiert.", course.language)
                      : W.w("Ab ", course.language) + seite.gelernt.kopfAb
                        + W.w(" Fotos wird auch die letzte Schicht des Netzes nachtrainiert.", course.language)
                font.pixelSize: Theme.fontSizeSmall
                color: seite.gelernt.kopf === true ? "#7ee787" : Theme.secondaryColor
            }
            TextSwitch {
                text: W.w("Von Claude lernen, unabhängig von meinem Tipp", course.language)
                description: W.w("Die App merkt Fables Klasse von selbst, bei Sicherheit „hoch“ und „mittel“ — auch wenn du etwas anderes getippt hast. Antwortet Opus, bleibt es beim Knopf.", course.language)
                checked: course.learnFromClaude
                automaticCheck: false
                onClicked: course.learnFromClaude = !checked
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                visible: seite.meldung !== ""
                text: seite.meldung
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
            }
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                Button {
                    width: (parent.width - Theme.paddingMedium) / 2
                    text: W.w("Sichern", course.language)
                    onClicked: seite.meldung = course.exportLearned()
                }
                Button {
                    width: (parent.width - Theme.paddingMedium) / 2
                    text: W.w("Laden", course.language)
                    onClicked: seite.meldung = course.importLearned("")
                }
            }
            /* Luft zur Liste: QtQuick 2.0 kennt an Column kein Padding
               (erst 2.6), und die Seite importiert 2.0 wie alle anderen. */
            Item { width: 1; height: Theme.paddingMedium }
        }

        delegate: ListItem {
            id: zeile
            contentHeight: Math.max(vorschau.height, texte.height) + Theme.paddingMedium
            width: liste.width

            // Die Vorschau ist das gespeicherte Netzbild (160 Pixel);
            // Course legt es dafuer in den Zwischenspeicher.
            Image {
                id: vorschau
                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.itemSizeMedium
                height: Theme.itemSizeMedium
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                clip: true
                sourceSize.width: 2 * Theme.itemSizeMedium
                source: "file://" + course.learnedImage(modelData.nummer)
            }

            Column {
                id: texte
                anchors.left: vorschau.right
                anchors.leftMargin: Theme.paddingMedium
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.paddingSmall / 2

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: modelData.name
                    color: modelData.bekannt === true
                           ? Theme.primaryColor : Theme.secondaryColor
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: seite.quellenWort(modelData.quelle)
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }
                // Eine Klasse, die dieses Netz nicht kennt, zaehlt nicht
                // mit -- das muss dastehen, sonst sucht man den Fehler
                // woanders.
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    visible: modelData.bekannt !== true
                    text: W.w("Diese Klasse kennt das Netz nicht.", course.language)
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: "#ffb14e"
                }
            }

            menu: ContextMenu {
                MenuItem {
                    text: W.w("Vergessen", course.language)
                    onClicked: {
                        course.forgetLearned(modelData.nummer)
                        seite.meldung = W.w("Eintrag gelöscht.", course.language)
                    }
                }
            }
        }

        footer: Column {
            width: liste.width
            spacing: Theme.paddingMedium

            Item { width: 1; height: Theme.paddingLarge }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                visible: liste.count === 0
                text: W.w("Noch nichts gelernt. Lass ein Foto von Claude deuten; "
                          + "stimmt sein Urteil mit deinem Tipp überein, merkt es "
                          + "die App.", course.language)
                color: Theme.secondaryColor
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: liste.count > 0
                text: W.w("Alles vergessen", course.language)
                onClicked: {
                    course.forgetAllLearned()
                    seite.meldung = W.w("Alles vergessen.", course.language)
                }
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        VerticalScrollDecorator {}
    }
}
