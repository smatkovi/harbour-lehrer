import QtQuick 2.0
import Sailfish.Silica 1.0
import "../worte.js" as W

// Die Lektionen eines Kapitels. Was erledigt ist, steht blass da -- und
// anfangen kann man trotzdem ueberall: Wer schon C kann, soll nicht durch
// drei Kapitel tippen, um zur Waermeleitung zu kommen.
Page {
    id: seite
    allowedOrientations: Orientation.All

    property string kapitelId: ""
    property var kapitel: leeresKapitel()

    function leeresKapitel() {
        var alle = course.chapters()
        for (var i = 0; i < alle.length; i++)
            if (alle[i].id === seite.kapitelId)
                return alle[i]
        return alle.length > 0 ? alle[0] : null
    }

    onStatusChanged: {
        if (status === PageStatus.Active)
            seite.kapitel = leeresKapitel()
    }

    SilicaListView {
        anchors.fill: parent
        model: seite.kapitel ? seite.kapitel.lektionen : []

        header: Column {
            width: seite.width

            PageHeader {
                title: seite.kapitel ? seite.kapitel.titel : ""
                description: seite.kapitel ? W.w("Stufe ", course.language) + seite.kapitel.stufe : ""
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: seite.kapitel ? seite.kapitel.text : ""
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        delegate: ListItem {
            width: parent.width
            contentHeight: zeile.height + Theme.paddingMedium

            Column {
                id: zeile
                x: Theme.horizontalPageMargin
                y: Theme.paddingSmall
                width: parent.width - 2 * Theme.horizontalPageMargin

                Label {
                    width: parent.width
                    text: modelData.titel
                    color: modelData.stand === "fertig"
                           ? Theme.secondaryColor : Theme.primaryColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    visible: modelData.stand !== ""
                    text: modelData.stand === "fertig" ? W.w("erledigt", course.language)
                          : (modelData.stand === "faellig" ? W.w("zur Wiederholung fällig", course.language)
                                                           : modelData.stand)
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: modelData.stand === "faellig" ? Theme.highlightColor
                                                         : Theme.secondaryColor
                }
            }

            onClicked: {
                course.startLesson(modelData.id)
                pageStack.push(Qt.resolvedUrl("LessonPage.qml"))
            }
        }

        VerticalScrollDecorator { }
    }
}
