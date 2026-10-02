import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"
import "stil.js" as Stil

// Die Lernapp. Welcher Kurs darin steckt, entscheidet data/kurs.json --
// das Programm kennt sein Fach nicht.
ApplicationWindow {
    id: app
    initialPage: Component { StartPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    // Die Formelbilder im Fliesstext: Pfad, Punktgroesse und Helligkeit
    // der Umgebung bekommt die Auszeichnungsbibliothek einmal, hier.
    Component.onCompleted: {
        Stil.bilderPfad = bilderPfad
        Stil.skala = Theme.pixelRatio
        Stil.helleUmgebung = Theme.colorScheme === Theme.DarkOnLight
    }
}
