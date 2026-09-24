#include "Course.h"
#include "Checker.h"
#include "Plotter.h"
#include "Runner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace {

// The shuffle for a Parsons exercise must be the same every time the same
// exercise is opened -- a different order on every redraw would make the
// screen jump under the learner's finger.
int stableSeed(const QString &lessonId, int index)
{
    int seed = index * 7919;
    for (int i = 0; i < lessonId.size(); ++i)
        seed = seed * 31 + lessonId.at(i).unicode();
    return qAbs(seed);
}

QVariantList shuffled(const QVariantList &input, int seed)
{
    QVariantList out = input;
    // A tiny linear congruential generator, so the order does not depend on
    // which qrand() other code has been calling.
    unsigned int state = unsigned(seed) + 1u;
    for (int i = out.size() - 1; i > 0; --i) {
        state = state * 1103515245u + 12345u;
        const int j = int((state >> 16) % unsigned(i + 1));
        out.swap(i, j);
    }
    return out;
}

}

void Course::setLanguage(const QString &code)
{
    if (code == m_course->language())
        return;
    m_course->setLanguage(code);
    m_engine.rememberLanguage(code);
    emit changed();
    emit lessonChanged();
    emit placementChanged();
}

Course::Course(Curriculum *course, Plotter *plotter, QObject *parent)
    : QObject(parent)
    , m_course(course)
    , m_plotter(plotter)
    , m_runner(new Runner(this))
    , m_engine(course)
{
    m_engine.load();
    // Die Sprache aus dem Fortschritt zurueckholen - sonst faengt jeder Start
    // wieder auf Deutsch an, auch wenn der Leser umgestellt hat.
    const QString gemerkt = m_engine.savedLanguage();
    if (!gemerkt.isEmpty() && m_course->languages().contains(gemerkt))
        m_course->setLanguage(gemerkt);

    // Beside the app binary, both in <root>/bin.
    const QString binDir = QFileInfo(QCoreApplication::applicationFilePath())
            .absolutePath();
    m_runner->setInterpreter(QLatin1String("c"), binDir + QLatin1String("/crun"),
                             QLatin1String(".c"));
    // C++ cannot be interpreted, so crunxx compiles and runs in one step. It
    // needs the g++ 4.4 tree from the c-lehrer-cpp package; without it the
    // C++ lessons stay readable but do not offer to run.
    const QString crunxx = binDir + QLatin1String("/crunxx");
    if (QFile::exists(crunxx)
            && QFile::exists(binDir + QLatin1String("/../toolchain/usr/bin/g++-4.4"))) {
        m_runner->setInterpreter(QLatin1String("cpp"), crunxx, QLatin1String(".cpp"));
    }
    // Rust: ein eigener Deuter fuer den Ausschnitt, den der Kurs lehrt.
    // rustc gibt es auf diesem Geraet nicht und wird es nicht geben --
    // warum, steht im Kopf von rust/rrun.cpp.
    const QString rrun = binDir + QLatin1String("/rrun");
    if (QFile::exists(rrun))
        m_runner->setInterpreter(QLatin1String("rust"), rrun, QLatin1String(".rs"));

    // The newest Python on the device; the plain /usr/bin/python is 2.6 and
    // has neither the syntax nor numpy.
    const QStringList pythons = QStringList()
            << QLatin1String("/opt/wunderw/bin/python3.11")
            << QLatin1String("/opt/wunderw/bin/python3")
            << QLatin1String("/usr/local/bin/python3")
            << QLatin1String("/usr/bin/python3");
    for (const QString &candidate : pythons) {
        if (QFile::exists(candidate)) {
            m_runner->setInterpreter(QLatin1String("python"), candidate,
                                     QLatin1String(".py"));
            break;
        }
    }
    m_runner->setWorkingDirectory(QDir::homePath() + QLatin1String("/.cache/c-lehrer"));
    connect(m_runner, SIGNAL(finished()), this, SLOT(onRunFinished()));
}

QVariantMap Course::stats() const
{
    QVariantMap out = m_engine.stats();
    out.insert(QLatin1String("planStufen"), m_course->plan().size());
    return out;
}

