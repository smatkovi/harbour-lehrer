#include "Curriculum.h"
#include "Json.h"

bool Curriculum::load(const QString &path, QString *error)
{
    const QVariant data = Json::parseFile(path, error);
    if (!data.isValid())
        return false;
    const QVariantMap root = data.toMap();
    m_chapters = root.value(QLatin1String("kapitel")).toList();
    m_plan = root.value(QLatin1String("plan")).toList();
    const QVariantMap placement = root.value(QLatin1String("einstufung")).toMap();
    m_items = placement.value(QLatin1String("fragen")).toList();
    m_topics = placement.value(QLatin1String("themen")).toMap();
    m_title = root.value(QLatin1String("titel"));
    m_subtitle = root.value(QLatin1String("untertitel"));
    // Only a course that carries runnable code gets a Run button and a
    // playground; a theory course would only be promising something it
    // cannot do.
    m_canRun = root.value(QLatin1String("ausfuehrbar"), false).toBool();

    m_languages.clear();
    for (const QVariant &code : root.value(QLatin1String("sprachen")).toList())
        m_languages << code.toString();
    if (m_languages.isEmpty())
        m_languages << QLatin1String("de");
    if (m_language.isEmpty() || !m_languages.contains(m_language))
        m_language = m_languages.first();

    if (m_chapters.isEmpty()) {
        if (error)
            *error = QLatin1String("Der Kurs ist leer.");
        return false;
    }
    return true;
}

QVariantList Curriculum::allLessons() const
{
    QVariantList out;
    for (const QVariant &chapterValue : m_chapters) {
        const QVariantMap chapter = chapterValue.toMap();
        for (const QVariant &lessonValue :
                 chapter.value(QLatin1String("lektionen")).toList()) {
            QVariantMap pair;
            pair.insert(QLatin1String("kapitel"), chapter);
            pair.insert(QLatin1String("lektion"), lessonValue.toMap());
            out.append(pair);
        }
    }
    return out;
}

QVariantMap Curriculum::chapterOf(const QString &lessonId) const
{
    for (const QVariant &chapterValue : m_chapters) {
        const QVariantMap chapter = chapterValue.toMap();
        for (const QVariant &lessonValue :
                 chapter.value(QLatin1String("lektionen")).toList()) {
            if (lessonValue.toMap().value(QLatin1String("id")).toString() == lessonId)
                return chapter;
        }
    }
    return QVariantMap();
}

QVariantMap Curriculum::lesson(const QString &lessonId) const
{
    for (const QVariant &chapterValue : m_chapters) {
        for (const QVariant &lessonValue :
                 chapterValue.toMap().value(QLatin1String("lektionen")).toList()) {
            const QVariantMap lesson = lessonValue.toMap();
            if (lesson.value(QLatin1String("id")).toString() == lessonId)
                return lesson;
        }
    }
    return QVariantMap();
}

QString Curriculum::text(const QVariant &value) const
{
    // A bare string is language-independent by construction: that is how
    // code samples and numbers are stored.
    if (value.type() != QVariant::Map)
        return value.toString();
    const QVariantMap pair = value.toMap();
    if (pair.contains(m_language))
        return pair.value(m_language).toString();
    // Fall back rather than show nothing: a half-translated course should
    // still be readable.
    for (const QString &code : m_languages) {
        if (pair.contains(code))
            return pair.value(code).toString();
    }
    return QString();
}

QVariantList Curriculum::textList(const QVariant &value) const
{
    QVariantList out;
    for (const QVariant &item : value.toList())
        out.append(text(item));
    return out;
}

int Curriculum::highestChapterLevel() const
{
    int highest = 0;
    for (const QVariant &chapterValue : m_chapters)
        highest = qMax(highest, chapterValue.toMap().value(QLatin1String("stufe")).toInt());
    return highest;
}
