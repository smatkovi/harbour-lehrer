#include "Engine.h"
#include "Curriculum.h"
#include "Json.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QStringList>

namespace {

const double Day = 24 * 3600;
const double EaseStart = 2.3;
const double EaseMin = 1.3;
const double EaseMax = 2.8;

double now()
{
    return QDateTime::currentDateTime().toTime_t();
}

QString configDir()
{
    // Auf Sailfish gibt die Sandbox einer App genau ihren eigenen Ordner
    // frei -- den, der zur Kennung in der Desktop-Datei passt. Deshalb wird
    // der Ort nicht selbst zusammengesetzt, sondern von Qt erfragt:
    // ~/.local/share/<app>. Wer hier eigene Pfade baut, hat eine App, die
    // aus der Konsole alles behaelt und ueber das Startsymbol nichts --
    // derselbe Fehler, der in harbour-tarock einen Abend gekostet hat.
    //
    // Dass der Ordner den Programmnamen traegt, bleibt wichtig: dasselbe
    // Programm traegt drei Kurse, und zwei Kurse in einer Fortschrittsdatei
    // wuerden sich gegenseitig die Stufe setzen.
    const QString ort = QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation);
    if (!ort.isEmpty())
        return ort;
    QString name = QCoreApplication::applicationName();
    if (name.isEmpty())
        name = QLatin1String("harbour-clehrer");
    return QDir::homePath() + QLatin1String("/.local/share/") + name;
}

}

Engine::Engine(const Curriculum *course)
    : m_course(course)
{
}

QString Engine::path() const
{
    return configDir() + QLatin1String("/fortschritt.json");
}

void Engine::load()
{
    const QVariant data = Json::parseFile(path());
    m_data = data.isValid() ? data.toMap() : QVariantMap();
}

void Engine::save() const
{
    QDir().mkpath(configDir());
    Json::writeFile(path(), m_data);
}

int Engine::level() const
{
    return m_data.value(QLatin1String("level"), 0).toInt();
}

QString Engine::lessonState(const QString &lessonId) const
{
    const QVariantMap lessons = m_data.value(QLatin1String("lektionen")).toMap();
    const QVariantMap entry = lessons.value(lessonId).toMap();
    return entry.value(QLatin1String("stand"), QLatin1String("neu")).toString();
}

void Engine::applyPlacement(const QVariantMap &result)
{
    m_data.insert(QLatin1String("level"), result.value(QLatin1String("level")));
    m_data.insert(QLatin1String("profil"), result.value(QLatin1String("profil")));

    QVariantMap record;
    record.insert(QLatin1String("zeit"), now());
    record.insert(QLatin1String("level"), result.value(QLatin1String("level")));
    record.insert(QLatin1String("richtig"), result.value(QLatin1String("richtig")));
    record.insert(QLatin1String("gesamt"), result.value(QLatin1String("gesamt")));
    record.insert(QLatin1String("rueckblick"), result.value(QLatin1String("rueckblick")));
    m_data.insert(QLatin1String("einstufung"), record);

    // Everything below the level reached counts as known: still open to
    // read, but no longer in the way of the next lesson.
    const int level = result.value(QLatin1String("level")).toInt();
    QVariantMap lessons = m_data.value(QLatin1String("lektionen")).toMap();
    for (const QVariant &pairValue : m_course->allLessons()) {
        const QVariantMap pair = pairValue.toMap();
        if (pair.value(QLatin1String("kapitel")).toMap()
                .value(QLatin1String("stufe")).toInt() >= level)
            continue;
        const QString id = pair.value(QLatin1String("lektion")).toMap()
                .value(QLatin1String("id")).toString();
        QVariantMap entry = lessons.value(id).toMap();
        if (entry.value(QLatin1String("stand")).toString() != QLatin1String("fertig"))
            entry.insert(QLatin1String("stand"), QLatin1String("bekannt"));
        lessons.insert(id, entry);
    }
    m_data.insert(QLatin1String("lektionen"), lessons);

    const QVariantMap next = nextLesson();
    m_data.insert(QLatin1String("letzte"),
                  next.value(QLatin1String("lektion")).toMap()
                      .value(QLatin1String("id")).toString());
    save();
}

QVariantList Engine::placementReview() const
{
    return m_data.value(QLatin1String("einstufung")).toMap()
            .value(QLatin1String("rueckblick")).toList();
}

QStringList Engine::weakTopics(const QVariantMap &topicNames) const
{
    const QVariantMap profile = m_data.value(QLatin1String("profil")).toMap();
    QList<QPair<double, QString> > pairs;
    for (auto it = profile.constBegin(); it != profile.constEnd(); ++it) {
        if (it.value().toDouble() >= 0.75)
            continue;
        const QVariant name = topicNames.value(it.key(), it.key());
        pairs.append(qMakePair(it.value().toDouble(),
                               m_course->text(name.isValid() ? name
                                                             : QVariant(it.key()))));
    }
    std::sort(pairs.begin(), pairs.end());
    QStringList out;
    for (const auto &pair : pairs)
        out << pair.second;
    return out;
}

