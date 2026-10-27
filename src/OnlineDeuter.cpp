#include "OnlineDeuter.h"
#include "Json.h"
#include "Klassifizierer.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

#if QT_VERSION >= 0x050000
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#else
#include <QProcess>
#include <QTemporaryFile>
#endif

/* Der gemeinsame Teil -- Bild packen, Anfrage bauen, Antwort deuten --
   kennt keinen Transport.  Beide Wege unten liefern am Ende nur ein
   Byte-Feld und einen HTTP-Status, und alles weitere passiert hier,
   damit Harmattan und Sailfish dieselbe Antwort lesen. */
namespace {

const char kModell[] = "claude-sonnet-5";
const char kVersion[] = "2023-06-01";
const int kAntwortTokens = 400;
const int kZeitlimitMs = 90 * 1000;

QString zweisprachig(const QString &sprache, const char *de, const char *en)
{
    return QString::fromUtf8(sprache == QLatin1String("en") ? en : de);
}

QString apiUrl()
{
    /* Nur der Pruefstand setzt die Variable, um auf einen Scheinserver zu
       zeigen; im Betrieb bleibt es die echte Adresse. */
    const QByteArray gesetzt = qgetenv("SEGELFLUG_API_URL");
    if (!gesetzt.isEmpty())
        return QString::fromLocal8Bit(gesetzt);
    return QLatin1String("https://api.anthropic.com/v1/messages");
}

/* Das Foto, verkleinert und als JPEG verpackt, in base64.  Leer, wenn es
   sich nicht laden laesst; dann steht in `fehler`, warum. */
QByteArray bildAlsBase64(const QString &pfad, QString *fehler)
{
    const QImage bild = Klassifizierer::bildLaden(pfad, 1024, fehler);
    if (bild.isNull())
        return QByteArray();
    QBuffer puffer;
    puffer.open(QIODevice::WriteOnly);
    if (!bild.save(&puffer, "JPEG", 85)) {
        if (fehler)
            *fehler = QLatin1String("JPEG");
        return QByteArray();
    }
    puffer.close();
    return puffer.data().toBase64();
}

QString systemText(const QVariantList &klassen, const QString &sprache)
{
    /* Englisch, weil das Modell damit am sichersten umgeht; nur die
       Begruendung soll in der Sprache des Lesers kommen. */
    const QString strich = QString::fromUtf8(" \xe2\x80\x94 ");  // der Gedankenstrich
    QString text = QLatin1String(
        "You look at a single sky photo for a glider pilot. Assign it to "
        "exactly one of these cloud classes (id");
    text += strich.trimmed() + QLatin1String(" name):\n");
    for (int i = 0; i < klassen.size(); ++i) {
        const QVariantMap k = klassen.at(i).toMap();
        text += k.value(QLatin1String("id")).toString();
        text += strich;
        text += k.value(QLatin1String("name")).toString();
        text += QLatin1Char('\n');
    }
    text += QLatin1String(
        "If none of them fits, use an empty string as the id.\n"
        "Reply with JSON only, no prose before or after, exactly in this form:\n"
        "{\"klasse\": \"<id>\", \"sicherheit\": \"hoch|mittel|niedrig\", "
        "\"begruendung\": \"<one or two sentences in ");
    text += sprache == QLatin1String("en") ? QLatin1String("English")
                                           : QLatin1String("German");
    text += QLatin1String(
        ": what gives the class away in this photo, and what it means for "
        "gliding on this day>\"}");
    return text;
}

QVariantMap anfrageRumpf(const QByteArray &base64, const QVariantList &klassen,
                         const QString &sprache)
{
    QVariantMap quelle;
    quelle.insert(QLatin1String("type"), QLatin1String("base64"));
    quelle.insert(QLatin1String("media_type"), QLatin1String("image/jpeg"));
    quelle.insert(QLatin1String("data"), QString::fromLatin1(base64));
    QVariantMap bild;
    bild.insert(QLatin1String("type"), QLatin1String("image"));
    bild.insert(QLatin1String("source"), quelle);
    QVariantMap frage;
    frage.insert(QLatin1String("type"), QLatin1String("text"));
    frage.insert(QLatin1String("text"),
                 QLatin1String("Which class is this sky? JSON only."));
    QVariantList inhalt;
    inhalt << bild << frage;
    QVariantMap nachricht;
    nachricht.insert(QLatin1String("role"), QLatin1String("user"));
    nachricht.insert(QLatin1String("content"), inhalt);
    QVariantList nachrichten;
    nachrichten << nachricht;

    /* Ohne diese Zeile denkt das Modell erst lange nach, und das Nachdenken
       zaehlt gegen max_tokens -- bei 400 bliebe fuer das JSON nichts. */
    QVariantMap denken;
    denken.insert(QLatin1String("type"), QLatin1String("disabled"));

    QVariantMap rumpf;
    rumpf.insert(QLatin1String("model"), QLatin1String(kModell));
    rumpf.insert(QLatin1String("max_tokens"), kAntwortTokens);
    rumpf.insert(QLatin1String("thinking"), denken);
    rumpf.insert(QLatin1String("system"), systemText(klassen, sprache));
    rumpf.insert(QLatin1String("messages"), nachrichten);
    return rumpf;
}

QVariantMap fehlerKarte(const QString &text)
{
    QVariantMap karte;
    karte.insert(QLatin1String("ok"), false);
    karte.insert(QLatin1String("klasse"), QString());
    karte.insert(QLatin1String("sicherheit"), QString());
    karte.insert(QLatin1String("begruendung"), QString());
    karte.insert(QLatin1String("fehler"), text);
    return karte;
}

QString sicherheitNormieren(const QString &roh)
{
    /* Das Modell haelt sich meist an die drei Woerter, aber nicht immer an
       die Sprache; "high" oder "sehr hoch" sollen nicht als Fehler enden. */
    const QString s = roh.trimmed().toLower();
    if (s.contains(QLatin1String("hoch")) || s.contains(QLatin1String("high")))
        return QLatin1String("hoch");
    if (s.contains(QLatin1String("niedrig")) || s.contains(QLatin1String("low"))
            || s.contains(QLatin1String("gering")))
        return QLatin1String("niedrig");
    return QLatin1String("mittel");
}

bool klasseBekannt(const QString &id, const QVariantList &klassen)
{
    for (int i = 0; i < klassen.size(); ++i) {
        if (klassen.at(i).toMap().value(QLatin1String("id")).toString() == id)
            return true;
    }
    return false;
}

/* Aus dem Antwortrumpf der API eine Karte fuer fertig().  `httpOk` sagt,
   ob der Transport einen 2xx gemeldet hat; bei allem anderen steht im
   Rumpf meist ein Fehlerobjekt der API, dessen message hilfreicher ist
   als die blosse Zahl. */
QVariantMap antwortDeuten(const QByteArray &rumpf, bool httpOk, int status,
                          const QVariantList &klassen, const QString &sprache)
{
    QString parseFehler;
    const QVariant geparst = Json::parse(QString::fromUtf8(rumpf), &parseFehler);
    const QVariantMap antwort = geparst.toMap();

    if (antwort.value(QLatin1String("type")).toString() == QLatin1String("error")
            || !httpOk) {
        const QVariantMap fehler = antwort.value(QLatin1String("error")).toMap();
        QString text = fehler.value(QLatin1String("message")).toString();
        if (text.isEmpty())
            text = fehler.value(QLatin1String("type")).toString();
        if (text.isEmpty()) {
            text = zweisprachig(sprache, "Der Dienst hat abgelehnt",
                                "The service refused");
            if (status > 0)
                text += QString::fromLatin1(" (HTTP %1)").arg(status);
        }
        return fehlerKarte(text);
    }

    /* Der Text kann hinter einem Nachdenk-Block stehen; alle Textbloecke
       zusammen, nicht nur der erste. */
    QString text;
    const QVariantList inhalt = antwort.value(QLatin1String("content")).toList();
    for (int i = 0; i < inhalt.size(); ++i) {
        const QVariantMap block = inhalt.at(i).toMap();
        if (block.value(QLatin1String("type")).toString() == QLatin1String("text"))
            text += block.value(QLatin1String("text")).toString();
    }
    const int auf = text.indexOf(QLatin1Char('{'));
    const int zu = text.lastIndexOf(QLatin1Char('}'));
    QVariantMap urteil;
    if (auf >= 0 && zu > auf)
        urteil = Json::parse(text.mid(auf, zu - auf + 1)).toMap();
    if (urteil.isEmpty()) {
        return fehlerKarte(zweisprachig(sprache, "Antwort nicht lesbar",
                                        "Reply not readable"));
    }

    QString klasse = urteil.value(QLatin1String("klasse")).toString().trimmed();
    if (!klasseBekannt(klasse, klassen))
        klasse.clear();
    QVariantMap karte;
    karte.insert(QLatin1String("ok"), true);
    karte.insert(QLatin1String("klasse"), klasse);
    karte.insert(QLatin1String("sicherheit"),
                 sicherheitNormieren(urteil.value(QLatin1String("sicherheit")).toString()));
    karte.insert(QLatin1String("begruendung"),
                 urteil.value(QLatin1String("begruendung")).toString().trimmed());
    karte.insert(QLatin1String("fehler"), QString());
    return karte;
}

#if QT_VERSION < 0x050000
QString helfer()
{
    /* Dieselbe Kruecke wie bei Lichess im Schachlehrer: ein wget, das TLS
       1.2 kann.  Die Umgebungsvariable ist fuer den Fall, dass es woanders
       liegt. */
    const QByteArray gesetzt = qgetenv("SEGELFLUG_HTTPS_HELPER");
    if (!gesetzt.isEmpty() && QFile::exists(QString::fromLocal8Bit(gesetzt)))
        return QString::fromLocal8Bit(gesetzt);
    static const char *kOrte[] = {
        "/opt/wunderw/bin/wget",
        "/usr/local/bin/wget",
    };
    for (unsigned i = 0; i < sizeof(kOrte) / sizeof(kOrte[0]); ++i) {
        if (QFile::exists(QString::fromLatin1(kOrte[i])))
            return QString::fromLatin1(kOrte[i]);
    }
    return QString();
}
#endif

}

