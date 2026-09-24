#ifndef COURSE_H
#define COURSE_H

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "Curriculum.h"
#include "Engine.h"
#include "Placement.h"

class Plotter;
class Runner;

// Everything the QML talks to.
//
// All the course state lives here, so there is exactly one place where the
// logic is. The QML reads properties and calls slots; it decides nothing.
class Course : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int level READ level NOTIFY changed)
    Q_PROPERTY(bool needsPlacement READ needsPlacement NOTIFY changed)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY changed)
    Q_PROPERTY(QStringList weakTopics READ weakTopics NOTIFY changed)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY changed)
    Q_PROPERTY(QStringList languages READ languages NOTIFY changed)
    Q_PROPERTY(QString courseTitle READ courseTitle NOTIFY changed)
    Q_PROPERTY(QString courseSubtitle READ courseSubtitle NOTIFY changed)
    Q_PROPERTY(bool canRun READ canRun NOTIFY changed)
    Q_PROPERTY(QVariantMap cardStats READ cardStats NOTIFY cardChanged)
    Q_PROPERTY(QVariantMap card READ card NOTIFY cardChanged)
    Q_PROPERTY(bool cardsDone READ cardsDone NOTIFY cardChanged)

    Q_PROPERTY(QVariantMap placementQuestion READ placementQuestion NOTIFY placementChanged)
    Q_PROPERTY(bool placementDone READ placementDone NOTIFY placementChanged)
    Q_PROPERTY(QVariantMap placementResult READ placementResult NOTIFY placementChanged)

    Q_PROPERTY(QVariantMap lesson READ lesson NOTIFY lessonChanged)
    Q_PROPERTY(QVariantMap exercise READ exercise NOTIFY lessonChanged)

    Q_PROPERTY(bool running READ running NOTIFY runChanged)
    Q_PROPERTY(QString output READ output NOTIFY runChanged)
    Q_PROPERTY(QString error READ error NOTIFY runChanged)
    Q_PROPERTY(int errorLine READ errorLine NOTIFY runChanged)
    Q_PROPERTY(int plotRevision READ plotRevision NOTIFY runChanged)
    Q_PROPERTY(bool hasPlot READ hasPlot NOTIFY runChanged)
    Q_PROPERTY(double seconds READ seconds NOTIFY runChanged)

public:
    Course(Curriculum *course, Plotter *plotter, QObject *parent = nullptr);

    int level() const { return m_engine.level(); }
    bool needsPlacement() const { return m_engine.needsPlacement(); }
    QVariantMap stats() const;
    QStringList weakTopics() const { return m_engine.weakTopics(m_course->topics()); }
    QString language() const { return m_course->language(); }
    QStringList languages() const { return m_course->languages(); }
    void setLanguage(const QString &code);
    QString courseTitle() const { return m_course->title(); }
    QString courseSubtitle() const { return m_course->subtitle(); }
    bool canRun() const { return m_course->canRun(); }

    QVariantMap placementQuestion() const;
    bool placementDone() const { return !m_placementResult.isEmpty(); }
    QVariantMap placementResult() const { return m_placementResult; }

    QVariantMap lesson() const;
    QVariantMap exercise() const;

    bool running() const;
    QString output() const { return m_output; }
    QString error() const { return m_error; }
    int errorLine() const { return m_errorLine; }
    int plotRevision() const { return m_plotRevision; }
    bool hasPlot() const { return m_hasPlot; }
    double seconds() const { return m_seconds; }

    Q_INVOKABLE QVariantList chapters() const;
    Q_INVOKABLE QVariantList plan() const;
    Q_INVOKABLE QVariantMap nextLesson() const;
    Q_INVOKABLE QVariantList placementReview() const;

    Q_INVOKABLE void startPlacement();
    Q_INVOKABLE void answerPlacement(int chosen);

    Q_INVOKABLE void startLesson(const QString &lessonId);
    Q_INVOKABLE void toExercises();
    Q_INVOKABLE void nextExercise();
    Q_INVOKABLE void showSolution();
    // Noch einmal versuchen, ohne dass es die Bewertung veraendert: gezaehlt
    // wird der erste Versuch, und der ist schon vorbei.
    Q_INVOKABLE void retryExercise();

    Q_INVOKABLE bool answerChoice(int chosen);
    Q_INVOKABLE bool answerText(const QString &text);
    Q_INVOKABLE bool answerBlanks(const QVariantList &values);
    Q_INVOKABLE bool answerParsons(const QVariantList &lines);
    Q_INVOKABLE bool answerNumber(double value);

    // Die Sprache ist ein eigener Parameter, weil nicht jeder Lauf zu einer
    // Lektion gehoert: die Spielwiese hat ihren eigenen Quelltext und darf
    // ihn nicht mit der Sprache der zuletzt geoeffneten Lektion ausfuehren.
    // Leer heisst "die der Lektion".
    Q_INVOKABLE void runCode(const QString &code, int seconds,
                             const QString &language = QString());
    // Laesst sich diese Sprache auf diesem Geraet ausfuehren? Die Spielwiese
    // zeigt nur an, was auch laeuft.
    Q_INVOKABLE bool canRunLanguage(const QString &language) const;
    QString lessonLanguage() const;
    bool lessonRunnable() const;
    Q_INVOKABLE void stopRun();
    Q_INVOKABLE bool checkRun();
    Q_INVOKABLE void keepCode(const QString &text);

    // Flashcards: every multiple-choice and number question of the whole
    // course, drilled on its own schedule. Separate from working through a
    // lesson, because the two have different purposes -- a lesson explains,
    // a card only asks.
    QVariantMap cardStats() const;
    QVariantMap card() const;
    bool cardsDone() const { return m_cardQueue.isEmpty() && m_cardStarted; }
    Q_INVOKABLE void startCards();
    Q_INVOKABLE bool answerCard(int chosen);
    Q_INVOKABLE bool answerCardNumber(double value);
    Q_INVOKABLE void nextCard();

signals:
    void changed();
    void placementChanged();
    void lessonChanged();
    void runChanged();
    void cardChanged();

private slots:
    void onRunFinished();

private:
    void resetExercise();
    QStringList allCards() const;
    QVariantMap taskOfCard(const QString &id, QVariantMap *lessonOut = nullptr) const;
    void record(bool right);
    QVariantMap currentTask() const;

    Curriculum *m_course;
    Plotter *m_plotter;
    Runner *m_runner;
    Engine m_engine;
    Placement m_placement;

    QVariantMap m_placementResult;

    QStringList m_cardQueue;
    QString m_cardId;
    bool m_cardStarted = false;
    bool m_cardChecked = false;
    bool m_cardRight = false;
    int m_cardDone = 0;

    QString m_lessonId;
    int m_index = -1;             // -1 = the lesson text itself
    int m_right = 0;
    int m_answered = 0;
    bool m_checked = false;
    bool m_wasRight = false;
    bool m_showSolution = false;
    // Wie oft diese Aufgabe schon geprueft wurde, und was zuletzt gewaehlt
    // wurde. Beides braucht die Oberflaeche: Nach dem zweiten Fehlversuch
    // wird die Loesung von selbst gezeigt, und bei der Auswahlaufgabe soll
    // die eigene falsche Wahl zu sehen sein, ohne die richtige zu verraten.
    int m_attempts = 0;
    int m_chosen = -1;

    QString m_output;
    QString m_error;
    int m_errorLine = 0;
    int m_plotRevision = 0;
    bool m_hasPlot = false;
    double m_seconds = 0.0;
};

#endif
