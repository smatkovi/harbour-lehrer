import QtQuick 2.0
import Sailfish.Silica 1.0
import "../stil.js" as Stil

// Ein echtes Foto unter der Zeichnung: das Bild, der Satz, der sagt, wohin
// zu schauen ist, und darunter die Herkunft.
//
// Nicht Bild.qml mit anderem Dateinamen, und das aus zwei Gruenden. Bild
// sucht erst nach einer englisch beschrifteten Fassung (<name>.en.png) --
// bei siebenundzwanzig Fotos waeren das siebenundzwanzig Fehlversuche, denn
// ein Foto traegt keine Beschriftung. Und Bild rechnet mit einem festen
// Seitenverhaeltnis, weil alle Zeichnungen dieselbe Form haben; Fotos haben
// jedes seine eigene. Die Masse stehen deshalb im Kurs (tools/fotos.py
// schreibt sie mit), und die Hoehe kommt aus einer Zahl -- **nicht** aus
// der implicitHeight des Bildes, die waere wieder die Bindungsschleife aus
// Bild.qml.
//
// Die Nennung ist keine Zierde: Die Fotos stehen unter CC BY und CC BY-SA,
// und beide verlangen genau diese Zeile.
Column {
    id: foto

    property var daten
    property real verhaeltnis: (daten === undefined || !daten.breite)
                               ? 0.7 : daten.hoehe / daten.breite

    spacing: Theme.paddingSmall

    Rectangle {
        width: foto.width
        height: Math.round(width * foto.verhaeltnis)
        color: Theme.rgba(Theme.highlightBackgroundColor, 0.06)
        border.color: Theme.rgba(Theme.highlightColor, 0.2)
        border.width: 1
        radius: Theme.paddingSmall
        clip: true

        Image {
            anchors.fill: parent
            anchors.margins: 1
            source: foto.daten === undefined ? ""
                    : Stil.bilderPfad + foto.daten.datei
            fillMode: Image.PreserveAspectFit
            smooth: true
            asynchronous: true
        }
    }

    Label {
        width: foto.width
        wrapMode: Text.WordWrap
        text: foto.daten === undefined ? "" : Stil.reich(foto.daten.hinweis)
        textFormat: Text.RichText
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryHighlightColor
    }

    Label {
        width: foto.width
        wrapMode: Text.WordWrap
        text: foto.daten === undefined ? ""
              : foto.daten.autor + " · " + foto.daten.lizenz
                + " · Wikimedia Commons"
        font.pixelSize: Theme.fontSizeTiny
        color: Theme.secondaryColor
    }
}