struct OnlineDeuter::Daten
{
    Daten()
        : laeuft(false)
        , lauf(0)
#if QT_VERSION >= 0x050000
        , netz(0)
        , antwort(0)
#else
        , prozess(0)
        , rumpfDatei(0)
        , schluesselDatei(0)
#endif
    {
    }

    bool laeuft;
    int lauf;                   // zaehlt Anfragen, damit Spaetes verfaellt
    QVariantList klassen;
    QString sprache;
    QString schluessel;         // nur, um ihn aus Fehlertexten zu tilgen
    QVariantMap verzoegert;     // Fehler, der im naechsten Umlauf gemeldet wird
    QTimer uhr;
#if QT_VERSION >= 0x050000
    QNetworkAccessManager *netz;
    QNetworkReply *antwort;
#else
    QProcess *prozess;
    QTemporaryFile *rumpfDatei;
    QTemporaryFile *schluesselDatei;
#endif
};

OnlineDeuter::OnlineDeuter(QObject *parent)
    : QObject(parent)
    , d(new Daten)
{
    d->uhr.setSingleShot(true);
    connect(&d->uhr, SIGNAL(timeout()), this, SLOT(zeitAbgelaufen()));
}

OnlineDeuter::~OnlineDeuter()
{
    abbrechen();
    delete d;
}

