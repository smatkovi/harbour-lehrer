#ifndef ENGINE_H
#define ENGINE_H

#include <QString>
#include <QStringList>
#include <QVariantMap>

class Curriculum;

// Progress, repetition, and deciding what comes next.
//
// Two ideas carry it:
//
//  * After the placement test the course carries on where the test left
//    off. The result is a level, the chapters carry the same levels, so
//    the next lesson is the first unfinished one at that level.
//    Everything below stays reachable but is marked as already known --
//    nobody should have to click past chapters they just proved they can do.
//
//  * What was answered right comes back later anyway. Every lesson names
//    the concepts it teaches, and each concept is a card with a due date:
//    right pushes the next showing further out, wrong brings it back to
//    tomorrow. Retrieval after a delay is what makes something stick.
class Engine
{
public:
    explicit Engine(const Curriculum *course);

    void load();
    void save() const;

    // Die gewaehlte Sprache gehoert zum Fortschritt: wer auf Englisch
    // umstellt, will beim naechsten Start nicht wieder Deutsch sehen.
    QString savedLanguage() const
    { return m_data.value(QLatin1String("sprache")).toString(); }
    void rememberLanguage(const QString &code)
    { m_data.insert(QLatin1String("sprache"), code); save(); }

    int level() const;
    bool needsPlacement() const { return level() <= 0; }

    void applyPlacement(const QVariantMap &result);
    QVariantList placementReview() const;
    QStringList weakTopics(const QVariantMap &topicNames) const;

    QString lessonState(const QString &lessonId) const;
    QVariantMap nextLesson() const;           // {kapitel, lektion} or empty
    void finishLesson(const QString &lessonId, int right, int total);

    void saveSolution(const QString &lessonId, int index, const QString &text);
    QString solution(const QString &lessonId, int index) const;

    void schedule(const QString &conceptName, bool right);

    // Flashcards. Same spaced scheme as the concepts, but keyed per
    // question instead of per topic, and kept in their own map so that
    // working through a lesson and drilling cards do not overwrite each
    // other's schedule.
    void scheduleCard(const QString &cardId, bool right);
    QVariantMap cardState(const QString &cardId) const;
    QStringList dueCards(const QStringList &all) const;
    QVariantMap cardStats(const QStringList &all) const;
    QStringList dueConcepts() const;
    QVariantList dueLessons() const;

    QVariantMap stats() const;

private:
    QString path() const;

    const Curriculum *m_course;
    QVariantMap m_data;
};

#endif
