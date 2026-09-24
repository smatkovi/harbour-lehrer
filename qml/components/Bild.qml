import QtQuick 2.0
import Sailfish.Silica 1.0

// Ein Bild zu einer Lektion oder Aufgabe.
//
// Im Kurs steht der blosse Name ("cu-humilis"); die Datei liegt als
// <bilderPfad>/<name>.png. Dass der Pfad nicht im Kurs steht, ist Absicht --
// dieselbe Kursdatei laeuft dann auf Harmattan und auf Sailfish.
//
// Die Hoehe kommt aus einem festen Verhaeltnis und **nicht** aus der
// implicitHeight des Bildes. Sie daraus abzuleiten, waehrend das Bild diesen
// Rahmen fuellt, ist eine Bindungsschleife: QML erkennt sie, wirft die
// Hoehenbindung weg, und der Rahmen bleibt null Pixel hoch -- ein Bild, das
// da ist, stimmt und vollstaendig unsichtbar bleibt.
Rectangle {
    id: rahmen

    property string name: ""
    property real verhaeltnis: 300 / 440      // die Form der Zeichnungen

    visible: name !== ""
    height: visible ? Math.round(width * verhaeltnis) : 0
    color: Theme.rgba(Theme.highlightBackgroundColor, 0.06)
    border.color: Theme.rgba(Theme.highlightColor, 0.2)
    border.width: 1
    radius: Theme.paddingSmall
    clip: true

    // Zeichnungen tragen ihre Beschriftung im Bild -- in der englischen
    // Fassung muss also ein anderes Bild her.  Es heisst <name>.en.png und
    // liegt neben dem deutschen; gibt es keines, bleibt das deutsche stehen.
    Image {
        anchors.fill: parent
        anchors.margins: 1
        property bool zurueckgefallen: false
        source: rahmen.name === "" ? ""
                : bilderPfad + rahmen.name
                  + (course.language !== "de" && !zurueckgefallen
                     ? "." + course.language : "") + ".png"
        onStatusChanged: if (status === Image.Error && !zurueckgefallen)
                             zurueckgefallen = true
        onSourceChanged: if (course.language === "de") zurueckgefallen = false
        fillMode: Image.PreserveAspectFit
        smooth: true
        asynchronous: true
    }
}
