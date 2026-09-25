import QtQuick 2.0
import Sailfish.Silica 1.0
import "../stil.js" as Stil
import "../worte.js" as W

// Wovon eine Aufgabe ausgeht und worauf sie hinauswill.
//
// Steht ueber der Frage und nicht in der Rueckmeldung: Eine Aufgabe ohne
// genannte Annahmen ist ein Raetsel -- wer nicht weiss, dass die Masse 1 kg
// ist oder dass `int` hier 32 Bit hat, raet nicht schlechter, nur an einer
// anderen Stelle. Verraten wird damit nichts: Es sind die Zahlen und
// Gleichungen, mit denen gerechnet wird, nicht das Ergebnis.
//
// Die Reihenfolge ist Absicht. Zuerst **Mathematisch**: die Gleichung, das
// Gebiet, Anfangs- und Randbedingungen, die Diskretisierung, die Bedingung,
// unter der das Verfahren haelt. Dann **Physikalisch**: woher die Gleichung
// kommt und was weggelassen wurde. Dann der Rest, dann das Ziel. Wer die
// Physik zuerst liest, sucht in der Mathematik danach; umgekehrt nicht.
//
// Jedes Feld ist einzeln entbehrlich -- eine Aufgabe ueber `printf` hat
// keine Physik, und ein "Ziel: Sag voraus, was das Programm schreibt" unter
// der Frage "Was schreibt dieses Programm?" waere Laerm.
Rectangle {
    id: rahmen

    property string mathematisch: ""
    property string physikalisch: ""
    property string annahmen: ""
    property string ziel: ""

    width: parent.width
    visible: mathematisch !== "" || physikalisch !== ""
             || annahmen !== "" || ziel !== ""
    height: visible ? spalte.height + 2 * Theme.paddingMedium : 0
    radius: Theme.paddingSmall
    color: Theme.rgba(Theme.highlightBackgroundColor, 0.08)

    Column {
        id: spalte
        x: Theme.paddingMedium
        y: Theme.paddingMedium
        width: parent.width - 2 * Theme.paddingMedium
        spacing: Theme.paddingMedium

        Repeater {
            // Ueberschrift und Text als Paar, damit die Reihenfolge an
            // einer Stelle steht und nicht viermal abgeschrieben ist.
            model: [
                { "titel": "Mathematisch", "text": rahmen.mathematisch },
                { "titel": "Physikalisch", "text": rahmen.physikalisch },
                { "titel": "Annahmen",     "text": rahmen.annahmen },
                { "titel": "Ziel",         "text": rahmen.ziel }
            ]
            // `topPadding` gibt es erst ab QtQuick 2.6; den Abstand
            // zwischen den Bloecken macht deshalb die aeussere Column.
            Column {
                width: spalte.width
                visible: modelData.text !== ""
                spacing: Theme.paddingSmall / 2

                Label {
                    width: parent.width
                    text: W.w(modelData.titel, course.language)
                    font.pixelSize: Theme.fontSizeExtraSmall
                    font.bold: true
                    color: Theme.secondaryHighlightColor
                }
                Label {
                    width: parent.width
                    text: Stil.reich(modelData.text)
                    textFormat: Text.RichText
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                }
            }
        }
    }
}
