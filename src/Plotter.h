#ifndef PLOTTER_H
#define PLOTTER_H

#include <QQuickImageProvider>
#include <QVector>

#include "Runner.h"

// Zeichnet, was das Programm des Lernenden ausgegeben hat.
//
// Gemalt wird mit QPainter und nicht mit dem Canvas von QtQuick 2: Das Bild
// entsteht ohnehin im C++-Teil, es aendert sich nur nach einem Lauf, und ein
// Bildlieferant kommt ohne Zwischendateien aus. QML fragt nach
// "image://plot/<n>" -- eine neue Nummer heisst ein neues Bild.
//
// The picture is the actual teacher in the numerics chapters: an explicit
// Euler step visibly gains energy, a time step past the stability limit
// visibly explodes. Reading that in a sentence convinces nobody. So the
// axes always follow the data, including when the data is absurd.
class Plotter : public QQuickImageProvider
{
public:
    Plotter();

    void setCurves(const QVector<Curve> &curves);
    int revision() const { return m_revision; }

    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override;

private:
    QVector<Curve> m_curves;
    int m_revision = 0;
};

#endif