QVariantList Course::chapters() const
{
    QVariantList out;
    for (const QVariant &chapterValue : m_course->chapters()) {
        QVariantMap chapter = chapterValue.toMap();
        QVariantList lessons;
        for (const QVariant &lessonValue : chapter.value(QLatin1String("lektionen")).toList()) {
            const QVariantMap source = lessonValue.toMap();
            QVariantMap lesson;
            lesson.insert(QLatin1String("id"), source.value(QLatin1String("id")));
            lesson.insert(QLatin1String("titel"),
                          m_course->text(source.value(QLatin1String("titel"))));
            lesson.insert(QLatin1String("aufgaben"),
                          source.value(QLatin1String("aufgaben")).toList().size());
            lesson.insert(QLatin1String("stand"),
                          m_engine.lessonState(source.value(QLatin1String("id")).toString()));
            lessons.append(lesson);
        }
        chapter.insert(QLatin1String("titel"),
                       m_course->text(chapter.value(QLatin1String("titel"))));
        chapter.insert(QLatin1String("text"),
                       m_course->text(chapter.value(QLatin1String("text"))));
        chapter.insert(QLatin1String("lektionen"), lessons);
        out.append(chapter);
    }
    return out;
}

QVariantMap Course::nextLesson() const
{
    const QVariantMap pair = m_engine.nextLesson();
    QVariantMap out;
    if (pair.isEmpty()) {
        out.insert(QLatin1String("leer"), true);
        return out;
    }
    const QVariantMap chapter = pair.value(QLatin1String("kapitel")).toMap();
    const QVariantMap lesson = pair.value(QLatin1String("lektion")).toMap();
    out.insert(QLatin1String("leer"), false);
    out.insert(QLatin1String("id"), lesson.value(QLatin1String("id")));
    out.insert(QLatin1String("titel"),
               m_course->text(lesson.value(QLatin1String("titel"))));
    out.insert(QLatin1String("kapitel"),
               m_course->text(chapter.value(QLatin1String("titel"))));
    out.insert(QLatin1String("stufe"), chapter.value(QLatin1String("stufe")));
    return out;
}

// -- placement --------------------------------------------------------------

void Course::startPlacement()
{
    m_placementResult.clear();
    m_placement.begin(m_course->placementItems(), m_course->topics());
    m_placement.nextItem();
    emit placementChanged();
}

QVariantMap Course::placementQuestion() const
{
    QVariantMap out;
    if (!m_placement.isRunning()) {
        out.insert(QLatin1String("leer"), true);
        return out;
    }
    const QVariantMap item = m_placement.current();
    out.insert(QLatin1String("leer"), false);
    out.insert(QLatin1String("frage"),
               m_course->text(item.value(QLatin1String("frage"))));
    out.insert(QLatin1String("code"), item.value(QLatin1String("code")));
    out.insert(QLatin1String("bild"), item.value(QLatin1String("bild")));
    out.insert(QLatin1String("optionen"),
               m_course->textList(item.value(QLatin1String("optionen"))));
    out.insert(QLatin1String("nummer"), m_placement.asked() + 1);
    return out;
}

/* Plan und Einstufungsrueckblick stehen zweisprachig im Kurs.  Wer sie roh an
   die Oberflaeche gibt, zeigt dem englischen Leser Deutsch - uebersetzt wird
   erst beim Anzeigen, dann stimmt es auch nach einem Sprachwechsel. */
QVariantList Course::plan() const
{
    QVariantList out;
    for (const QVariant &value : m_course->plan()) {
        QVariantMap entry = value.toMap();
        entry.insert(QLatin1String("titel"),
                     m_course->text(entry.value(QLatin1String("titel"))));
        out.append(entry);
    }
    return out;
}

QVariantList Course::placementReview() const
{
    QVariantList out;
    for (const QVariant &value : m_engine.placementReview()) {
        QVariantMap entry = value.toMap();
        static const char *const texte[] = { "frage", "warum", "thema", "code" };
        for (size_t i = 0; i < sizeof(texte) / sizeof(texte[0]); ++i) {
            const QString key = QLatin1String(texte[i]);
            if (entry.contains(key))
                entry.insert(key, m_course->text(entry.value(key)));
        }
        if (entry.contains(QLatin1String("optionen")))
            entry.insert(QLatin1String("optionen"),
                         m_course->textList(entry.value(QLatin1String("optionen"))));
        out.append(entry);
    }
    return out;
}

