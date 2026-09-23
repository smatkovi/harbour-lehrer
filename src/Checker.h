#ifndef CHECKER_H
#define CHECKER_H

#include <QString>
#include <QStringList>

// Deciding whether an answer counts.
//
// Comparing program output as plain text does not work for numerics: two
// correct solutions can add the same terms in a different order and part
// company in the last digit. Failing someone for being right is the worst
// thing a teaching app can do, so anything that parses as a number is
// compared as a number, with a tolerance.
namespace Checker {

bool outputMatches(const QString &got, const QString &want);
bool textMatches(const QString &got, const QString &want,
                 const QStringList &alternatives = QStringList());

}

#endif