QVariantMap Engine::nextLesson() const
{
    const int level = qMax(1, this->level());
    QVariantList pending;
    for (const QVariant &pairValue : m_course->allLessons()) {
        const QVariantMap pair = pairValue.toMap();
        const QString id = pair.value(QLatin1String("lektion")).toMap()
                .value(QLatin1String("id")).toString();
        if (lessonState(id) == QLatin1String("fertig"))
            continue;
        pending.append(pair);
    }

    QVariantMap best;
    int bestLevel = 0;
    // At or above the placement level first -- that is where the learner is.
    for (const QVariant &pairValue : pending) {
        const QVariantMap pair = pairValue.toMap();
        const int chapterLevel = pair.value(QLatin1String("kapitel")).toMap()
                .value(QLatin1String("stufe")).toInt();
        if (chapterLevel < level)
            continue;
        if (best.isEmpty() || chapterLevel < bestLevel) {
            best = pair;
            bestLevel = chapterLevel;
        }
    }
    if (!best.isEmpty())
        return best;

    // Nothing left up there -- the learner placed above everything that is
    // written so far. Then the *highest* chapter is the useful one, not the
    // lowest: someone at level 8 does not want to be sent back to printf.
    for (const QVariant &pairValue : pending) {
        const QVariantMap pair = pairValue.toMap();
        const int chapterLevel = pair.value(QLatin1String("kapitel")).toMap()
                .value(QLatin1String("stufe")).toInt();
        if (best.isEmpty() || chapterLevel > bestLevel) {
            best = pair;
            bestLevel = chapterLevel;
        }
    }
    return best;
}

void Engine::finishLesson(const QString &lessonId, int right, int total)
{
    QVariantMap lessons = m_data.value(QLatin1String("lektionen")).toMap();
    QVariantMap entry = lessons.value(lessonId).toMap();
    entry.insert(QLatin1String("stand"), QLatin1String("fertig"));
    entry.insert(QLatin1String("richtig"), right);
    entry.insert(QLatin1String("gesamt"), total);
    entry.insert(QLatin1String("zeit"), now());
    lessons.insert(lessonId, entry);
    m_data.insert(QLatin1String("lektionen"), lessons);

    const QVariantMap lesson = m_course->lesson(lessonId);
    const double share = total > 0 ? double(right) / total : 1.0;
    for (const QVariant &concept : lesson.value(QLatin1String("begriffe")).toList())
        schedule(concept.toString(), share >= 0.6);

    m_data.insert(QLatin1String("letzte"), lessonId);
    save();
}

void Engine::saveSolution(const QString &lessonId, int index, const QString &text)
{
    QVariantMap store = m_data.value(QLatin1String("loesungen")).toMap();
    store.insert(QString::fromLatin1("%1/%2").arg(lessonId).arg(index), text);
    m_data.insert(QLatin1String("loesungen"), store);
    save();
}

QString Engine::solution(const QString &lessonId, int index) const
{
    return m_data.value(QLatin1String("loesungen")).toMap()
            .value(QString::fromLatin1("%1/%2").arg(lessonId).arg(index)).toString();
}

void Engine::schedule(const QString &conceptName, bool right)
{
    QVariantMap cards = m_data.value(QLatin1String("begriffe")).toMap();
    QVariantMap card = cards.value(conceptName).toMap();
    if (card.isEmpty()) {
        card.insert(QLatin1String("ease"), EaseStart);
        card.insert(QLatin1String("tage"), 0);
        card.insert(QLatin1String("faellig"), 0.0);
        card.insert(QLatin1String("wdh"), 0);
    }
    double ease = card.value(QLatin1String("ease"), EaseStart).toDouble();
    int days = card.value(QLatin1String("tage"), 0).toInt();
    int reps = card.value(QLatin1String("wdh"), 0).toInt();

    if (right) {
        ++reps;
        if (reps == 1)
            days = 1;
        else if (reps == 2)
            days = 3;
        else
            days = qMax(1, int(qRound(days * ease)));
        ease = qMin(EaseMax, ease + 0.1);
    } else {
        reps = 0;
        days = 1;
        ease = qMax(EaseMin, ease - 0.2);
    }

    card.insert(QLatin1String("ease"), ease);
    card.insert(QLatin1String("tage"), days);
    card.insert(QLatin1String("wdh"), reps);
    card.insert(QLatin1String("faellig"), now() + days * Day);
    cards.insert(conceptName, card);
    m_data.insert(QLatin1String("begriffe"), cards);
}

