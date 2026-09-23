#include "Placement.h"

#include <QtGlobal>
#include <cmath>

namespace {

const int MaxQuestions = 20;
const int BreadthQuestions = 3;

const char *const TopicOrder[] = {
    "grundlagen", "fluss", "funktionen", "zeiger",
    "speicher", "numerik", "cpp", "stroemung"
};

}

void Placement::begin(const QVariantList &items, const QVariantMap &topics)
{
    m_items = items;
    m_topics = topics;
    m_used.clear();
    m_answers.clear();
    m_current.clear();
    m_level = 4.0;
    m_step = 2.0;
    m_reversals = 0;
    m_streak = 0;
    m_lastRight = -1;
    m_breadth = false;
    m_breadthLeft = 0;
}

QVariantMap Placement::byLevel(int level, int reach) const
{
    // "or nothing" matters: without the reach limit the test keeps handing a
    // beginner ever harder questions simply because the easy ones are used up.
    QVariantMap best;
    int distance = 99;
    for (const QVariant &value : m_items) {
        const QVariantMap item = value.toMap();
        if (m_used.contains(item.value(QLatin1String("id")).toString()))
            continue;
        const int gap = qAbs(item.value(QLatin1String("stufe")).toInt() - level);
        if (gap < distance) {
            best = item;
            distance = gap;
        }
    }
    if (best.isEmpty() || distance > reach)
        return QVariantMap();
    return best;
}

QVariantMap Placement::byTopic(const QString &topic, int level) const
{
    QVariantMap best;
    int distance = 99;
    for (const QVariant &value : m_items) {
        const QVariantMap item = value.toMap();
        if (m_used.contains(item.value(QLatin1String("id")).toString()))
            continue;
        if (item.value(QLatin1String("thema")).toString() != topic)
            continue;
        const int gap = qAbs(item.value(QLatin1String("stufe")).toInt() - level);
        if (gap < distance) {
            best = item;
            distance = gap;
        }
    }
    return best;
}

QStringList Placement::seenTopics() const
{
    QStringList out;
    for (const QVariant &value : m_answers)
        out << value.toMap().value(QLatin1String("thema")).toString();
    return out;
}

bool Placement::settled() const
{
    if (m_answers.size() >= MaxQuestions)
        return true;
    if (m_reversals >= 4)
        return true;
    // Pinned at an end and still going the same way: the answer is clear.
    if (m_streak >= 3 && (m_level <= 1.0 || m_level >= 11.0))
        return true;
    return false;
}

QVariantMap Placement::nextItem()
{
    if (!m_breadth) {
        if (!settled()) {
            const QVariantMap found = byLevel(int(qRound(m_level)));
            if (!found.isEmpty()) {
                m_current = found;
                m_used << found.value(QLatin1String("id")).toString();
                return m_current;
            }
        }
        m_breadth = true;
        m_breadthLeft = BreadthQuestions;
    }

    if (m_breadthLeft > 0) {
        const QStringList seen = seenTopics();
        for (const char *const name : TopicOrder) {
            const QString topic = QLatin1String(name);
            if (seen.contains(topic))
                continue;
            const QVariantMap found = byTopic(topic, int(qRound(m_level)));
            if (!found.isEmpty()) {
                --m_breadthLeft;
                m_current = found;
                m_used << found.value(QLatin1String("id")).toString();
                return m_current;
            }
        }
    }

    m_current.clear();
    return m_current;
}

bool Placement::answer(int chosen)
{
    if (m_current.isEmpty())
        return false;
    const bool right = (chosen == m_current.value(QLatin1String("antwort")).toInt());

    QVariantMap record;
    record.insert(QLatin1String("frage"), m_current);
    record.insert(QLatin1String("gewaehlt"), chosen);
    record.insert(QLatin1String("korrekt"), right);
    record.insert(QLatin1String("thema"), m_current.value(QLatin1String("thema")));
    record.insert(QLatin1String("phase"),
                  m_breadth ? QLatin1String("breite") : QLatin1String("adaptiv"));
    m_answers.append(record);

    if (m_breadth)
        return right;            // breadth questions do not move the estimate

    if (m_lastRight >= 0 && right != bool(m_lastRight)) {
        ++m_reversals;
        m_step = qMax(1.0, m_step - 1.0);
        m_streak = 1;
    } else {
        ++m_streak;
    }
    m_lastRight = right ? 1 : 0;
    m_level += right ? m_step : -m_step;
    m_level = qBound(1.0, m_level, 11.0);
    return right;
}

QVariantMap Placement::result() const
{
    int right = 0;
    int highestSolved = 0;
    int coreTotal = 0;
    int coreRight = 0;

    for (const QVariant &value : m_answers) {
        const QVariantMap record = value.toMap();
        const bool ok = record.value(QLatin1String("korrekt")).toBool();
        const QVariantMap item = record.value(QLatin1String("frage")).toMap();
        if (ok) {
            ++right;
            highestSolved = qMax(highestSolved,
                                 item.value(QLatin1String("stufe")).toInt());
        }
        // Only the adaptive part decides the level: the breadth questions
        // deliberately reach into topics the learner has not met, so counting
        // them would push everyone a level down.
        if (record.value(QLatin1String("phase")).toString() == QLatin1String("adaptiv")) {
            ++coreTotal;
            if (ok)
                ++coreRight;
        }
    }

    int level = highestSolved > 0 ? highestSolved : 1;
    const double share = coreTotal ? double(coreRight) / coreTotal : 0.0;
    if (share < 0.5)
        level = qMax(1, level - 1);
    level = qBound(1, level, 11);

    QVariantMap hits, totals;
    for (const QVariant &value : m_answers) {
        const QVariantMap record = value.toMap();
        const QString topic = record.value(QLatin1String("thema")).toString();
        totals.insert(topic, totals.value(topic, 0).toInt() + 1);
        if (record.value(QLatin1String("korrekt")).toBool())
            hits.insert(topic, hits.value(topic, 0).toInt() + 1);
    }
    QVariantMap profile;
    for (auto it = totals.constBegin(); it != totals.constEnd(); ++it) {
        profile.insert(it.key(),
                       double(hits.value(it.key(), 0).toInt()) / it.value().toInt());
    }

    QVariantList review;
    for (const QVariant &value : m_answers) {
        const QVariantMap record = value.toMap();
        const QVariantMap item = record.value(QLatin1String("frage")).toMap();
        QVariantMap entry;
        entry.insert(QLatin1String("frage"), item.value(QLatin1String("frage")));
        entry.insert(QLatin1String("code"), item.value(QLatin1String("code")));
        entry.insert(QLatin1String("optionen"), item.value(QLatin1String("optionen")));
        entry.insert(QLatin1String("richtig"), item.value(QLatin1String("antwort")));
        entry.insert(QLatin1String("gewaehlt"), record.value(QLatin1String("gewaehlt")));
        entry.insert(QLatin1String("korrekt"), record.value(QLatin1String("korrekt")));
        entry.insert(QLatin1String("warum"), item.value(QLatin1String("warum")));
        entry.insert(QLatin1String("stufe"), item.value(QLatin1String("stufe")));
        entry.insert(QLatin1String("thema"),
                     m_topics.value(record.value(QLatin1String("thema")).toString(),
                                    record.value(QLatin1String("thema"))));
        review.append(entry);
    }

    QVariantMap out;
    out.insert(QLatin1String("level"), level);
    out.insert(QLatin1String("richtig"), right);
    out.insert(QLatin1String("gesamt"), m_answers.size());
    out.insert(QLatin1String("profil"), profile);
    out.insert(QLatin1String("rueckblick"), review);
    return out;
}
