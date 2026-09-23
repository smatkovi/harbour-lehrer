#include "Json.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTextStream>

// Qt 5 bringt JSON selbst mit. Die Harmattan-Fassung dieser Datei musste den
// Umweg ueber QtScript nehmen -- dort gab es QJsonDocument noch nicht.

namespace Json {

QVariant parse(const QString &text, QString *error)
{
    QJsonParseError fehler;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &fehler);
    if (fehler.error != QJsonParseError::NoError) {
        if (error)
            *error = fehler.errorString();
        return QVariant();
    }
    if (doc.isObject())
        return doc.object().toVariantMap();
    if (doc.isArray())
        return doc.array().toVariantList();
    return QVariant();
}

QVariant parseFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("%1 laesst sich nicht lesen").arg(path);
        return QVariant();
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    return parse(stream.readAll(), error);
}

QString stringify(const QVariant &value)
{
    return QString::fromUtf8(QJsonDocument::fromVariant(value)
                             .toJson(QJsonDocument::Compact));
}

bool writeFile(const QString &path, const QVariant &value)
{
    // Erst daneben schreiben, dann umbenennen: ein Absturz mitten im
    // Schreiben darf den Lernfortschritt nicht zerreissen.
    const QString vorlaeufig = path + QLatin1String(".neu");
    QFile file(vorlaeufig);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << stringify(value);
    file.close();
    QFile::remove(path);
    return QFile::rename(vorlaeufig, path);
}

}
