import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "../worte.js" as W

// Eine leere Seite mit einem Deuter dahinter. Die Haelfte des Lernens ist,
// etwas Kleines auszuprobieren, nur um zu sehen, was passiert -- dafuer
// braucht es keine Lektion.
Page {
    id: seite
    allowedOrientations: Orientation.All

    // Die Spielwiese hat ihre eigene Sprache. Sie an der zuletzt geoeffneten
    // Lektion haengen zu lassen, war auf dem N9 ein Fehler: Wer aus dem
    // NumPy-Kapitel kam, bekam auf sein C-Programm einen Syntaxfehler von
    // Python.
    property string sprache: "c"

    property var vorlagen: {
        "c": "#include <stdio.h>\n#include <math.h>\n\n"
             + "int main()\n{\n    int i;\n\n"
             + "    for (i = 0; i < 100; i++) {\n"
             + "        double t = i * 0.1;\n"
             + "        printf(\"plot %.3f %.5f\\n\", t, sin(t) / (1 + t));\n"
             + "    }\n    return 0;\n}\n",
        "rust": "fn main() {\n"
                + "    for i in 0..100 {\n"
                + "        let t = i as f64 * 0.1;\n"
                + "        println!(\"plot {:.3} {:.5}\", t, t.sin() / (1.0 + t));\n"
                + "    }\n}\n",
        "python": "import math\n\n"
                  + "for i in range(100):\n"
                  + "    t = i * 0.1\n"
                  + "    print('plot %.3f %.5f' % (t, math.sin(t) / (1 + t)))\n"
    }

    property var zeichen: {
        "c": ["{", "}", "(", ")", ";", "*", "[", "]"],
        "rust": ["{", "}", "(", ")", ";", "&", "!", "    "],
        "python": [":", "(", ")", "[", "]", "=", "%", "    "]
    }

    function sprachwechsel(neu) {
        if (seite.sprache === neu)
            return
        if (editor.text === seite.vorlagen[seite.sprache])
            editor.text = seite.vorlagen[neu]
        seite.sprache = neu
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: spalte.height + Theme.paddingLarge

        Column {
            id: spalte
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: W.w("Spielwiese", course.language) }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: seite.sprache === "python"
                      ? W.w("Zeilen, die mit plot beginnen, werden gezeichnet: print('plot %f %f' % (t, x)).", course.language)
                      : (seite.sprache === "rust"
                         ? "Zeilen, die mit plot beginnen, werden gezeichnet: println!(\"plot {} {}\", t, x);"
                         : "Zeilen, die mit plot beginnen, werden gezeichnet: printf(\"plot %f %f\\n\", t, x);")
            }

            // Angeboten wird nur, was auf diesem Geraet auch laufen kann.
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingSmall

                Repeater {
                    model: [["c", "C"], ["rust", "Rust"], ["python", "Python"]]
                    BackgroundItem {
                        visible: course.canRunLanguage(modelData[0])
                        width: (parent.width - 2 * Theme.paddingSmall) / 3
                        height: Theme.itemSizeSmall * 0.8
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.paddingSmall
                            color: seite.sprache === modelData[0]
                                   ? Theme.rgba(Theme.highlightColor, 0.3)
                                   : Theme.rgba(Theme.highlightBackgroundColor, 0.1)
                        }
                        Label {
                            anchors.centerIn: parent
                            text: modelData[1]
                            color: Theme.primaryColor
                        }
                        onClicked: seite.sprachwechsel(modelData[0])
                    }
                }
            }

            TextArea {
                id: editor
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: Math.max(Theme.itemSizeHuge * 2.5, implicitHeight)
                font.family: "monospace"
                font.pixelSize: Theme.fontSizeExtraSmall
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                text: seite.vorlagen[seite.sprache]
            }

            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingSmall

                Repeater {
                    model: seite.zeichen[seite.sprache]
                    BackgroundItem {
                        width: (parent.width - 7 * Theme.paddingSmall) / 8
                        height: Theme.itemSizeExtraSmall * 0.7
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.paddingSmall
                            color: Theme.rgba(Theme.highlightBackgroundColor, 0.12)
                        }
                        Label {
                            anchors.centerIn: parent
                            text: modelData === "    " ? "␣␣" : modelData
                            font.family: "monospace"
                            color: Theme.primaryColor
                        }
                        onClicked: {
                            var stelle = editor.cursorPosition
                            editor.text = editor.text.substring(0, stelle) + modelData
                                        + editor.text.substring(stelle)
                            editor.cursorPosition = stelle + modelData.length
                        }
                    }
                }
            }

            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingMedium

                Button {
                    width: (parent.width - Theme.paddingMedium) * 0.62
                    text: course.running ? W.w("läuft …", course.language) : W.w("Ausführen", course.language)
                    enabled: !course.running
                    onClicked: course.runCode(editor.text, 30, seite.sprache)
                }
                Button {
                    width: (parent.width - Theme.paddingMedium) * 0.38
                    text: W.w("Stopp", course.language)
                    enabled: course.running
                    onClicked: course.stopRun()
                }
            }

            OutputBlock {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
            }

            Item { width: 1; height: Theme.paddingLarge }
        }

        VerticalScrollDecorator { }
    }
}
