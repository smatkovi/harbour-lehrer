#include "Plotter.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace {

const int DefaultWidth = 620;
const int DefaultHeight = 380;
const int MarginLeft = 64;
const int MarginRight = 14;
const int MarginTop = 14;
const int MarginBottom = 34;

const char *const Colours[] = {"#5aa9ff", "#ffb14e", "#7ee787", "#ff6b6b", "#c792ea"};
const int ColourCount = 5;

// A grid spacing a person would have chosen.
double niceStep(double span, int target)
{
    if (span <= 0.0)
        return 1.0;
    const double rough = span / target;
    const double power = std::floor(std::log10(rough));
    const double base = std::pow(10.0, power);
    const double factors[] = {1.0, 2.0, 2.5, 5.0, 10.0};
    for (double factor : factors) {
        if (base * factor >= rough)
            return base * factor;
    }
    return base * 10.0;
}

bool usable(double value)
{
    return !std::isnan(value) && std::fabs(value) < 1e30;
}

}

Plotter::Plotter()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

void Plotter::setCurves(const QVector<Curve> &curves)
{
    m_curves = curves;
    ++m_revision;
}

QImage Plotter::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(id);
    const int width = requestedSize.width() > 0 ? requestedSize.width() : DefaultWidth;
    const int height = requestedSize.height() > 0 ? requestedSize.height() : DefaultHeight;
    if (size)
        *size = QSize(width, height);

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(QColor(QLatin1String("#101014")).rgba());

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(QFont(QLatin1String("Nokia Pure Text"), 11));

    double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
    bool first = true;
    for (const Curve &curve : m_curves) {
        for (const Point &point : curve.points) {
            if (!usable(point.y))
                continue;
            if (first) {
                minX = maxX = point.x;
                minY = maxY = point.y;
                first = false;
                continue;
            }
            minX = qMin(minX, point.x);
            maxX = qMax(maxX, point.x);
            minY = qMin(minY, point.y);
            maxY = qMax(maxY, point.y);
        }
    }
    if (first) {
        painter.setPen(QColor(QLatin1String("#d8d8e0")));
        painter.drawText(20, 30, QString::fromUtf8("Nichts zu zeichnen"));
        return image;
    }

    if (maxX - minX < 1e-12) { minX -= 1.0; maxX += 1.0; }
    if (maxY - minY < 1e-12) { minY -= 1.0; maxY += 1.0; }
    const double pad = (maxY - minY) * 0.08;
    minY -= pad;
    maxY += pad;

    const double plotW = width - MarginLeft - MarginRight;
    const double plotH = height - MarginTop - MarginBottom;
    auto sx = [&](double x) { return MarginLeft + (x - minX) / (maxX - minX) * plotW; };
    auto sy = [&](double y) {
        const double clamped = qBound(minY, y, maxY);
        return MarginTop + plotH - (clamped - minY) / (maxY - minY) * plotH;
    };

    const QColor gridColour(QLatin1String("#26262e"));
    const QColor textColour(QLatin1String("#d8d8e0"));

    const double stepY = niceStep(maxY - minY, 5);
    for (double tick = std::ceil(minY / stepY) * stepY; tick <= maxY; tick += stepY) {
        painter.setPen(QPen(gridColour, 1));
        painter.drawLine(QPointF(MarginLeft, sy(tick)),
                         QPointF(width - MarginRight, sy(tick)));
        painter.setPen(textColour);
        painter.drawText(QRectF(2, sy(tick) - 8, MarginLeft - 10, 16),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(tick, 'g', 4));
    }

    const double stepX = niceStep(maxX - minX, 5);
    for (double tick = std::ceil(minX / stepX) * stepX; tick <= maxX; tick += stepX) {
        painter.setPen(QPen(gridColour, 1));
        painter.drawLine(QPointF(sx(tick), MarginTop),
                         QPointF(sx(tick), MarginTop + plotH));
        painter.setPen(textColour);
        painter.drawText(QRectF(sx(tick) - 40, MarginTop + plotH + 4, 80, 18),
                         Qt::AlignHCenter | Qt::AlignTop,
                         QString::number(tick, 'g', 4));
    }

    painter.setPen(QPen(QColor(QLatin1String("#8a8a95")), 1));
    painter.drawLine(QPointF(MarginLeft, MarginTop), QPointF(MarginLeft, MarginTop + plotH));
    painter.drawLine(QPointF(MarginLeft, MarginTop + plotH),
                     QPointF(width - MarginRight, MarginTop + plotH));

    for (int index = 0; index < m_curves.size(); ++index) {
        const Curve &curve = m_curves.at(index);
        const QColor colour(QLatin1String(Colours[index % ColourCount]));
        painter.setPen(QPen(colour, 2));
        QPainterPath path;
        bool started = false;
        for (const Point &point : curve.points) {
            if (!usable(point.y)) {
                started = false;        // a gap rather than a wild jump
                continue;
            }
            const QPointF here(sx(point.x), sy(point.y));
            if (!started) {
                path.moveTo(here);
                started = true;
            } else {
                path.lineTo(here);
            }
        }
        painter.drawPath(path);
        if (!curve.name.isEmpty()) {
            painter.setPen(colour);
            painter.drawText(MarginLeft + 12, MarginTop + 18 + index * 18, curve.name);
        }
    }

    return image;
}