void Course::answerPlacement(int chosen)
{
    if (!m_placement.isRunning())
        return;
    m_placement.answer(chosen);
    // One tap answers and moves straight on: a test that needs a second tap
    // per question feels twice as long as it is.
    if (m_placement.nextItem().isEmpty()) {
        QVariantMap result = m_placement.result();
        m_engine.applyPlacement(result);
        const QVariantMap next = m_engine.nextLesson();
        const QVariantMap chapter = next.value(QLatin1String("kapitel")).toMap();
        const QVariantMap lesson = next.value(QLatin1String("lektion")).toMap();
        result.insert(QLatin1String("leer"), false);
        result.insert(QLatin1String("schwach"), weakTopics());
        result.insert(QLatin1String("weiterKapitel"), chapter.value(QLatin1String("titel")));
        result.insert(QLatin1String("weiterLektion"), lesson.value(QLatin1String("titel")));
        result.insert(QLatin1String("weiterId"), lesson.value(QLatin1String("id")));
        // True when the learner placed above everything that is written yet.
        result.insert(QLatin1String("ueberStoff"),
                      !chapter.isEmpty()
                      && chapter.value(QLatin1String("stufe")).toInt()
                         < result.value(QLatin1String("level")).toInt());
        m_placementResult = result;
        emit changed();
    }
    emit placementChanged();
}

// -- lessons ----------------------------------------------------------------

void Course::startLesson(const QString &lessonId)
{
    m_lessonId = lessonId;
    m_index = -1;
    m_right = 0;
    m_answered = 0;
    resetExercise();
    emit lessonChanged();
}

void Course::resetExercise()
{
    m_checked = false;
    m_wasRight = false;
    m_showSolution = false;
    m_attempts = 0;
    m_chosen = -1;
    m_output.clear();
    m_error.clear();
    m_errorLine = 0;
    m_hasPlot = false;
    m_seconds = 0.0;
    emit runChanged();
}

QVariantMap Course::lesson() const
{
    QVariantMap out;
    const QVariantMap source = m_course->lesson(m_lessonId);
    if (source.isEmpty()) {
        out.insert(QLatin1String("leer"), true);
        return out;
    }
    const QVariantMap chapter = m_course->chapterOf(m_lessonId);
    out.insert(QLatin1String("leer"), false);
    out.insert(QLatin1String("id"), source.value(QLatin1String("id")));
    out.insert(QLatin1String("titel"),
               m_course->text(source.value(QLatin1String("titel"))));
    out.insert(QLatin1String("kapitel"),
               m_course->text(chapter.value(QLatin1String("titel"))));
    out.insert(QLatin1String("stufe"), chapter.value(QLatin1String("stufe")));
    out.insert(QLatin1String("text"),
               m_course->text(source.value(QLatin1String("text"))));
    out.insert(QLatin1String("bild"), source.value(QLatin1String("bild")));

    /* Die Formeln einer Lektion: die Codezeile, wie sie im Beispiel steht,
       und daneben dieselbe Sache gesetzt.  Code, TeX und Bildname sind in
       jeder Sprache gleich; nur der Untertitel steht zweisprachig im Kurs
       und muss hier uebersetzt werden, sonst liest der englische Leser
       Deutsch (oder QML zeigt "[object Object]"). */
    QVariantList formeln;
    for (const QVariant &value : source.value(QLatin1String("formeln")).toList()) {
        QVariantMap eintrag = value.toMap();
        eintrag.insert(QLatin1String("untertitel"),
                       m_course->text(eintrag.value(QLatin1String("untertitel"))));
        eintrag.insert(QLatin1String("erklaerung"),
                       m_course->text(eintrag.value(QLatin1String("erklaerung"))));
        formeln.append(eintrag);
    }
    out.insert(QLatin1String("formeln"), formeln);
    out.insert(QLatin1String("beispiel"), source.value(QLatin1String("beispiel")));
    out.insert(QLatin1String("ausgabe"), source.value(QLatin1String("ausgabe")));
    out.insert(QLatin1String("aufgaben"),
               source.value(QLatin1String("aufgaben")).toList().size());
    out.insert(QLatin1String("index"), m_index);
    out.insert(QLatin1String("sprache"), lessonLanguage());
    out.insert(QLatin1String("laeuft"), lessonRunnable());
    return out;
}

