import QtQuick 2.0
import Sailfish.Silica 1.0
import QtMultimedia 5.0
import "../worte.js" as W

// Ein Himmelsfoto direkt aufnehmen.
//
// Eigene Datei, weil der Import von QtMultimedia nur hier haengt: Fehlt
// das Modul, laedt FotoPage.qml diese Seite nicht (Component.Error) und
// blendet die Taste aus, statt selbst zu scheitern.
//
// Aufgenommen wird ohne Zielpfad -- capture() legt das Bild wie die
// Kamera-App unter Pictures ab, und Sailjail hat dafuer Pictures und
// Camera (harbour-segelflug.desktop). Der Pfad kommt ueber imageSaved
// zurueck und geht an Course; das Foto selbst bleibt dem Leser.
Page {
    id: seite
    // Hochkant festgehalten: Der Sucher dreht sich nicht mit, und ein
    // Himmelsfoto wird ohnehin nach oben geschossen.
    allowedOrientations: Orientation.Portrait

    property bool fehler: false

    Camera {
        id: kamera
        captureMode: Camera.CaptureStillImage
        // Nur wenn die Seite vorn ist; sonst haelt die Kamera die
        // Hardware auch hinter der Fotoseite fest.
        cameraState: seite.status === PageStatus.Active
                     ? Camera.ActiveState : Camera.UnloadedState
        focus.focusMode: Camera.FocusContinuous

        imageCapture {
            onImageSaved: {
                course.classifyPhoto(path)
                pageStack.pop()
            }
            onCaptureFailed: seite.fehler = true
        }
        onErrorStringChanged: if (errorString !== "") seite.fehler = true
    }

    VideoOutput {
        id: sucher
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: leiste.top
        source: kamera
        fillMode: VideoOutput.PreserveAspectFit
    }

    Label {
        anchors.centerIn: sucher
        width: parent.width - 2 * Theme.horizontalPageMargin
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        visible: seite.fehler
        text: W.w("Die Kamera lässt sich nicht öffnen.", course.language)
        color: Theme.secondaryColor
    }

    Column {
        id: leiste
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.paddingLarge
        width: parent.width
        spacing: Theme.paddingMedium

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            enabled: !seite.fehler && kamera.imageCapture.ready
            text: W.w("Aufnehmen", course.language)
            onClicked: kamera.imageCapture.capture()
        }
    }
}
