import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

// Die Lernapp. Welcher Kurs darin steckt, entscheidet data/kurs.json --
// das Programm kennt sein Fach nicht.
ApplicationWindow {
    id: app
    initialPage: Component { StartPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}
