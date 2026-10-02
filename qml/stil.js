// Der kleine Auszeichnungssatz, den die Kurstexte benutzen: **fett**,
// *kursiv*, `Programmtext`. Mehr braucht ein Lehrtext nicht, und mehr
// wollte auch niemand in JSON schreiben.
//
// Silica bringt die Farben mit; hier wird nur ausgezeichnet.
.pragma library

// Wo die gesetzten Formeln liegen, wie gross ein Punkt auf diesem Geraet
// ist und ob die Umgebung hell ist. main.qml traegt alle drei beim Start
// ein -- eine Bibliothek kommt an Theme und Kontext nicht heran, und
// reich() wird von zu vielen Stellen aufgerufen, als dass jede den Pfad
// mitgeben sollte.
var bilderPfad = ""
var skala = 1.0
var helleUmgebung = false
// Breiter als die Seite darf kein Bild werden; main.qml traegt die Breite
// ein, die zwischen den Seitenraendern bleibt.
var breiteMax = 0

// Die Formeln sind fuer 17 Punkt bei 100 dpi gesetzt (tools/formeln.py),
// ein Geviert misst also 23,6 Bildpunkte bei Skala 1. Sie stehen etwas
// groesser als die Schrift daneben, weil Brueche, Indizes und Exponenten
// im Bild kleiner ausfallen als die Grundlinie -- bei gleicher Hoehe
// wirkte ein Bruch im Satz winzig.
var GEVIERT = 17 * 100 / 72
function skalaFuer(schriftgroesse) {
    return 1.15 * schriftgroesse / GEVIERT
}

// Breite und Hoehe eines gesetzten Bildes auf diesem Geraet: skaliert,
// und wenn es dann breiter waere als die Seite, auf die Seite verkleinert.
function bildmasse(b, h) {
    var f = skala
    if (breiteMax > 0 && b * f > breiteMax)
        f = breiteMax / b
    return { breite: Math.round(b * f), hoehe: Math.round(h * f) }
}

function schuetzen(text) {
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
}

function reich(quelle) {
    if (!quelle)
        return ""
    var aus = schuetzen(quelle)
    // Ueberschriften. Die Herleitungen sind lang genug, dass sie
    // Zwischenstufen brauchen -- ohne diese Zeile stand "## Herleitung"
    // woertlich im Text.
    aus = aus.replace(/^##\s*(.+)$/gm,
                      "<b>" + "$1" + "</b>")
    aus = aus.replace(/\*\*([^*]+)\*\*/g, "<b>$1</b>")
    aus = aus.replace(/\*([^*\n]+)\*/g, "<i>$1</i>")
    // Programmtext in fester Breite. Die Farbe kommt aus dem Thema und wird
    // beim Aufruf eingesetzt -- so bleibt die Datei eine reine Bibliothek.
    aus = aus.replace(/`([^`]+)`/g,
                      "<span style='font-family:monospace'>$1</span>")
    // Gesetzte Formeln im Fliesstext: make-kurs.py hat $...$ durch
    // [[formel:<name>:<breite>:<hoehe>]] ersetzt. Das Bild traegt helle
    // Schrift; auf heller Umgebung nimmt es die dunkle Fassung daneben
    // (<name>.dunkel.png), weil Rich Text nicht einfaerbt.
    aus = aus.replace(/\[\[formel:([A-Za-z0-9_-]+):(\d+):(\d+)\]\]/g,
                      function (ganz, name, b, h) {
                          var m = bildmasse(b, h)
                          return "<img src='" + bilderPfad + name
                                 + (helleUmgebung ? ".dunkel" : "") + ".png' width='"
                                 + m.breite + "' height='" + m.hoehe + "'>"
                      })
    aus = aus.replace(/\n\n/g, "<br><br>")
    aus = aus.replace(/\n/g, "<br>")
    return aus
}