QVariantMap Course::currentTask() const
{
    if (m_lessonId.isEmpty() || m_index < 0)
        return QVariantMap();
    const QVariantList tasks = m_course->lesson(m_lessonId)
            .value(QLatin1String("aufgaben")).toList();
    if (m_index >= tasks.size())
        return QVariantMap();
    return tasks.at(m_index).toMap();
}

QVariantMap Course::exercise() const
{
    QVariantMap out;
    const QVariantMap task = currentTask();
    if (task.isEmpty()) {
        out.insert(QLatin1String("leer"), true);
        const QVariantList tasks = m_course->lesson(m_lessonId)
                .value(QLatin1String("aufgaben")).toList();
        out.insert(QLatin1String("fertig"), m_index >= tasks.size() && m_index > 0);
        out.insert(QLatin1String("richtigGesamt"), m_right);
        out.insert(QLatin1String("beantwortet"), m_answered);
        return out;
    }

    const QString kind = task.value(QLatin1String("kind")).toString();
    const QVariantList tasks = m_course->lesson(m_lessonId)
            .value(QLatin1String("aufgaben")).toList();

    out.insert(QLatin1String("leer"), false);
    out.insert(QLatin1String("art"), kind);
    out.insert(QLatin1String("frage"),
               m_course->text(task.value(QLatin1String("q"))));
    out.insert(QLatin1String("warum"),
               m_course->text(task.value(QLatin1String("warum")).isValid()
                              ? task.value(QLatin1String("warum"))
                              : task.value(QLatin1String("why"))));
    out.insert(QLatin1String("bild"), task.value(QLatin1String("bild")));
    out.insert(QLatin1String("nummer"), m_index + 1);
    out.insert(QLatin1String("gesamt"), tasks.size());
    out.insert(QLatin1String("geprueft"), m_checked);
    out.insert(QLatin1String("richtig"), m_wasRight);
    out.insert(QLatin1String("loesungZeigen"), m_showSolution);
    out.insert(QLatin1String("versuche"), m_attempts);
    out.insert(QLatin1String("gewaehlt"), m_chosen);

    if (kind == QLatin1String("mc")) {
        out.insert(QLatin1String("code"), task.value(QLatin1String("code")));
        out.insert(QLatin1String("optionen"),
                   m_course->textList(task.value(QLatin1String("options"))));
        out.insert(QLatin1String("antwort"), task.value(QLatin1String("answer")));
    } else if (kind == QLatin1String("predict")) {
        out.insert(QLatin1String("code"), task.value(QLatin1String("code")));
        out.insert(QLatin1String("antwort"), task.value(QLatin1String("answer")));
    } else if (kind == QLatin1String("blank")) {
        out.insert(QLatin1String("code"), task.value(QLatin1String("code")));
        out.insert(QLatin1String("antworten"), task.value(QLatin1String("answers")));
        out.insert(QLatin1String("luecken"),
                   task.value(QLatin1String("answers")).toList().size());
    } else if (kind == QLatin1String("parsons")) {
        const QVariantList lines = task.value(QLatin1String("lines")).toList();
        QVariantList pool = lines;
        pool += task.value(QLatin1String("distractors")).toList();
        out.insert(QLatin1String("zeilen"), lines);
        out.insert(QLatin1String("gemischt"),
                   shuffled(pool, stableSeed(m_lessonId, m_index)));
    } else if (kind == QLatin1String("zahl")) {
        // A number to work out -- cloud base, glide ratio, time. Compared
        // with a tolerance, because nobody does mental arithmetic to four
        // digits and being right must not depend on rounding.
        out.insert(QLatin1String("einheit"),
                   m_course->text(task.value(QLatin1String("einheit"))));
        out.insert(QLatin1String("antwort"), task.value(QLatin1String("antwort")));
        out.insert(QLatin1String("toleranz"),
                   task.value(QLatin1String("toleranz"), 0.1));
    } else if (kind == QLatin1String("code")) {
        const QString kept = m_engine.solution(m_lessonId, m_index);
        out.insert(QLatin1String("vorlage"),
                   kept.isEmpty() ? task.value(QLatin1String("starter")).toString() : kept);
        out.insert(QLatin1String("erwartet"), task.value(QLatin1String("expect")));
        out.insert(QLatin1String("loesung"), task.value(QLatin1String("solution")));
        out.insert(QLatin1String("zeichnet"), task.value(QLatin1String("plot")));
        out.insert(QLatin1String("sekunden"),
                   task.value(QLatin1String("seconds"), 10));
    }
    return out;
}

