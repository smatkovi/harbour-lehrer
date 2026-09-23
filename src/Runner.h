#ifndef RUNNER_H
#define RUNNER_H

#include <QObject>
#include <QMap>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QVector>

// Runs the learner's C program and brings back what it said.
//
// Three things matter and none of them are optional:
//
//  * A time limit. The first loop anyone writes wrong is an endless one,
//    and without a limit the app would simply hang with no way back.
//  * A separate process. A program can eat all memory or recurse until it
//    dies -- in a child that is a message, in the app it is a crash.
//  * An output limit. "for(;;) printf" fills the flash otherwise.
//
// Nothing here blocks: QProcess reports back through signals, so the page
// keeps drawing and the Stop button keeps working while a program runs.
// Our own point, deliberately not QPointF: on ARM Qt 4 builds qreal is
// float, so a QPointF would quietly round every simulation value to about
// seven digits before it is ever drawn or compared.
struct Point
{
    double x;
    double y;
};

struct Curve
{
    QString name;
    QVector<Point> points;
};

class RunResult
{
public:
    QString output;               // what it printed, plot lines removed
    QString error;                // the complaint, if any
    int errorLine = 0;
    QVector<Curve> curves;
    bool timedOut = false;
    double seconds = 0.0;
    bool ok() const { return error.isEmpty() && !timedOut; }
};

class Runner : public QObject
{
    Q_OBJECT
public:
    explicit Runner(QObject *parent = nullptr);

    // One runner, several languages. C goes to crun (picoc); Python goes
    // to whatever Python 3 the device carries -- on this phone that is
    // /opt/wunderw/bin/python3.11, which since the numpy build can do real
    // numerics. C++ has no interpreter here and is never started.
    void setInterpreter(const QString &language, const QString &path,
                        const QString &suffix);
    void setWorkingDirectory(const QString &path) { m_workDir = path; }
    bool canRun(const QString &language) const;

    void start(const QString &code, int seconds,
               const QString &language = QLatin1String("c"));
    void stop();
    bool isRunning() const;

    RunResult result() const { return m_result; }

signals:
    void finished();

private slots:
    void onFinished(int code, QProcess::ExitStatus status);
    void onError(QProcess::ProcessError error);
    void onTimeout();

private:
    void collect(int exitCode);

    struct Tool {
        QString path;
        QString suffix;
    };
    QMap<QString, Tool> m_tools;
    QString m_language;

    QProcess *m_process;
    // A member, not QTimer::singleShot: a fire-and-forget timer from an
    // earlier run stays armed after that run finishes, and then kills the
    // NEXT program mid-flight and reports it as a timeout. Held here so it
    // can be stopped the moment the process it belongs to is done.
    QTimer m_timer;
    QString m_workDir;
    RunResult m_result;
    qint64 m_started = 0;
    bool m_killed = false;
    int m_seconds = 10;
};

#endif
