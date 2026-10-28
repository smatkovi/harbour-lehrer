#ifndef COURSE_H
#define COURSE_H

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QImage>
#include <QVector>

#include "Curriculum.h"
#include "Engine.h"
#include "Erkennen.h"
#include "Klassifizierer.h"
#include "Nachlernen.h"
#include "OnlineDeuter.h"
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

    Q_PROPERTY(bool hasRecognition READ hasRecognition NOTIFY changed)
    Q_PROPERTY(QString recognitionTitle READ recognitionTitle NOTIFY changed)
    Q_PROPERTY(QString recognitionSubtitle READ recognitionSubtitle NOTIFY changed)
    Q_PROPERTY(int recognitionCount READ recognitionCount NOTIFY changed)
    Q_PROPERTY(QVariantMap recognitionQuestion READ recognitionQuestion NOTIFY recognitionChanged)
    Q_PROPERTY(bool recognitionDone READ recognitionDone NOTIFY recognitionChanged)
    Q_PROPERTY(QVariantMap recognitionResult READ recognitionResult NOTIFY recognitionChanged)

    /* Foto deuten: das eigene Netz (data/modell.bin) immer, Claude dazu,
       wenn ein Weg ins Netz und ein Schluessel da sind. */
    Q_PROPERTY(bool hasClassifier READ hasClassifier NOTIFY changed)
    Q_PROPERTY(QString classifierTitle READ classifierTitle NOTIFY changed)
    Q_PROPERTY(QString classifierSubtitle READ classifierSubtitle NOTIFY changed)
    Q_PROPERTY(QStringList classifierTips READ classifierTips NOTIFY changed)
    Q_PROPERTY(QVariantList classifierClasses READ classifierClasses NOTIFY changed)
    Q_PROPERTY(bool onlineAvailable READ onlineAvailable NOTIFY changed)
    Q_PROPERTY(bool hasApiKey READ hasApiKey NOTIFY photoChanged)
    Q_PROPERTY(QVariantMap photo READ photo NOTIFY photoChanged)

    /* Was die App dazugelernt hat (GELERNTES.md): {anzahl, nutzbare,
       wirksam, fehlende, eintraege:[{nummer, klasse, name, quelle, zeit,
       bekannt}]}.  Die Liste ist das Neueste zuerst. */
    Q_PROPERTY(QVariantMap learned READ learned NOTIFY learnedChanged)
    /* Auch ohne eigenen Tipp von Claude lernen: Dann merkt die App ein
       Foto von selbst, wenn Claude seine Sicherheit mit "hoch" angibt und
       der Leser nichts geraten hat (GELERNTES.md, "Was gemerkt wird"). */
    Q_PROPERTY(bool learnFromClaude READ learnFromClaude WRITE setLearnFromClaude NOTIFY learnedChanged)

    /* Das Glossar, uebersetzt und nach Wort sortiert: [{id, wort}].  Den
       ganzen Eintrag holt glossaryEntry(id). */
    Q_PROPERTY(bool hasGlossary READ hasGlossary NOTIFY changed)
    Q_PROPERTY(QVariantList glossary READ glossary NOTIFY changed)

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
    ~Course();

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

    bool hasRecognition() const { return m_course->hasRecognition(); }
    QString recognitionTitle() const { return m_course->recognitionTitle(); }
    QString recognitionSubtitle() const { return m_course->recognitionSubtitle(); }
    int recognitionCount() const { return m_course->recognitionPhotos().size(); }
    QVariantMap recognitionQuestion() const;
    bool recognitionDone() const { return m_erkennen.done(); }
    QVariantMap recognitionResult() const;

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

    // Der Erkennungsmodus. `wieviele` <= 0 heisst: alle Fotos.
    Q_INVOKABLE void startRecognition(int wieviele = 0);
    Q_INVOKABLE void answerRecognition(int chosen);
    Q_INVOKABLE void nextRecognition();

    /* Foto deuten.  loadClassifier() ruft main.cpp einmal mit dem Pfad
       der Modelldatei; ohne Datei oder ohne Klassentafel im Kurs bleibt
       hasClassifier falsch und die Taste weg.

       Ablauf auf der Seite: classifyPhoto(pfad) gibt das Foto in einen
       Arbeitsfaden (photo.laeuft), der Leser tippt derweil seinen eigenen
       Tipp (guessPhoto), und erst mit Tipp UND fertigem Netz zeigt die
       Seite photo.ergebnis -- die drei wahrscheinlichsten Klassen mit
       Anteil, Name, Bedeutung fuers Fliegen.  askOnline() holt dazu den
       zweiten Blick von Claude in photo.online; beide bleiben sichtbar.

       `photo`: leer, pfad, laeuft, fertig, fehler, geraten ("" = noch
       nicht, "?" = weiss nicht), geratenName, ergebnis [{id, name,
       anteil, prozent, bedeutung, sinn, erkennen, bedeutet}], treffer,
       online {laeuft, fertig, ok, klasse, name, sinn, bedeutet,
       sicherheit, begruendung, fehler, einig}. */
    void loadClassifier(const QString &pfad);
    bool hasClassifier() const;
    QString classifierTitle() const { return m_course->classifierTitle(); }
    QString classifierSubtitle() const { return m_course->classifierSubtitle(); }
    QStringList classifierTips() const { return m_course->classifierTips(); }
    QVariantList classifierClasses() const;
    bool onlineAvailable() const;
    bool hasApiKey() const { return !m_schluessel.isEmpty(); }
    QVariantMap photo() const;
    Q_INVOKABLE void classifyPhoto(const QString &pfad);
    Q_INVOKABLE void guessPhoto(const QString &klasseId);
    Q_INVOKABLE void askOnline();
    Q_INVOKABLE void setApiKey(const QString &schluessel);
    Q_INVOKABLE void clearPhoto();
    /* Gibt es den Ordner?  Die eigene Galerie auf Harmattan zeigt nur die
       Ordner als Umschalter an, die es auf diesem Geraet wirklich gibt. */
    Q_INVOKABLE bool pathExists(const QString &pfad) const;

    /* Merken: `klasseId` ist die Klasse, die gelernt werden soll, `quelle`
       0 = beide einig, 1 = Claude allein, 2 = der Leser stellt richtig.
       Ohne Foto oder ohne Abdruck tut es nichts.  Nach einer Antwort von
       Claude, die mit dem eigenen Tipp uebereinstimmt, merkt askOnline das
       von selbst -- die Taste ist fuer die Faelle, in denen die beiden
       auseinandergehen. */
    Q_INVOKABLE void rememberPhoto(const QString &klasseId, int quelle);
    bool learnFromClaude() const;
    void setLearnFromClaude(bool an);
    Q_INVOKABLE void forgetLearned(int nummer);
    Q_INVOKABLE void forgetAllLearned();
    /* Das gemerkte Bild als Datei im Zwischenspeicher, fuer die Anzeige;
       leer, wenn es die Nummer nicht gibt. */
    Q_INVOKABLE QString learnedImage(int nummer);
    /* Sichern und Laden: die Datei ist uebertragbar, und Laden fuehrt
       zusammen statt zu ersetzen.  Rueckgabe ist ein Satz fuer die
       Oberflaeche -- was passiert ist oder was schiefging. */
    Q_INVOKABLE QString exportLearned();
    Q_INVOKABLE QString importLearned(const QString &pfad);
    Q_INVOKABLE QString learnedFilePath() const;

    /* Ein Glossareintrag: {leer, id, wort, definition, bild, siehe:[{id,
       wort}]}; definition im selben Markdown wie die Lektionstexte
       (Style.rich in QML setzt es). */
    bool hasGlossary() const { return m_course->hasGlossary(); }
    QVariantList glossary() const;
    Q_INVOKABLE QVariantMap glossaryEntry(const QString &id) const;

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
    bool numpyDa() const;
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
    void recognitionChanged();
    void photoChanged();
    void learnedChanged();
    void lessonChanged();
    void runChanged();
    void cardChanged();

