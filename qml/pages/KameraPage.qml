import QtQuick 2.0
import Sailfish.Silica 1.0
import QtMultimedia 5.4
import "../worte.js" as W

// Ein Himmelsfoto direkt aufnehmen.
//
// Eigene Datei, weil der Import von QtMultimedia nur hier haengt: Fehlt
// das Modul, laedt FotoPage.qml diese Seite nicht (Component.Error) und
// blendet die Taste aus, statt selbst zu scheitern. QtMultimedia 5.4, nicht
// 5.0: erst damit gibt es Camera.orientation (Revision 1), und das braucht
// der Tipp (unten).
//
// Aufgenommen wird ohne Zielpfad -- capture() legt das Bild wie die
// Kamera-App unter Pictures ab, und Sailjail hat dafuer Pictures und
// Camera (harbour-segelflug.desktop). Der Pfad kommt ueber imageSaved
// zurueck und geht an Course; das Foto selbst bleibt dem Leser.
//
// Scharfstellen und Belichten auf Tippen -- was drei Pruefer am Quelltext
// des Sailfish-Forks von QtMultimedia (mer-5.6, camerabin) und von
// gst-droid belegt haben, nachdem zwei Fassungen hier nicht
// scharfstellten:
//  - Die alte Fassung rief kamera.exposure.isMeteringModeSupported() auf.
//    Die Methode gibt es im QML-Typ nicht (plugins.qmltypes: CameraExposure
//    hat nur set…-Methoden); der Aufruf warf einen TypeError, und der stand
//    VOR searchAndLock() -- der Fokusbefehl kam nie. Die Pruefung mit
//    tools/qml-laden.sh sieht so etwas nicht, Methoden werden erst zur
//    Laufzeit aufgeloest.
//  - Eine eigene Belichtungsstelle gibt es im Fork ohnehin nicht
//    (CameraBinExposure kennt MeteringMode/SpotMeteringPoint nicht). Die
//    Messflaeche kommt trotzdem mit: CameraBinFocus schickt den Fokuspunkt
//    als Region ohne Typ, und gst-droid setzt daraus focus-areas UND
//    metering-areas. Tippen belichtet also ueber den Fokuspunkt.
//  - Der Punkt muss in Sensorkoordinaten: gst-droid liefert den Sucher in
//    Sensorlage (quer) mit einem Drehungs-Tag, der Texturbackend dreht die
//    Anzeige, mapPointToSourceNormalized liefert Anzeigekoordinaten -- die
//    Jolla-Kamera dreht den Tipp darum um camera.orientation
//    (CaptureOverlay.qml, onClicked). Ohne die Drehung trifft jeder nicht
//    mittige Tipp eine andere Stelle.
//  - Fuer den Himmel ist "unendlich" die richtige Schaerfe: Wolken liegen
//    jenseits der hyperfokalen Entfernung, und ein Kontrast-Autofokus auf
//    weichen Wolkenraendern scheitert oft und laesst die Linse irgendwo
//    stehen. Kann die Kamera FocusInfinity, nehmen wir das; der Tipp setzt
//    dann nur noch die Messflaeche. Sonst der Weg der Jolla-Kamera
//    (CaptureView.setFocusPoint): unlock, Auto + eigener Punkt als
//    Bindungen, Punkt setzen, searchAndLock.
//  - Kein Rueckfall nach fuenf Sekunden wie dort: Der Wechsel zurueck auf
//    FocusPointAuto setzt in gst-droid auch die Messflaeche zurueck, und
//    diese Seite lebt nur bis zur einen Aufnahme.
Page {
    id: seite
    // Hochkant festgehalten: Der Sucher dreht sich nicht mit, und ein
    // Himmelsfoto wird ohnehin nach oben geschossen.
    allowedOrientations: Orientation.Portrait

    property bool fehler: false
    property bool aufnahmeFehler: false
    // Wohin zuletzt getippt wurde, in Sucherkoordinaten -- der Ring.
    property real zielX: -1
    property real zielY: -1
    // Der Tipp-Fokus ist an: eigener Punkt (CaptureView: tapFocusActive).
    property bool tippFokus: false
    // Das Ergebnis des letzten Tipps: "" (keins), "scharf", "nicht",
    // "gemessen" (Messflaeche gesetzt, Schaerfe liegt im Unendlichen).
    property string ergebnis: ""
    // Ein Suchlauf ist angestossen; nur dann zaehlt ein Sperrstatus als
    // Ergebnis (unlock() allein meldet auch "Unlocked").
    property bool suche: false
    // Die Aufnahme wartet auf das Ende des Suchlaufs.
    property bool aufnahmeWartet: false
    readonly property bool unendlich: kamera.cameraStatus === Camera.ActiveStatus
                                      && kamera.focus.focusMode === Camera.FocusInfinity
    // Eine Aufnahme laeuft; ein zweiter Druck darf keinen zweiten capture()
    // ausloesen (der meldete "nicht bereit" als Fehlschlag).
    property bool aufnahmeLaeuft: false

    /* isFocusModeSupported bekommt ein Camera.-Enum, erwartet aber das der
       CameraFocus-Klasse (gleiche Zahlen). Wirft der Aufruf doch, soll
       nicht die ganze Bindung sterben -- dann eben Auto, der Weg der
       Jolla-Kamera. */
    function kann(modus) {
        try {
            return kamera.focus.isFocusModeSupported(modus) === true
        } catch (e) {
            return false
        }
    }

    Camera {
        id: kamera
        captureMode: Camera.CaptureStillImage
        // Nur wenn die Seite vorn ist; sonst haelt die Kamera die
        // Hardware auch hinter der Fotoseite fest.
        cameraState: seite.status === PageStatus.Active
                     ? Camera.ActiveState : Camera.UnloadedState

        focus {
            /* Der Kamerastatus steht in der Bedingung, damit die Bindung
               neu ausgewertet wird, sobald die Kamera aktiv ist: Erst dann
               kennt der Fork die vom Geraet unterstuetzten Modi
               (supported-focus-modes von droidcamsrc). */
            focusMode: {
                if (kamera.cameraStatus !== Camera.ActiveStatus)
                    return Camera.FocusContinuous
                if (seite.kann(Camera.FocusInfinity))
                    return Camera.FocusInfinity
                if (seite.tippFokus)
                    return Camera.FocusAuto
                return seite.kann(Camera.FocusContinuous)
                        ? Camera.FocusContinuous : Camera.FocusAuto
            }
            focusPointMode: seite.tippFokus ? Camera.FocusPointCustom : Camera.FocusPointAuto
        }
        focus.onFocusModeChanged: kamera.unlock()

        onLockStatusChanged: {
            if (lockStatus === Camera.Searching)
                return
            if (!seite.suche)
                return
            seite.suche = false
            if (seite.tippFokus && seite.ergebnis !== "gemessen")
                seite.ergebnis = lockStatus === Camera.Locked ? "scharf" : "nicht"
            if (seite.aufnahmeWartet && kamera.cameraStatus === Camera.ActiveStatus) {
                seite.aufnahmeWartet = false
                aufnahmeFrist.stop()
                seite.aufnahmeLaeuft = true
                kamera.imageCapture.capture()
            }
        }

        imageCapture {
            onImageSaved: {
                seite.aufnahmeLaeuft = false
                course.classifyPhoto(path)
                if (seite.status === PageStatus.Active)
                    pageStack.pop()
            }
            onCaptureFailed: { seite.aufnahmeLaeuft = false; seite.aufnahmeFehler = true }
        }
        onErrorStringChanged: if (errorString !== "") seite.fehler = true
    }

    onStatusChanged: {
        if (status === PageStatus.Deactivating) {
            nachfrist.stop()
            aufnahmeFrist.stop()
            seite.aufnahmeWartet = false
            seite.suche = false
            seite.tippFokus = false
        }
    }

    /* Tippen heisst: Messflaeche auf die Wolke -- und, wenn die Kamera
       keinen Unendlich-Modus hat, dort scharfstellen.
       Eine Wolke vor hellem Himmel brennt aus, wenn die Kamera auf das
       ganze Bild belichtet -- dann ist sie eine weisse Flaeche, und das
       Netz sieht nichts mehr. Der Hinweis auf der Fotoseite sagt "auf die
       Wolke tippen"; hier geschieht es. */
    function scharfstellen(x, y) {
        /* Der Ring zuerst, und zwar immer: Wer tippt, muss sehen, dass der
           Tipp angekommen ist. */
        seite.zielX = x
        seite.zielY = y
        ringAus.restart()
        if (seite.aufnahmeWartet || seite.aufnahmeLaeuft)
            return      // die Aufnahme geht vor
        if (kamera.cameraStatus !== Camera.ActiveStatus)
            return      // noch nicht bereit; der Ring bleibt als Antwort

        /* Sucherkoordinaten ins angezeigte Bild (0..1), dann in die
           Sensorlage drehen -- siehe Kopf. Ist das Bildrechteck noch leer,
           gibt mapPointToSourceNormalized (0,0); dann im Verhaeltnis. */
        var n
        if (sucher.contentRect.width > 0 && sucher.contentRect.height > 0)
            n = sucher.mapPointToSourceNormalized(Qt.point(x, y))
        else
            n = Qt.point(x / Math.max(1, sucher.width), y / Math.max(1, sucher.height))
        n = Qt.point(Math.max(0, Math.min(1, n.x)), Math.max(0, Math.min(1, n.y)))
        var p
        switch (kamera.orientation) {
        case 90:  p = Qt.point(1 - n.y, n.x); break
        case 180: p = Qt.point(1 - n.x, 1 - n.y); break
        case 270: p = Qt.point(n.y, 1 - n.x); break
        default:  p = n
        }

        // Genau die Reihenfolge von CaptureView.setFocusPoint(). Das eigene
        // unlock() meldet "Unlocked" -- das ist kein Ergebnis, darum das
        // Tor vorher schliessen.
        seite.suche = false
        kamera.unlock()
        seite.tippFokus = true
        kamera.focus.customFocusPoint = p
        if (seite.unendlich) {
            // Kein Suchlauf noetig: Die Messflaeche ist gesetzt, die
            // Schaerfe liegt im Unendlichen.
            seite.ergebnis = "gemessen"
            return
        }
        seite.ergebnis = ""
        seite.suche = true
        nachfrist.restart()
        kamera.searchAndLock()
    }

    function aufnehmen() {
        /* Wie die Jolla-Kamera: Ohne Tipp-Fokus wird vor der Aufnahme
           einmal scharfgestellt und gewartet, bis die Suche vorbei ist --
           ausser die Schaerfe liegt ohnehin fest (Unendlich). */
        if (seite.aufnahmeLaeuft)
            return
        seite.aufnahmeFehler = false
        if (kamera.lockStatus === Camera.Searching) {
            seite.aufnahmeWartet = true
            seite.suche = true
            aufnahmeFrist.restart()
            return
        }
        if (!seite.tippFokus && kamera.lockStatus === Camera.Unlocked
                && kamera.focus.focusMode !== Camera.FocusInfinity
                && kamera.focus.focusMode !== Camera.FocusHyperfocal) {
            seite.aufnahmeWartet = true
            seite.suche = true
            aufnahmeFrist.restart()
            kamera.searchAndLock()
            return
        }
        seite.aufnahmeLaeuft = true
        kamera.imageCapture.capture()
    }

    Timer {
        id: ringAus
        interval: 5000
        onTriggered: { seite.zielX = -1; seite.zielY = -1 }
    }

    /* Meldet die Kamera nach vier Sekunden weder scharf noch aufgegeben,
       steht es da. Vier, nicht anderthalb: Auf manchen Geraeten dauert
       der Suchlauf laenger. */
    Timer {
        id: nachfrist
        interval: 4000
        onTriggered: {
            if (seite.suche && seite.tippFokus && seite.ergebnis === "") {
                seite.ergebnis = "nicht"
                ringAus.restart()
                /* Der Fork kennt keinen Fokus-Timeout: Ohne Rueckruf bliebe
                   lockStatus ewig "Searching". unlock() beendet den Lauf;
                   die Messflaeche bleibt (resetFocusPoint laeuft nur beim
                   Wechsel des Punktmodus und beim Kamerastopp). */
                seite.suche = false
                kamera.unlock()
            }
        }
    }

    // Bleibt die Suche vor der Aufnahme stumm, wird trotzdem aufgenommen.
    Timer {
        id: aufnahmeFrist
        interval: 3000
        onTriggered: {
            if (seite.aufnahmeWartet) {
                seite.aufnahmeWartet = false
                seite.suche = false
                kamera.unlock()     // sonst schoesse capture() in den laufenden Suchlauf
                if (kamera.cameraStatus === Camera.ActiveStatus) {
                    seite.aufnahmeLaeuft = true
                    kamera.imageCapture.capture()
                }
            }
        }
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
        // scharf hat oder die Messflaeche gesetzt ist, orange, wenn der
        // Suchlauf aufgegeben hat, sonst hell.
        Rectangle {
            visible: seite.zielX >= 0
            x: seite.zielX - width / 2
            y: seite.zielY - height / 2
            width: Theme.itemSizeSmall
            height: width
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: seite.ergebnis === "scharf" || seite.ergebnis === "gemessen"
                          ? "#7ee787"
                          : (seite.ergebnis === "nicht" ? "#ffb14e" : Theme.lightPrimaryColor)
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
            text: {
                if (seite.aufnahmeFehler)
                    return W.w("Die Aufnahme ist fehlgeschlagen — noch einmal versuchen.", course.language)
                if (kamera.cameraStatus !== Camera.ActiveStatus)
                    return W.w("Die Kamera startet …", course.language)
                if (seite.aufnahmeLaeuft)
                    return W.w("Aufnahme …", course.language)
                if (seite.aufnahmeWartet || kamera.lockStatus === Camera.Searching)
                    return W.w("Scharfstellen …", course.language)
                if (seite.ergebnis === "nicht")
                    return W.w("An dieser Stelle fand die Kamera keinen Kontrast — auf eine Kante der Wolke tippen; die Messstelle gilt trotzdem.", course.language)
                if (seite.ergebnis === "scharf")
                    return W.w("Scharf, Belichtung auf der Wolke — jetzt aufnehmen.", course.language)
                if (seite.ergebnis === "gemessen")
                    return W.w("Belichtung auf der Wolke, Schärfe im Unendlichen — jetzt aufnehmen.", course.language)
                return W.w("Auf die Wolke tippen, dann aufnehmen.", course.language)
            }
            font.pixelSize: Theme.fontSizeExtraSmall
            color: seite.ergebnis === "nicht" || seite.aufnahmeFehler ? "#ffb14e" : Theme.secondaryColor
        }
        /* Was die Kamera gerade tut, in Worten -- damit ein Fehlschlag
           benennbar ist. Dazu das Sensorbild (die Aufloesung, mit der die
           Fokusregion gerechnet wird) und die Sensorlage, weil davon die
           Drehung des Tipps abhaengt. */
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 2 * Theme.horizontalPageMargin
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: !seite.fehler && seite.zielX >= 0
            text: (kamera.focus.focusMode === Camera.FocusInfinity
                   ? W.w("Fokus: unendlich", course.language)
                   : (kamera.focus.focusMode === Camera.FocusAuto
                      ? W.w("Fokus: Auto", course.language)
                      : (kamera.focus.focusMode === Camera.FocusContinuous
                         ? W.w("Fokus: laufend", course.language)
                         : W.w("Fokus: anderer Modus", course.language))))
                  + (kamera.focus.focusPointMode === Camera.FocusPointCustom
                     ? W.w(", Punkt", course.language) : W.w(", Mitte", course.language))
                  + (kamera.lockStatus === Camera.Locked
                     ? W.w(", gesperrt", course.language)
                     : (kamera.lockStatus === Camera.Searching
                        ? W.w(", sucht", course.language) : W.w(", frei", course.language)))
                  + " · " + kamera.viewfinder.resolution.width + "×"
                  + kamera.viewfinder.resolution.height
                  + " · " + kamera.orientation + "°"
            font.pixelSize: Theme.fontSizeTiny
            color: Theme.secondaryColor
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            enabled: !seite.fehler && kamera.imageCapture.ready && !seite.aufnahmeWartet
                     && !seite.aufnahmeLaeuft && kamera.cameraStatus === Camera.ActiveStatus
            text: W.w("Aufnehmen", course.language)
            onClicked: seite.aufnehmen()
        }
    }
}