namespace {

// One step of the spaced scheme, shared by concepts and cards.
QVariantMap advance(QVariantMap card, bool right)
{
    double ease = card.value(QLatin1String("ease"), EaseStart).toDouble();
    int days = card.value(QLatin1String("tage"), 0).toInt();
    int reps = card.value(QLatin1String("wdh"), 0).toInt();

    if (right) {
        ++reps;
        if (reps == 1)
            days = 1;
        else if (reps == 2)
            days = 3;
        else
            days = qMax(1, int(qRound(days * ease)));
        ease = qMin(EaseMax, ease + 0.1);
    } else {
        reps = 0;
        days = 1;
        ease = qMax(EaseMin, ease - 0.2);
    }

    card.insert(QLatin1String("ease"), ease);
    card.insert(QLatin1String("tage"), days);
    card.insert(QLatin1String("wdh"), reps);
    card.insert(QLatin1String("faellig"), now() + days * Day);
    card.insert(QLatin1String("gesehen"), card.value(QLatin1String("gesehen"), 0).toInt() + 1);
    return card;
}

}

void Engine::scheduleCard(const QString &cardId, bool right)
{
    QVariantMap cards = m_data.value(QLatin1String("karten")).toMap();
    cards.insert(cardId, advance(cards.value(cardId).toMap(), right));
    m_data.insert(QLatin1String("karten"), cards);
    save();
}

QVariantMap Engine::cardState(const QString &cardId) const
{
    return m_data.value(QLatin1String("karten")).toMap().value(cardId).toMap();
}

QStringList Engine::dueCards(const QStringList &all) const
{
    const QVariantMap cards = m_data.value(QLatin1String("karten")).toMap();
    QStringList due, fresh;
    for (const QString &id : all) {
        if (!cards.contains(id)) {
            fresh << id;                 // never seen: always worth showing
            continue;
        }
        if (cards.value(id).toMap().value(QLatin1String("faellig"), 0.0).toDouble() <= now())
            due << id;
    }
    // Due repetitions first: forgetting something already learned costs
    // more than not having met the next card yet.
    return due + fresh;
}

QVariantMap Engine::cardStats(const QStringList &all) const
{
    const QVariantMap cards = m_data.value(QLatin1String("karten")).toMap();
    int seen = 0, due = 0, learned = 0;
    for (const QString &id : all) {
        if (!cards.contains(id))
            continue;
        ++seen;
        const QVariantMap card = cards.value(id).toMap();
        if (card.value(QLatin1String("faellig"), 0.0).toDouble() <= now())
            ++due;
        if (card.value(QLatin1String("wdh"), 0).toInt() >= 3)
            ++learned;
    }
    QVariantMap out;
    out.insert(QLatin1String("gesamt"), all.size());
    out.insert(QLatin1String("gesehen"), seen);
    out.insert(QLatin1String("faellig"), due + (all.size() - seen));
    out.insert(QLatin1String("sitzt"), learned);
    return out;
}

QStringList Engine::dueConcepts() const
{
    QStringList out;
    const QVariantMap cards = m_data.value(QLatin1String("begriffe")).toMap();
    for (auto it = cards.constBegin(); it != cards.constEnd(); ++it) {
        if (it.value().toMap().value(QLatin1String("faellig"), 0.0).toDouble() <= now())
            out << it.key();
    }
    return out;
}

QVariantList Engine::dueLessons() const
{
    const QStringList due = dueConcepts();
    QVariantList out;
    if (due.isEmpty())
        return out;
    for (const QVariant &pairValue : m_course->allLessons()) {
        const QVariantMap pair = pairValue.toMap();
        const QVariantMap lesson = pair.value(QLatin1String("lektion")).toMap();
        if (lessonState(lesson.value(QLatin1String("id")).toString())
                != QLatin1String("fertig"))
            continue;
        for (const QVariant &concept : lesson.value(QLatin1String("begriffe")).toList()) {
            if (due.contains(concept.toString())) {
                out.append(pair);
                break;
            }
        }
    }
    return out;
}

QVariantMap Engine::stats() const
{
    const QVariantList lessons = m_course->allLessons();
    int done = 0;
    for (const QVariant &pairValue : lessons) {
        const QString id = pairValue.toMap().value(QLatin1String("lektion")).toMap()
                .value(QLatin1String("id")).toString();
        if (lessonState(id) == QLatin1String("fertig"))
            ++done;
    }
    QVariantMap out;
    out.insert(QLatin1String("fertig"), done);
    out.insert(QLatin1String("gesamt"), lessons.size());
    out.insert(QLatin1String("faellig"), dueLessons().size());
    out.insert(QLatin1String("level"), level());
    out.insert(QLatin1String("hoechstesKapitel"), m_course->highestChapterLevel());
    return out;
}
