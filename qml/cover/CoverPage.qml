import QtQuick 2.0
import Sailfish.Silica 1.0
import "../worte.js" as W

// Die Abdeckung zeigt, was als Naechstes ansteht -- mehr braucht sie nicht.
// Die Zahl der faelligen Wiederholungen ist der eine Wert, der jemanden
// wirklich zurueckholt.
CoverBackground {
    id: deckel

    property var werte: course.stats

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: course.courseTitle
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.highlightColor
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: deckel.werte.fertig + " " + W.w("von", course.language) + " "
                  + deckel.werte.gesamt + " " + W.w("Lektionen", course.language)
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            visible: deckel.werte.faellig > 0
            text: deckel.werte.faellig + " " + W.w("fällig", course.language)
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.primaryColor
        }
    }

    CoverActionList {
        CoverAction {
            iconSource: "image://theme/icon-cover-next"
            onTriggered: {
                app.activate()
                var l = course.nextLesson()
                if (!l.leer) {
                    course.startLesson(l.id)
                    pageStack.push(Qt.resolvedUrl("../pages/LessonPage.qml"))
                }
            }
        }
    }
}
