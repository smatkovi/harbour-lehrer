// Der kleine Auszeichnungssatz, den die Kurstexte benutzen: **fett**,
// *kursiv*, `Programmtext`. Mehr braucht ein Lehrtext nicht, und mehr
// wollte auch niemand in JSON schreiben.
//
// Silica bringt die Farben mit; hier wird nur ausgezeichnet.
.pragma library

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
    aus = aus.replace(/\n\n/g, "<br><br>")
    aus = aus.replace(/\n/g, "<br>")
    return aus
}