void Course::toExercises()
{
    m_index = 0;
    resetExercise();
    emit lessonChanged();
}

void Course::nextExercise()
{
    ++m_index;
    resetExercise();
    const QVariantList tasks = m_course->lesson(m_lessonId)
            .value(QLatin1String("aufgaben")).toList();
    if (m_index >= tasks.size() && !m_lessonId.isEmpty()) {
        m_engine.finishLesson(m_lessonId, m_right, qMax(1, m_answered));
        emit changed();
    }
    emit lessonChanged();
}

void Course::showSolution()
{
    m_showSolution = true;
    emit lessonChanged();
}

void Course::record(bool right)
{
    if (!m_checked) {
        ++m_answered;
        if (right)
            ++m_right;
    }
    ++m_attempts;
    m_checked = true;
    m_wasRight = right;
    // Nach dem zweiten Fehlversuch steht die Loesung da, ohne dass jemand
    // danach fragen muss.
    //
    // Warum nicht schon nach dem ersten: Wer die richtige Antwort sofort
    // liest, hoert auf nachzudenken -- der zweite Anlauf ist die Stelle, an
    // der aus einem Fehler etwas wird. Und warum nicht erst auf Nachfrage:
    // Wer sich gerade geirrt hat, fragt am seltensten nach, und eine
    // Korrektur, die niemand liest, wirkt nicht.
    if (!right && m_attempts >= 2)
        m_showSolution = true;
    emit lessonChanged();
}

void Course::retryExercise()
{
    m_checked = false;
    m_wasRight = false;
    m_chosen = -1;
    emit lessonChanged();
}

bool Course::answerChoice(int chosen)
{
    const QVariantMap task = currentTask();
    const bool right = (chosen == task.value(QLatin1String("answer")).toInt());
    m_chosen = chosen;
    record(right);
    return right;
}

bool Course::answerText(const QString &text)
{
    const QVariantMap task = currentTask();
    const bool right = Checker::outputMatches(
        text, task.value(QLatin1String("answer")).toString());
    record(right);
    return right;
}

bool Course::answerBlanks(const QVariantList &values)
{
    const QVariantMap task = currentTask();
    const QVariantList wanted = task.value(QLatin1String("answers")).toList();
    const QVariantList alternatives = task.value(QLatin1String("alts")).toList();
    bool right = (values.size() == wanted.size());
    for (int i = 0; right && i < values.size(); ++i) {
        QStringList alts;
        if (i < alternatives.size()) {
            for (const QVariant &alt : alternatives.at(i).toList())
                alts << alt.toString();
        }
        if (!Checker::textMatches(values.at(i).toString(),
                                  wanted.at(i).toString(), alts))
            right = false;
    }
    record(right);
    return right;
}

bool Course::answerNumber(double value)
{
    const QVariantMap task = currentTask();
    const double want = task.value(QLatin1String("antwort")).toDouble();
    const double tol = task.value(QLatin1String("toleranz"), 0.1).toDouble();
    const bool right = qAbs(value - want) <= qAbs(want) * tol + 1e-9;
    record(right);
    return right;
}

