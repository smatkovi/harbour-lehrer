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
#endif

/* Der gemeinsame Teil -- Bild packen, Anfrage bauen, Antwort deuten --
   kennt keinen Transport.  Beide Wege unten liefern am Ende nur ein
   Byte-Feld und einen HTTP-Status, und alles weitere passiert hier,
   damit Harmattan und Sailfish dieselbe Antwort lesen. */
namespace {

const char kModell[] = "claude-sonnet-5";
const char kVersion[] = "2023-06-01";
/* Platz fuer das Urteil: Das Modell antwortet ueber ein Werkzeug (unten),
   dessen Eingabe die API als gueltiges JSON liefert; 400 waren knapp, wenn
   die Begruendung laenger geriet -- dann kam ein abgeschnittener Text und
   "Antwort nicht lesbar". */
const int kAntwortTokens = 800;
/* Wohin der rohe Antwortrumpf geschrieben wird, wenn er sich nicht deuten
   laesst (Course setzt den Pfad): So laesst sich ein "nicht lesbar" am
   Geraet nachlesen, statt zu raten.  Der Rumpf enthaelt keinen
   Schluessel. */
QString g_protokoll;
const int kZeitlimitMs = 90 * 1000;

QString zweisprachig(const QString &sprache, const char *de, const char *en)
{
    return QString::fromUtf8(sprache == QLatin1String("en") ? en : de);
}

QString apiUrl()
{
#ifdef SEGELFLUG_PRUEFSTAND
    /* Nur der Pruefstand zeigt auf einen Scheinserver.  In der
       ausgelieferten App gibt es diesen Weg nicht: Wer die Umgebung der App
       setzen kann (auf Harmattan eine Zeile in ~/.profile), lenkte sonst
       Schluessel und Foto auf einen beliebigen Rechner, und das auch noch
       im Klartext. */
    const QByteArray gesetzt = qgetenv("SEGELFLUG_API_URL");
    if (!gesetzt.isEmpty())
        return QString::fromLocal8Bit(gesetzt);
#endif
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
        "Answer through the tool \"urteil\": klasse = the id, sicherheit = "
        "hoch, mittel or niedrig, begruendung = one or two sentences in ");
    text += sprache == QLatin1String("en") ? QLatin1String("English")
                                           : QLatin1String("German");
    text += QLatin1String(
        ": what gives the class away in this photo, and what it means for "
        "gliding on this day.");
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
                 QLatin1String("Which class is this sky?"));
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

    /* Das Urteil als Werkzeugaufruf: Die API prueft die Eingabe gegen das
       Schema und liefert sie als JSON-Objekt -- kein Suchen nach Klammern
       in Prosa mehr, keine halben Objekte, keine erfundenen Woerter fuer
       die Sicherheit.  tool_choice erzwingt den Aufruf. */
    QVariantList kennungen;
    for (int i = 0; i < klassen.size(); ++i)
        kennungen << klassen.at(i).toMap().value(QLatin1String("id")).toString();
    kennungen << QString();
    QVariantMap klasseFeld;
    klasseFeld.insert(QLatin1String("type"), QLatin1String("string"));
    klasseFeld.insert(QLatin1String("enum"), kennungen);
    klasseFeld.insert(QLatin1String("description"),
                      QLatin1String("the id of the class, or an empty string if none fits"));
    QVariantMap sicherheitFeld;
    sicherheitFeld.insert(QLatin1String("type"), QLatin1String("string"));
    sicherheitFeld.insert(QLatin1String("enum"), QVariantList()
                          << QLatin1String("hoch") << QLatin1String("mittel")
                          << QLatin1String("niedrig"));
    QVariantMap begruendungFeld;
    begruendungFeld.insert(QLatin1String("type"), QLatin1String("string"));
    begruendungFeld.insert(QLatin1String("description"),
                           QLatin1String("one or two sentences: what gives the class away, "
                                         "and what it means for gliding today"));
    QVariantMap felder;
    felder.insert(QLatin1String("klasse"), klasseFeld);
    felder.insert(QLatin1String("sicherheit"), sicherheitFeld);
    felder.insert(QLatin1String("begruendung"), begruendungFeld);
    QVariantMap schema;
    schema.insert(QLatin1String("type"), QLatin1String("object"));
    schema.insert(QLatin1String("properties"), felder);
    schema.insert(QLatin1String("required"), QVariantList()
                  << QLatin1String("klasse") << QLatin1String("sicherheit")
                  << QLatin1String("begruendung"));
    QVariantMap werkzeug;
    werkzeug.insert(QLatin1String("name"), QLatin1String("urteil"));
    werkzeug.insert(QLatin1String("description"),
                    QLatin1String("Deliver the verdict on the sky photo."));
    werkzeug.insert(QLatin1String("input_schema"), schema);
    QVariantMap wahl;
    wahl.insert(QLatin1String("type"), QLatin1String("tool"));
    wahl.insert(QLatin1String("name"), QLatin1String("urteil"));

    QVariantMap rumpf;
    rumpf.insert(QLatin1String("model"), QLatin1String(kModell));
    rumpf.insert(QLatin1String("max_tokens"), kAntwortTokens);
    rumpf.insert(QLatin1String("thinking"), denken);
    rumpf.insert(QLatin1String("system"), systemText(klassen, sprache));
    rumpf.insert(QLatin1String("tools"), QVariantList() << werkzeug);
    rumpf.insert(QLatin1String("tool_choice"), wahl);
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

