#include "Runner.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

namespace {

const int MaxOutput = 64 * 1024;

// picoc reports "file:line:col message".
QRegExp errorPattern()
{
    return QRegExp(QLatin1String("^[^:]*:(\\d+):(\\d+)\\s+(.*)$"));
}

}

Runner::Runner(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_timer.setSingleShot(true);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    connect(m_process, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(onFinished(int, QProcess::ExitStatus)));
    connect(m_process, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(onError(QProcess::ProcessError)));
}

void Runner::setInterpreter(const QString &language, const QString &path,
                            const QString &suffix)
{
    Tool tool;
    tool.path = path;
    tool.suffix = suffix;
    m_tools.insert(language, tool);
}

bool Runner::canRun(const QString &language) const
{
    return m_tools.contains(language)
            && QFile::exists(m_tools.value(language).path);
}

QString Runner::interpreterPath(const QString &language) const
{
    return m_tools.value(language).path;
}

bool Runner::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void Runner::start(const QString &code, int seconds, const QString &language)
{
    if (isRunning())
        return;

    m_result = RunResult();
    m_killed = false;
    m_seconds = seconds;
    m_language = language;

    if (!m_tools.contains(language)) {
        // Say what is missing, not just that something is. Nur C++ braucht
        // ein Zusatzpaket; C bringt picoc mit, Python liegt unter /opt.
        if (language == QLatin1String("cpp")) {
            m_result.error = QString::fromUtf8(
                "Für C++ fehlt der Compiler. Er steckt im Paket c-lehrer-cpp; "
                "ohne ihn lassen sich die C++-Lektionen lesen, aber nicht "
                "ausführen.");
        } else if (language == QLatin1String("rust")) {
            m_result.error = QString::fromUtf8(
                "Der Rust-Deuter rrun fehlt neben der App. Ohne ihn lassen "
                "sich die Rust-Lektionen lesen, aber nicht ausführen.");
        } else {
            m_result.error = QString::fromUtf8(
                "Für %1 gibt es auf diesem Gerät keinen Ausführer.").arg(language);
        }
        emit finished();
        return;
    }
    const Tool tool = m_tools.value(language);

    QDir().mkpath(m_workDir);
    const QString source = m_workDir + QLatin1String("/lauf") + tool.suffix;
    QFile file(source);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_result.error = QString::fromUtf8("Der Quelltext lässt sich nicht ablegen.");
        emit finished();
        return;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << code;
    file.close();

    if (!QFile::exists(tool.path)) {
        m_result.error = QString::fromUtf8("Der Ausführer fehlt: ") + tool.path;
        emit finished();
        return;
    }

    m_started = QDateTime::currentMSecsSinceEpoch();
    m_process->setWorkingDirectory(m_workDir);
    m_process->start(tool.path, QStringList() << source);
    m_timer.start(seconds * 1000);
}

void Runner::stop()
{
    m_timer.stop();
    if (isRunning()) {
        m_killed = true;
        m_process->kill();
    }
}

void Runner::onTimeout()
{
    if (!isRunning())
        return;
    m_killed = true;
    m_result.timedOut = true;
    m_process->kill();
}

void Runner::onError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        m_timer.stop();
        m_result.error = QString::fromUtf8("Der Ausführer lässt sich nicht starten.");
        emit finished();
    }
}

void Runner::onFinished(int code, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    m_timer.stop();
    collect(code);
    emit finished();
}

void Runner::collect(int exitCode)
{
    m_result.seconds = (QDateTime::currentMSecsSinceEpoch() - m_started) / 1000.0;

    QByteArray raw = m_process->readAll();
    if (raw.size() > MaxOutput) {
        raw.truncate(MaxOutput);
        raw += "\n... (Ausgabe abgeschnitten)";
    }
    const QString text = QString::fromUtf8(raw);

    // A program draws by printing. "plot <x> <y>" is one point of one curve,
    // "plot <name> <x> <y>" one point of a named curve, so a lesson can put
    // two integrators in the same picture.
    QStringList kept;
    QVector<Curve> curves;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QLatin1String("plot "))) {
            kept << line;
            continue;
        }
        const QStringList parts = trimmed.mid(5).split(QRegExp(QLatin1String("\\s+")),
                                                       QString::SkipEmptyParts);
        QString name;
        double x = 0.0, y = 0.0;
        bool okX = false, okY = false;
        if (parts.size() == 2) {
            x = parts.at(0).toDouble(&okX);
            y = parts.at(1).toDouble(&okY);
        } else if (parts.size() >= 3) {
            name = parts.at(0);
            x = parts.at(1).toDouble(&okX);
            y = parts.at(2).toDouble(&okY);
        }
        if (!okX || !okY) {
            kept << line;
            continue;
        }
        int index = -1;
        for (int i = 0; i < curves.size(); ++i) {
            if (curves.at(i).name == name) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            Curve curve;
            curve.name = name;
            curves.append(curve);
            index = curves.size() - 1;
        }
        curves[index].points.append(Point{x, y});
    }
    m_result.output = kept.join(QLatin1String("\n")).trimmed();
    m_result.curves = curves;

    if (m_result.timedOut) {
        m_result.error = QString::fromUtf8(
            "Das Programm lief länger als %1 Sekunden und wurde abgebrochen. "
            "Meist ist eine Schleife schuld, die nicht endet.").arg(m_seconds);
        return;
    }
    if (m_killed)
        return;

    if (exitCode != 0 && m_language == QLatin1String("python")) {
        // A traceback: the last line is the message, and the innermost
        // "File ..., line N" that refers to the learner's own file is the
        // place worth pointing at.
        const QStringList lines = text.trimmed().split(QLatin1Char('\n'));
        QRegExp where(QLatin1String("^\\s*File \"[^\"]*lauf\\.py\", line (\\d+)"));
        for (const QString &line : lines) {
            if (where.indexIn(line) >= 0)
                m_result.errorLine = where.cap(1).toInt();
        }
        for (int i = lines.size() - 1; i >= 0; --i) {
            if (!lines.at(i).trimmed().isEmpty()) {
                m_result.error = lines.at(i).trimmed();
                break;
            }
        }
        return;
    }

    if (exitCode != 0) {
        QRegExp pattern = errorPattern();
        const QStringList lines = text.trimmed().split(QLatin1Char('\n'));
        for (int i = lines.size() - 1; i >= 0; --i) {
            const QString line = lines.at(i).trimmed();
            if (line.isEmpty())
                continue;
            if (pattern.indexIn(line) >= 0) {
                m_result.errorLine = pattern.cap(1).toInt();
                m_result.error = pattern.cap(3);
            } else {
                m_result.error = line;
            }
            break;
        }
        if (m_result.error.isEmpty()) {
            m_result.error = QString::fromUtf8("Das Programm endete mit Fehler %1.")
                    .arg(exitCode);
        }
    }
}