private slots:
    void onRunFinished();
    void onlineStarten();
    void onKopfNachtrainiert();
    void onPhotoClassified();
    void onOnlineDone(const QVariantMap &antwort);

private:
    void resetExercise();
    QStringList allCards() const;
    QVariantMap taskOfCard(const QString &id, QVariantMap *lessonOut = nullptr) const;
    void record(bool right);
    QVariantMap currentTask() const;

    Curriculum *m_course;
    Plotter *m_plotter;
    Runner *m_runner;
    mutable int m_numpy = -1;      // -1 = noch nicht gefragt
    Engine m_engine;
    Placement m_placement;
    Erkennen m_erkennen;

    QVariantMap classEntry(const QString &id) const;
    QString schluesselPfad() const;
    QString zweisprachig(const char *de, const char *en) const;

    Klassifizierer m_klassifizierer;
    QString m_modellPfad;          // geladen wird erst beim ersten Foto
    Nachlernen m_gelerntes;
    bool m_gelerntesGeladen = false;
    QImage m_fotoNetzBild;         // das Bild, wie das Netz es sah
    QVector<float> m_fotoAbdruck;  // sein Merkmalsvektor
    bool m_fotoKorrigiert = false; // hat das Gelernte die Reihenfolge geaendert?
    QString m_fotoGemerkt;         // was zu diesem Foto gemerkt wurde
    bool m_vonClaudeLernen = false; // auch ohne eigenen Tipp merken (Datei im Datenordner)
    int m_fotoDrehung = 0;         // EXIF-Drehung in Grad, fuer die Anzeige
    QString m_fotoWartend;         // Foto, das auf den laufenden Faden wartet
    bool m_fadenLaeuft = false;    // rechnet gerade ein BewertungsFaden?
    bool m_onlineWartend = false;  // Anfrage, die im naechsten Umlauf startet

    QVariantMap learned() const;
    void gelerntesLaden();
    QString gelerntesPfad() const;
    QString vonClaudePfad() const;
    void gelerntesGeaendert();     // speichern, melden, Kopf nachtrainieren
    void kopfNachtrainieren();     // im Arbeitsfaden, wenn es sich lohnt
    bool m_lernFadenLaeuft = false;
    float m_fotoLaenge = 0.f;      // Laenge des Abdrucks vor dem Normieren
    OnlineDeuter *m_online;
    int m_fotoLauf = 0;            // zaehlt die Fotos, damit ein spaeter
                                   // fertiger Faden kein neueres ueberschreibt
    QString m_fotoPfad;
    bool m_fotoLaeuft = false;
    bool m_fotoFertig = false;
    QString m_fotoFehler;          // "" | "laden" | "rechnen"
    QString m_fotoGeraten;
    QVector<float> m_fotoAnteile;
    QVariantMap m_fotoOnline;
    QString m_schluessel;

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