bool Course::answerParsons(const QVariantList &lines)
{
    const QVariantMap task = currentTask();
    const QVariantList wanted = task.value(QLatin1String("lines")).toList();
    bool right = (lines.size() == wanted.size());
    for (int i = 0; right && i < lines.size(); ++i) {
        if (lines.at(i).toString().trimmed() != wanted.at(i).toString().trimmed())
            right = false;
    }
    record(right);
    return right;
}

// -- running ----------------------------------------------------------------

bool Course::running() const
{
    return m_runner->isRunning();
}

QString Course::lessonLanguage() const
{
    const QVariantMap chapter = m_course->chapterOf(m_lessonId);
    const QString lang = chapter.value(QLatin1String("sprache")).toString();
    return lang.isEmpty() ? QLatin1String("c") : lang;
}

bool Course::lessonRunnable() const
{
    return m_runner->canRun(lessonLanguage());
}

bool Course::canRunLanguage(const QString &language) const
{
    return m_runner->canRun(language);
}

void Course::runCode(const QString &code, int seconds, const QString &language)
{
    if (m_runner->isRunning())
        return;
    const QString sprache = language.isEmpty() ? lessonLanguage() : language;
    m_output.clear();
    m_error.clear();
    m_hasPlot = false;
    emit runChanged();
    int frist = seconds > 0 ? seconds : 10;
    // Bei C++ geht die Zeit fast ganz fuers Uebersetzen drauf: GCC 4.4 kaut
    // auf diesem Geraet an einem <iostream> laenger als das fertige Programm
    // je laufen wird. Die im Kurs hinterlegte Frist meint aber die Laufzeit,
    // also kommt der Uebersetzungslauf oben drauf statt sie aufzuzehren.
    if (sprache == QLatin1String("cpp"))
        frist += 45;
    m_runner->start(code, frist, sprache);
    emit runChanged();
}

void Course::stopRun()
{
    m_runner->stop();
}

void Course::onRunFinished()
{
    const RunResult result = m_runner->result();
    m_output = result.output;
    m_error = result.error;
    m_errorLine = result.errorLine;
    m_seconds = result.seconds;
    m_hasPlot = !result.curves.isEmpty();
    if (m_hasPlot) {
        m_plotter->setCurves(result.curves);
        m_plotRevision = m_plotter->revision();
    }
    emit runChanged();
}

bool Course::checkRun()
{
    const QVariantMap task = currentTask();
    const bool right = Checker::outputMatches(
        m_output, task.value(QLatin1String("expect")).toString());
    record(right);
    return right;
}

// -- flashcards -------------------------------------------------------------

namespace {

QString cardId(const QString &lessonId, int index)
{
    return lessonId + QLatin1Char('#') + QString::number(index);
}

// Only the question types that work as a card. A lesson also holds things
// to assemble, predict and write -- those want the lesson, not a card.
bool isCardKind(const QString &kind)
{
    return kind == QLatin1String("mc") || kind == QLatin1String("zahl");
}

}

QStringList Course::allCards() const
{
    QStringList out;
    for (const QVariant &pairValue : m_course->allLessons()) {
        const QVariantMap lesson = pairValue.toMap().value(QLatin1String("lektion")).toMap();
        const QString id = lesson.value(QLatin1String("id")).toString();
        const QVariantList tasks = lesson.value(QLatin1String("aufgaben")).toList();
        for (int i = 0; i < tasks.size(); ++i) {
            if (isCardKind(tasks.at(i).toMap().value(QLatin1String("kind")).toString()))
                out << cardId(id, i);
        }
    }
    return out;
}

QVariantMap Course::taskOfCard(const QString &id, QVariantMap *lessonOut) const
{
    const int hash = id.lastIndexOf(QLatin1Char('#'));
    if (hash < 0)
        return QVariantMap();
    const QString lessonId = id.left(hash);
    const int index = id.mid(hash + 1).toInt();
    const QVariantMap lesson = m_course->lesson(lessonId);
    const QVariantList tasks = lesson.value(QLatin1String("aufgaben")).toList();
    if (index < 0 || index >= tasks.size())
        return QVariantMap();
    if (lessonOut)
        *lessonOut = lesson;
    return tasks.at(index).toMap();
}

QVariantMap Course::cardStats() const
{
    return m_engine.cardStats(allCards());
}