void protokollSchreiben(const QByteArray &rumpf)
{
    if (g_protokoll.isEmpty())
        return;
    QFile datei(g_protokoll);
    if (!datei.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    datei.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    datei.write(rumpf.left(256 * 1024));
    datei.close();
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

    /* Zuerst der Werkzeugaufruf (so ist die Anfrage gestellt); zur
       Sicherheit auch noch der alte Weg, JSON im Text -- der Text kann
       hinter einem Nachdenk-Block stehen, alle Textbloecke zusammen. */
    QString text;
    QVariantMap urteil;
    const QVariantList inhalt = antwort.value(QLatin1String("content")).toList();
    for (int i = 0; i < inhalt.size(); ++i) {
        const QVariantMap block = inhalt.at(i).toMap();
        const QString art = block.value(QLatin1String("type")).toString();
        if (art == QLatin1String("tool_use")
                && block.value(QLatin1String("name")).toString() == QLatin1String("urteil")
                && urteil.isEmpty())
            urteil = block.value(QLatin1String("input")).toMap();
        else if (art == QLatin1String("text"))
            text += block.value(QLatin1String("text")).toString();
    }
    if (urteil.isEmpty()) {
        const int auf = text.indexOf(QLatin1Char('{'));
        const int zu = text.lastIndexOf(QLatin1Char('}'));
        if (auf >= 0 && zu > auf)
            urteil = Json::parse(text.mid(auf, zu - auf + 1)).toMap();
    }
    if (urteil.isEmpty()) {
        /* Den Rumpf aufheben und sagen, woran es lag: abgeschnitten, leer,
           oder der Anfang dessen, was statt des Urteils kam. */
        protokollSchreiben(rumpf);
        QString grund;
        if (antwort.value(QLatin1String("stop_reason")).toString() == QLatin1String("max_tokens"))
            grund = zweisprachig(sprache, "abgeschnitten", "cut off");
        else if (text.trimmed().isEmpty())
            grund = zweisprachig(sprache, "leer", "empty");
        else
            grund = text.trimmed().left(80).simplified() + QString::fromUtf8("\xe2\x80\xa6");
        return fehlerKarte(zweisprachig(sprache, "Antwort nicht lesbar: ",
                                        "Reply not readable: ") + grund);
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

/* Wo Rumpf und wgetrc liegen: im eigenen Ordner unter den
   Einstellungen, nicht in /tmp.  Dort sind sie nur fuer den Benutzer
   lesbar, und ein Absturz laesst hoechstens eine Leiche liegen, die der
   naechste Lauf ueberschreibt. */
QString anfrageOrdner()
{
    return QDir::homePath() + QLatin1String("/.config/segelflug");
}
QString helfer()
{
    /* Dieselbe Kruecke wie bei Lichess im Schachlehrer: ein wget, das TLS
       1.2 kann. */
#ifdef SEGELFLUG_PRUEFSTAND
    const QByteArray gesetzt = qgetenv("SEGELFLUG_HTTPS_HELPER");
    if (!gesetzt.isEmpty() && QFile::exists(QString::fromLocal8Bit(gesetzt)))
        return QString::fromLocal8Bit(gesetzt);
#endif
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
    QFile *rumpfDatei;
    QFile *schluesselDatei;
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

void OnlineDeuter::protokollSetzen(const QString &pfad)
{
    g_protokoll = pfad;
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
       0600.  Der Schluessel steht damit nie in argv, wo ps ihn laese,
       sondern nur in einer wgetrc, die WGETRC benennt.

       Feste Namen in einem eigenen Ordner, nicht QTemporaryFile in /tmp:
       Stirbt die App mitten in der Anfrage (am N950 holt der Kernel sich
       bei Speichernot gern einen Prozess), raeumt kein Destruktor mehr auf
       -- mit festen Namen ueberschreibt der naechste Lauf die Leiche des
       vorigen, statt eine zweite dazuzulegen. */
    const QString ordner = anfrageOrdner();
    QDir().mkpath(ordner);
    d->rumpfDatei = new QFile(ordner + QLatin1String("/anfrage-rumpf"));
    d->schluesselDatei = new QFile(ordner + QLatin1String("/anfrage-kopf"));
    if (!d->rumpfDatei->open(QIODevice::WriteOnly | QIODevice::Truncate)
            || !d->schluesselDatei->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        aufraeumen();
        d->verzoegert = fehlerKarte(zweisprachig(sprache, "Kein Platz für die Anfrage",
                                                 "No room for the request"));
        QTimer::singleShot(0, this, SLOT(fehlerMelden()));
        return;
    }
    d->rumpfDatei->setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    d->schluesselDatei->setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    /* Ob das Schreiben gelang, muss gefragt werden: /tmp ist auf Harmattan
       ein tmpfs mit vier Megabyte, und eine halb geschriebene Anfrage
       brachte sonst eine 400-Meldung der API zurueck statt des Satzes, der
       zwei Zeilen darueber schon bereitsteht. */
    const QByteArray rumpfRoh = Json::stringify(anfrageRumpf(base64, klassen, sprache)).toUtf8();
    const QByteArray kopfRoh = "header = x-api-key: " + schluessel.toUtf8() + "\n";
    if (d->rumpfDatei->write(rumpfRoh) != rumpfRoh.size() || !d->rumpfDatei->flush()
            || d->schluesselDatei->write(kopfRoh) != kopfRoh.size()
            || !d->schluesselDatei->flush()) {
        aufraeumen();
        d->verzoegert = fehlerKarte(zweisprachig(sprache, "Kein Platz für die Anfrage",
                                                 "No room for the request"));
        QTimer::singleShot(0, this, SLOT(fehlerMelden()));
        return;
    }

    QStringList argumente;
    argumente << QLatin1String("--quiet")
              << QLatin1String("--tries=1")
              << QLatin1String("--timeout=90")
              << QLatin1String("--header=Content-Type: application/json")
              << QString::fromLatin1("--header=anthropic-version: %1").arg(QLatin1String(kVersion))
              << QLatin1String("--post-file=") + d->rumpfDatei->fileName()
              << QLatin1String("-O") << QLatin1String("-")
              << apiUrl();

    d->prozess = new QProcess(this);
    QProcessEnvironment umgebung = QProcessEnvironment::systemEnvironment();
    /* Stand der Schluessel beim Start in der Umgebung (auf den Nokias in
       ~/.profile), erbte ihn das Kind und jeder koennte ihn in
       /proc/<pid>/environ lesen -- obwohl wget ihn aus der wgetrc nimmt. */
    umgebung.remove(QLatin1String("ANTHROPIC_API_KEY"));
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
    /* QFile raeumt nicht von selbst weg -- der Schluessel darf nicht
       liegenbleiben. */
    for (int i = 0; i < 2; ++i) {
        QFile *datei = i == 0 ? d->rumpfDatei : d->schluesselDatei;
        if (!datei)
            continue;
        const QString name = datei->fileName();
        datei->close();
        delete datei;
        if (!name.isEmpty())
            QFile::remove(name);
    }
    d->rumpfDatei = 0;
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

    /* wget nennt den HTTP-Status nicht, nur seinen eigenen Rueckgabewert:
       6 heisst "Beglaubigung abgelehnt" (das sieht wget bei 401), 8 "Server
       hat abgelehnt" (alles andere ab 400).  Ohne --content-on-error (das
       kennt erst wget 1.14, und welches wget wunderw mitbringt, ist nicht
       geprueft -- die Lichess-Anbindung des Schachlehrers kommt ohne aus)
       bleibt der Rumpf dann meist leer; dann sagt die Karte nur, woran es
       lag.  Alles andere ist kein Netz. */
    if (abgestuerzt || (code != 0 && code != 6 && code != 8)) {
        melden(fehlerKarte(zweisprachig(d->sprache, "Keine Verbindung", "No connection")
                           + QString::fromLatin1(" (%1)").arg(code)));
        return;
    }
    if (code == 6 || (code == 8 && rumpf.trimmed().isEmpty())) {
        melden(fehlerKarte(zweisprachig(d->sprache,
                                        "Der Server hat die Anfrage abgelehnt (Schlüssel?).",
                                        "The server rejected the request (key?).")));
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
