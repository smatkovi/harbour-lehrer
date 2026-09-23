#include "Checker.h"

#include <QRegExp>
#include <cmath>

namespace {

const double Tolerance = 1e-6;

bool isNumber(const QString &token, double *value)
{
    bool ok = false;
    const double parsed = token.toDouble(&ok);
    if (ok && value)
        *value = parsed;
    return ok;
}

bool sameNumber(double a, double b)
{
    if (std::isnan(a) || std::isnan(b))
        return std::isnan(a) && std::isnan(b);
    return std::fabs(a - b) <= Tolerance * std::max(1.0, std::fabs(b));
}

QStringList tokens(const QString &text)
{
    return text.trimmed().split(QRegExp(QLatin1String("\\s+")),
                                QString::SkipEmptyParts);
}

}

namespace Checker {

bool outputMatches(const QString &got, const QString &want)
{
    const QStringList mine = tokens(got);
    const QStringList theirs = tokens(want);
    if (mine.size() != theirs.size())
        return false;
    for (int i = 0; i < mine.size(); ++i) {
        if (mine.at(i) == theirs.at(i))
            continue;
        double a = 0.0, b = 0.0;
        if (isNumber(mine.at(i), &a) && isNumber(theirs.at(i), &b)
                && sameNumber(a, b))
            continue;
        return false;
    }
    return true;
}

bool textMatches(const QString &got, const QString &want,
                 const QStringList &alternatives)
{
    // For a short written answer neither spacing nor case is the point.
    auto flatten = [](const QString &text) {
        QString out = text.trimmed().toLower();
        out.remove(QRegExp(QLatin1String("\\s+")));
        return out;
    };
    const QString mine = flatten(got);
    if (mine == flatten(want))
        return true;
    for (const QString &candidate : alternatives) {
        if (mine == flatten(candidate))
            return true;
    }
    return false;
}

}
