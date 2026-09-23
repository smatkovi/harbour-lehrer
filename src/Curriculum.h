#ifndef CURRICULUM_H
#define CURRICULUM_H

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// The course, as read from data/kurs.json.
//
// The content is authored in Python (curriculum.py) because that is where
// the test harness lives: every example and every model solution is run
// through the real interpreter before it ships. This class only reads the
// result, so the app carries no authoring code.
class Curriculum
{
public:
    bool load(const QString &path, QString *error = nullptr);

    // A course may be written in one language or in several. Where it is
    // several, every readable string is an object {"de": ..., "en": ...}
    // rather than a bare string, so switching the language switches the
    // whole course and not just its menus. Strings that are the same in
    // every language -- a piece of C, a number -- stay bare.
    void setLanguage(const QString &code) { m_language = code; }
    QString language() const { return m_language; }
    QStringList languages() const { return m_languages; }
    QString text(const QVariant &value) const;
    QVariantList textList(const QVariant &value) const;

    QVariantList chapters() const { return m_chapters; }
    QVariantList plan() const { return m_plan; }
    QVariantList placementItems() const { return m_items; }
    QVariantMap topics() const { return m_topics; }
    QString title() const { return text(m_title); }
    QString subtitle() const { return text(m_subtitle); }
    bool canRun() const { return m_canRun; }

    QVariantMap chapterOf(const QString &lessonId) const;
    QVariantMap lesson(const QString &lessonId) const;
    QVariantList allLessons() const;          // [{kapitel, lektion}, ...]
    int highestChapterLevel() const;

private:
    QVariantList m_chapters;
    QVariantList m_plan;
    QVariantList m_items;
    QVariantMap m_topics;
    QVariant m_title;
    QVariant m_subtitle;
    bool m_canRun = false;
    QString m_language;
    QStringList m_languages;
};

#endif
