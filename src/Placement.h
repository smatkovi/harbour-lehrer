#ifndef PLACEMENT_H
#define PLACEMENT_H

#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// The placement test: find out where to start, in about ten questions.
//
// Adaptive rather than fixed. It starts in the middle, moves two levels
// while the estimate is still far off and one once it has over- and
// undershot, and stops as soon as the estimate settles -- so a beginner is
// not walked through fifteen questions about pointers before being told
// they are a beginner.
//
// After that a short breadth round asks about topics the adaptive part
// never reached, at the level it found. Without it a quick run names two
// topics out of eight, which is no use for saying what to work on. Those
// answers deliberately do not move the level.
class Placement
{
public:
    Placement() = default;
    void begin(const QVariantList &items, const QVariantMap &topics);

    QVariantMap nextItem();       // empty map when the test is over
    bool answer(int chosen);      // true if that was right
    bool isRunning() const { return !m_current.isEmpty(); }
    QVariantMap current() const { return m_current; }
    int asked() const { return m_answers.size(); }

    QVariantMap result() const;

private:
    QVariantMap byLevel(int level, int reach = 2) const;
    QVariantMap byTopic(const QString &topic, int level) const;
    QStringList seenTopics() const;
    bool settled() const;

    QVariantList m_items;
    QVariantMap m_topics;
    QStringList m_used;
    QVariantList m_answers;       // {frage, gewaehlt, korrekt, phase}
    QVariantMap m_current;
    double m_level = 4.0;
    double m_step = 2.0;
    int m_reversals = 0;
    int m_streak = 0;
    int m_lastRight = -1;         // -1 = nothing answered yet
    int m_breadthLeft = 0;
    bool m_breadth = false;
};

#endif