void Course::startCards()
{
    m_cardQueue = m_engine.dueCards(allCards());
    m_cardStarted = true;
    m_cardDone = 0;
    m_cardChecked = false;
    m_cardRight = false;
    m_cardId = m_cardQueue.isEmpty() ? QString() : m_cardQueue.first();
    emit cardChanged();
}

QVariantMap Course::card() const
{
    QVariantMap out;
    if (m_cardId.isEmpty()) {
        out.insert(QLatin1String("leer"), true);
        out.insert(QLatin1String("erledigt"), m_cardDone);
        return out;
    }
    QVariantMap lesson;
    const QVariantMap task = taskOfCard(m_cardId, &lesson);
    if (task.isEmpty()) {
        out.insert(QLatin1String("leer"), true);
        return out;
    }
    const QVariantMap chapter = m_course->chapterOf(
        lesson.value(QLatin1String("id")).toString());

    out.insert(QLatin1String("leer"), false);
    out.insert(QLatin1String("art"), task.value(QLatin1String("kind")));
    out.insert(QLatin1String("frage"), m_course->text(task.value(QLatin1String("q"))));
    out.insert(QLatin1String("code"), task.value(QLatin1String("code")));
    out.insert(QLatin1String("bild"), task.value(QLatin1String("bild")));
    out.insert(QLatin1String("optionen"),
               m_course->textList(task.value(QLatin1String("options"))));
    out.insert(QLatin1String("antwort"),
               task.value(QLatin1String("answer")).isValid()
               ? task.value(QLatin1String("answer"))
               : task.value(QLatin1String("antwort")));
    out.insert(QLatin1String("einheit"),
               m_course->text(task.value(QLatin1String("einheit"))));
    out.insert(QLatin1String("warum"),
               m_course->text(task.value(QLatin1String("warum")).isValid()
                              ? task.value(QLatin1String("warum"))
                              : task.value(QLatin1String("why"))));
    out.insert(QLatin1String("kapitel"),
               m_course->text(chapter.value(QLatin1String("titel"))));
    out.insert(QLatin1String("geprueft"), m_cardChecked);
    out.insert(QLatin1String("richtig"), m_cardRight);
    out.insert(QLatin1String("offen"), m_cardQueue.size());
    out.insert(QLatin1String("erledigt"), m_cardDone);

    const QVariantMap state = m_engine.cardState(m_cardId);
    out.insert(QLatin1String("neu"), state.isEmpty());
    out.insert(QLatin1String("tage"), state.value(QLatin1String("tage"), 0));
    return out;
}

bool Course::answerCard(int chosen)
{
    if (m_cardId.isEmpty() || m_cardChecked)
        return false;
    const QVariantMap task = taskOfCard(m_cardId);
    const bool right = (chosen == task.value(QLatin1String("answer")).toInt());
    m_cardChecked = true;
    m_cardRight = right;
    m_engine.scheduleCard(m_cardId, right);
    emit cardChanged();
    return right;
}

bool Course::answerCardNumber(double value)
{
    if (m_cardId.isEmpty() || m_cardChecked)
        return false;
    const QVariantMap task = taskOfCard(m_cardId);
    const double want = task.value(QLatin1String("antwort")).toDouble();
    const double tol = task.value(QLatin1String("toleranz"), 0.1).toDouble();
    const bool right = qAbs(value - want) <= qAbs(want) * tol + 1e-9;
    m_cardChecked = true;
    m_cardRight = right;
    m_engine.scheduleCard(m_cardId, right);
    emit cardChanged();
    return right;
}

void Course::nextCard()
{
    if (!m_cardQueue.isEmpty())
        m_cardQueue.removeFirst();
    if (m_cardChecked)
        ++m_cardDone;
    m_cardChecked = false;
    m_cardRight = false;
    m_cardId = m_cardQueue.isEmpty() ? QString() : m_cardQueue.first();
    emit cardChanged();
    emit changed();
}

void Course::keepCode(const QString &text)
{
    if (!m_lessonId.isEmpty() && m_index >= 0)
        m_engine.saveSolution(m_lessonId, m_index, text);
}
