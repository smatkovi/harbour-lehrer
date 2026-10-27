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
    // Wohin zuletzt getippt wurde, in Sucherkoordinaten -- der Ring.
    property real zielX: -1
    property real zielY: -1
    property bool gesucht: false
    property bool gescheitert: false
    // Was die Kamera auf Tippen hin ueberhaupt annimmt: P Punkt,
    // A/C Scharfstellen, B Belichtung. Steht im Hinweis, wenn nichts geht.
    property string konnte: ""

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

    /* Tippen heisst scharfstellen UND belichten.
       Beides gehoert zusammen: Eine Wolke vor hellem Himmel brennt aus,
       wenn die Kamera auf das ganze Bild belichtet -- dann ist sie eine
       weisse Flaeche, und das Netz sieht nichts mehr. Der Hinweis auf der
       Fotoseite sagt "auf die Wolke tippen"; hier geschieht es.

       Die Koordinaten muessen vom Sucher ins Bild umgerechnet werden
       (mapPointToSourceNormalized), sonst zielt die Kamera daneben, sobald
       der Sucher nicht genau das Seitenverhaeltnis des Bildes hat. */
    function scharfstellen(x, y) {
        /* Der Ring zuerst, und zwar immer: Wer tippt, muss sehen, dass der
           Tipp angekommen ist -- auch wenn die Kamera danach nichts kann.
           (Die erste Fassung stieg hier still aus, wenn die Umrechnung
           misslang; dann geschah sichtbar gar nichts.) */
        seite.zielX = x
        seite.zielY = y
        seite.gesucht = true
        seite.gescheitert = false
        ringAus.restart()
        aufgeben.restart()

        /* Sucherkoordinaten ins Bild umrechnen.  mapPointToSourceNormalized
           gibt (-1,-1), solange contentRect noch leer ist; dann rechnen wir
           schlicht im Verhaeltnis -- besser als gar nicht zielen. */
        var p = Qt.point(-1, -1)
        try {
            p = sucher.mapPointToSourceNormalized(Qt.point(x, y))
        } catch (e) {
            p = Qt.point(-1, -1)
        }
        if (p === undefined || p.x < 0 || p.x > 1 || p.y < 0 || p.y > 1)
            p = Qt.point(Math.max(0, Math.min(1, x / Math.max(1, sucher.width))),
                         Math.max(0, Math.min(1, y / Math.max(1, sucher.height))))

        seite.konnte = ""
        if (kamera.focus.isFocusPointModeSupported(Camera.FocusPointCustom)) {
            kamera.focus.focusPointMode = Camera.FocusPointCustom
            kamera.focus.customFocusPoint = p
            seite.konnte += "P"
        }
        if (kamera.focus.isFocusModeSupported(Camera.FocusAuto)) {
            kamera.focus.focusMode = Camera.FocusAuto
            seite.konnte += "A"
        } else if (kamera.focus.isFocusModeSupported(Camera.FocusContinuous)) {
            kamera.focus.focusMode = Camera.FocusContinuous
            seite.konnte += "C"
        }
        if (kamera.exposure.isMeteringModeSupported(Camera.MeteringSpot)) {
            kamera.exposure.spotMeteringPoint = p
            kamera.exposure.meteringMode = Camera.MeteringSpot
            seite.konnte += "B"
        }
        kamera.unlock()
        kamera.searchAndLock()
    }

    Timer {
        id: ringAus
        interval: 3000
        onTriggered: { seite.zielX = -1; seite.zielY = -1; seite.gesucht = false }
    }

    /* Meldet sich die Kamera nach anderthalb Sekunden nicht als scharf,
       kann sie es offenbar nicht -- dann steht es da, statt dass man weiter
       auf einen gruenen Ring wartet, den es nie gibt. */
    Timer {
        id: aufgeben
        interval: 1500
        onTriggered: if (kamera.lockStatus !== Camera.Locked) seite.gescheitert = true
    }

    VideoOutput {
        id: sucher
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: leiste.top
        source: kamera
        fillMode: VideoOutput.PreserveAspectFit

        MouseArea {
            anchors.fill: parent
            onClicked: seite.scharfstellen(mouse.x, mouse.y)
        }

        // Der Ring sagt, wohin die Kamera jetzt schaut: gruen, wenn sie
        // scharf hat, sonst hell.
        Rectangle {
            visible: seite.zielX >= 0
            x: seite.zielX - width / 2
            y: seite.zielY - height / 2
            width: Theme.itemSizeSmall
            height: width
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: kamera.lockStatus === Camera.Locked
                          ? "#7ee787"
                          : (seite.gescheitert ? "#ffb14e" : Theme.lightPrimaryColor)
            Behavior on border.color { ColorAnimation { duration: 150 } }
        }
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

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 2 * Theme.horizontalPageMargin
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            visible: !seite.fehler
            text: seite.gescheitert
                  ? W.w("Diese Kamera stellt auf Tippen nicht scharf", course.language)
                    + (seite.konnte === ""
                       ? W.w(" und nimmt auch keine Belichtungsstelle an.", course.language)
                       : W.w(" — die Belichtung ist gesetzt (", course.language)
                         + seite.konnte + ").")
                  : W.w("Auf die Wolke tippen, dann aufnehmen.", course.language)
            font.pixelSize: Theme.fontSizeExtraSmall
            color: seite.gescheitert ? "#ffb14e" : Theme.secondaryColor
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            enabled: !seite.fehler && kamera.imageCapture.ready
            text: W.w("Aufnehmen", course.language)
            onClicked: kamera.imageCapture.capture()
        }
    }
}
