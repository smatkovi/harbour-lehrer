import QtQuick 2.0
import Sailfish.Silica 1.0
import "../worte.js" as W

// Die echten Fotos zu einer Zeichnung, mit Ueberschrift.
//
// Sie stehen **unter** der Zeichnung, nicht an ihrer Stelle. Die Zeichnung
// zeigt das Merkmal allein, das Foto zeigt es dort, wo es im Weg von allem
// anderen steht -- erst die Reihenfolge macht aus zwei Bildern eine
// Uebung.
//
// Die Liste ist leer, wenn der Kurs keine Fotos fuehrt, und dann ist dieses
// Element unsichtbar und null hoch: Dieselbe Seite traegt C, Rust,
// Meteorologie und Segelflug.
Column {
    id: block

    property var fotos: []
    property string sprache: "de"
    property bool hat: fotos !== undefined && fotos !== null && fotos.length > 0

    visible: hat
    spacing: Theme.paddingMedium

    SectionHeader {
        text: W.w("So sieht es wirklich aus", block.sprache)
    }

    Repeater {
        model: block.hat ? block.fotos : []
        Foto {
            width: block.width
            daten: modelData
        }
    }
}
