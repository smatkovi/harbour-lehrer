#ifndef JSON_H
#define JSON_H

#include <QString>
#include <QVariant>

// Qt 5 bringt JSON mit: QJsonDocument liest und schreibt, und QVariant ist
// die Waehrung, in der der Rest des Programms rechnet. Die Harmattan-Fassung
// dieser Datei nahm dafuer den Umweg ueber die ECMAScript-Maschine von
// QtScript -- dort gab es QJsonDocument noch nicht.
namespace Json {

QVariant parse(const QString &text, QString *error = nullptr);
QVariant parseFile(const QString &path, QString *error = nullptr);
QString stringify(const QVariant &value);
bool writeFile(const QString &path, const QVariant &value);

}

#endif