bool OnlineDeuter::laeuft() const
{
    return d->laeuft;
}

QString OnlineDeuter::schluesselAusProfil()
{
    /* Auf Sailfish liegt ~/.profile ausserhalb des Sandkastens; dann ist
       die Datei einfach nicht da, und das Ergebnis bleibt leer. */
    QFile datei(QDir::homePath() + QLatin1String("/.profile"));
    if (!datei.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream strom(&datei);
    strom.setCodec("UTF-8");
    const QString name = QLatin1String("ANTHROPIC_API_KEY=");
    while (!strom.atEnd()) {
        QString zeile = strom.readLine().trimmed();
        if (zeile.startsWith(QLatin1String("export ")))
            zeile = zeile.mid(7).trimmed();
        if (!zeile.startsWith(name))
            continue;
        QString wert = zeile.mid(name.size()).trimmed();
        if (wert.size() >= 2
                && (wert.startsWith(QLatin1Char('"')) || wert.startsWith(QLatin1Char('\'')))
                && wert.endsWith(wert.at(0))) {
            wert = wert.mid(1, wert.size() - 2);
        }
        if (!wert.isEmpty())
            return wert;
    }
    return QString();
}

void OnlineDeuter::fehlerMelden()
{
    /* Ein Fehler, der schon in anfragen() feststand (Bild nicht lesbar,
       kein Hilfsprogramm), kommt trotzdem erst im naechsten Umlauf: Course
       rechnet damit, dass fertig() nie aus anfragen() heraus feuert. */
    if (!d->laeuft || d->verzoegert.isEmpty())
        return;
    const QVariantMap karte = d->verzoegert;
    d->verzoegert.clear();
    d->laeuft = false;
    d->schluessel.clear();
    emit fertig(karte);
}

void OnlineDeuter::melden(QVariantMap karte)
{
    d->laeuft = false;
    QString fehler = karte.value(QLatin1String("fehler")).toString();
    if (!d->schluessel.isEmpty() && fehler.contains(d->schluessel)) {
        fehler.replace(d->schluessel, QLatin1String("***"));
        karte.insert(QLatin1String("fehler"), fehler);
    }
    d->schluessel.clear();
    emit fertig(karte);
}

#if QT_VERSION >= 0x050000

bool OnlineDeuter::verfuegbar()
{
    return true;
}

void OnlineDeuter::anfragen(const QString &bildPfad, const QVariantList &klassen,
                            const QString &sprache, const QString &schluessel)
{
    abbrechen();
    ++d->lauf;
    d->laeuft = true;
    d->klassen = klassen;
    d->sprache = sprache;
    d->schluessel = schluessel;

    QString bildFehler;
    const QByteArray base64 = bildAlsBase64(bildPfad, &bildFehler);
    if (base64.isEmpty()) {
        d->verzoegert = fehlerKarte(zweisprachig(sprache, "Foto nicht lesbar",
                                                 "Photo not readable"));
        QTimer::singleShot(0, this, SLOT(fehlerMelden()));
        return;
    }

    if (!d->netz)
        d->netz = new QNetworkAccessManager(this);
    /* Zwei Klammern mehr, sonst liest der Uebersetzer eine
       Funktionsdeklaration (most vexing parse). */
    const QUrl ziel(apiUrl());
    QNetworkRequest anfrage(ziel);
    anfrage.setRawHeader("x-api-key", schluessel.toUtf8());
    anfrage.setRawHeader("anthropic-version", kVersion);
    anfrage.setHeader(QNetworkRequest::ContentTypeHeader,
                      QLatin1String("application/json"));
    const QByteArray rumpf = Json::stringify(anfrageRumpf(base64, klassen, sprache)).toUtf8();
    d->antwort = d->netz->post(anfrage, rumpf);
    connect(d->antwort, SIGNAL(finished()), this, SLOT(antwortFertig()));
    d->uhr.start(kZeitlimitMs);
}

void OnlineDeuter::abbrechen()
{
    d->uhr.stop();
    d->verzoegert.clear();
    if (d->antwort) {
        /* Erst den Zeiger raeumen, dann abort(): finished() feuert darin
           sofort, und antwortFertig() soll die Leiche nicht mehr kennen. */
        QNetworkReply *alt = d->antwort;
        d->antwort = 0;
        alt->disconnect(this);
        alt->abort();
        alt->deleteLater();
    }
    d->laeuft = false;
    d->schluessel.clear();
}

void OnlineDeuter::antwortFertig()
{
    QNetworkReply *antwort = d->antwort;
    if (!antwort || sender() != antwort)
        return;
    d->antwort = 0;
    d->uhr.stop();
    const int status = antwort->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray rumpf = antwort->readAll();
    const bool netzFehler = antwort->error() != QNetworkReply::NoError && status == 0;
    const QString fehlerText = antwort->errorString();
    antwort->deleteLater();

    if (netzFehler) {
        melden(fehlerKarte(zweisprachig(d->sprache, "Keine Verbindung: ",
                                        "No connection: ") + fehlerText));
        return;
    }
    melden(antwortDeuten(rumpf, status >= 200 && status < 300, status,
                         d->klassen, d->sprache));
}

void OnlineDeuter::zeitAbgelaufen()
{
    if (!d->antwort)
        return;
    QNetworkReply *alt = d->antwort;
    d->antwort = 0;
    alt->disconnect(this);
    alt->abort();
    alt->deleteLater();
    melden(fehlerKarte(zweisprachig(d->sprache, "Zeit abgelaufen", "Timed out")));
}

void OnlineDeuter::prozessFertig(int)
{
    // Nur auf Harmattan; moc verlangt den Slot in beiden Fassungen.
}

#else  /* Qt 4.7, Harmattan: ueber den wget-Helfer */

bool OnlineDeuter::verfuegbar()
{
    return !helfer().isEmpty();
}

void OnlineDeuter::anfragen(const QString &bildPfad, const QVariantList &klassen,
                            const QString &sprache, const QString &schluessel)
{
    abbrechen();
    ++d->lauf;
    d->laeuft = true;
    d->klassen = klassen;
    d->sprache = sprache;
    d->schluessel = schluessel;

    const QString programm = helfer();
    if (programm.isEmpty()) {
        d->verzoegert = fehlerKarte(zweisprachig(sprache, "Kein Hilfsprogramm fürs Netz",
                                                 "No network helper installed"));
        QTimer::singleShot(0, this, SLOT(fehlerMelden()));
        return;
    }
    QString bildFehler;
    const QByteArray base64 = bildAlsBase64(bildPfad, &bildFehler);
    if (base64.isEmpty()) {
        d->verzoegert = fehlerKarte(zweisprachig(sprache, "Foto nicht lesbar",
                                                 "Photo not readable"));
        QTimer::singleShot(0, this, SLOT(fehlerMelden()));
        return;
    }

    /* Rumpf und Schluessel gehen je ueber eine eigene Datei mit Rechten
       0600 -- QTemporaryFile legt sie so an und raeumt sie beim Loeschen
       des Objekts wieder weg.  Der Schluessel steht damit nie in argv, wo
       ps ihn laese, sondern nur in einer wgetrc, die WGETRC benennt. */
    const QString vorlage = QDir::tempPath() + QLatin1String("/segelflug-XXXXXX");
    d->rumpfDatei = new QTemporaryFile(vorlage);
    d->schluesselDatei = new QTemporaryFile(vorlage);
    if (!d->rumpfDatei->open() || !d->schluesselDatei->open()) {
        aufraeumen();
        d->verzoegert = fehlerKarte(zweisprachig(sprache, "Kein Platz für die Anfrage",
                                                 "No room for the request"));
        QTimer::singleShot(0, this, SLOT(fehlerMelden()));
        return;
    }
    d->rumpfDatei->setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    d->schluesselDatei->setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    d->rumpfDatei->write(Json::stringify(anfrageRumpf(base64, klassen, sprache)).toUtf8());
    d->rumpfDatei->flush();
    d->schluesselDatei->write("header = x-api-key: " + schluessel.toUtf8() + "\n");
    d->schluesselDatei->flush();

    QStringList argumente;
    argumente << QLatin1String("--quiet")
              << QLatin1String("--tries=1")
              << QLatin1String("--timeout=90")
              << QLatin1String("--content-on-error")
              << QLatin1String("--header=Content-Type: application/json")
              << QString::fromLatin1("--header=anthropic-version: %1").arg(QLatin1String(kVersion))
              << QLatin1String("--post-file=") + d->rumpfDatei->fileName()
              << QLatin1String("-O") << QLatin1String("-")
              << apiUrl();

    d->prozess = new QProcess(this);
    QProcessEnvironment umgebung = QProcessEnvironment::systemEnvironment();
    umgebung.insert(QLatin1String("WGETRC"), d->schluesselDatei->fileName());
    d->prozess->setProcessEnvironment(umgebung);
    d->prozess->setStandardErrorFile(QLatin1String("/dev/null"));
    connect(d->prozess, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(prozessFertig(int)));
    d->prozess->start(programm, argumente);
    if (!d->prozess->waitForStarted(5000)) {
        QProcess *alt = d->prozess;
        d->prozess = 0;
        alt->deleteLater();
        aufraeumen();
        d->verzoegert = fehlerKarte(zweisprachig(sprache, "Hilfsprogramm startet nicht",
                                                 "Network helper does not start"));
        QTimer::singleShot(0, this, SLOT(fehlerMelden()));
        return;
    }
    d->uhr.start(kZeitlimitMs);
}

void OnlineDeuter::aufraeumen()
{
    delete d->rumpfDatei;
    d->rumpfDatei = 0;
    delete d->schluesselDatei;
    d->schluesselDatei = 0;
}

void OnlineDeuter::abbrechen()
{
    d->uhr.stop();
    d->verzoegert.clear();
    if (d->prozess) {
        QProcess *alt = d->prozess;
        d->prozess = 0;
        alt->disconnect(this);
        alt->kill();
        alt->waitForFinished(1000);
        alt->deleteLater();
    }
    aufraeumen();
    d->laeuft = false;
    d->schluessel.clear();
}

void OnlineDeuter::prozessFertig(int code)
{
    QProcess *prozess = d->prozess;
    if (!prozess || sender() != prozess)
        return;
    d->prozess = 0;
    d->uhr.stop();
    const QByteArray rumpf = prozess->readAllStandardOutput();
    const bool abgestuerzt = prozess->exitStatus() != QProcess::NormalExit;
    prozess->deleteLater();
    aufraeumen();

    /* wget nennt den HTTP-Status nicht; 8 heisst "Server hat abgelehnt",
       und dank --content-on-error liegt der Rumpf mit der Meldung der API
       trotzdem vor.  Alles andere als 0 und 8 ist kein Netz. */
    if (abgestuerzt || (code != 0 && code != 8)) {
        melden(fehlerKarte(zweisprachig(d->sprache, "Keine Verbindung", "No connection")
                           + QString::fromLatin1(" (%1)").arg(code)));
        return;
    }
    melden(antwortDeuten(rumpf, code == 0, 0, d->klassen, d->sprache));
}

void OnlineDeuter::zeitAbgelaufen()
{
    if (!d->prozess)
        return;
    QProcess *alt = d->prozess;
    d->prozess = 0;
    alt->disconnect(this);
    alt->kill();
    alt->deleteLater();
    aufraeumen();
    melden(fehlerKarte(zweisprachig(d->sprache, "Zeit abgelaufen", "Timed out")));
}

void OnlineDeuter::antwortFertig()
{
    // Nur auf Qt 5; moc verlangt den Slot in beiden Fassungen.
}

#endif
