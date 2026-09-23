// Lernapp fuer Sailfish OS: C, C++, Rust und Python -- oder Segeln, oder
// Segelflug. Dasselbe Programm, drei Kurse.
//
// Der Kurs steckt vollstaendig in data/kurs.json; das Programm weiss nichts
// ueber sein Fach. Deshalb ist ein zweiter Kurs ein zweites Paket und kein
// zweites Programm -- gebaut wird dieselbe Quelle mit einem anderen
// APP_NAME.
//
// Portiert von der Harmattan-Fassung (Qt 4.7, QtQuick 1.1, com.nokia.meego)
// auf Qt 5 und Silica. Was dabei wirklich neu geschrieben werden musste,
// war die Oberflaeche; die Maschine darunter -- Kurs, Pruefer, Einstufung,
// Laeufer, Zeichner -- ist dieselbe.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QtGlobal>

#include <sailfishapp.h>

#include "Course.h"
#include "Curriculum.h"
#include "Plotter.h"

int main(int argc, char **argv)
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));

    // Beides gleich dem Paketnamen. Die Sandbox gibt genau diese Kennung
    // frei -- steht hier etwas anderes als im [X-Sailjail]-Block der
    // Desktop-Datei, merkt sich die App ueber das Startsymbol nichts.
    app->setOrganizationName(QLatin1String(APP_NAME));
    app->setApplicationName(QLatin1String(APP_NAME));

    Curriculum curriculum;
    QString fehler;
    const QString kurs = QLatin1String(APP_SHARE_DIR) + QLatin1String("/data/kurs.json");
    if (!curriculum.load(kurs, &fehler)) {
        qWarning("Kurs nicht lesbar: %s", qPrintable(fehler));
        return 1;
    }

    Plotter *plotter = new Plotter();          // die Ansicht uebernimmt ihn
    Course course(&curriculum, plotter);

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->engine()->addImageProvider(QLatin1String("plot"), plotter);
    view->rootContext()->setContextProperty(QLatin1String("course"), &course);
    // Wo die Bilder des Kurses liegen -- die QML-Seiten setzen den Pfad
    // selbst zusammen, damit sie von der Paketierung nichts wissen muessen.
    view->rootContext()->setContextProperty(QLatin1String("bilderPfad"),
                                            QLatin1String(APP_SHARE_DIR)
                                            + QLatin1String("/bilder/"));
    view->setSource(SailfishApp::pathTo(QLatin1String("qml/main.qml")));
    view->show();
    return app->exec();
}
