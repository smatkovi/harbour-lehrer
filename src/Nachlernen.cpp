#include "Nachlernen.h"

#include <QBuffer>
#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QVariantMap>

#include <cmath>
#include <vector>

#include "Json.h"

/* Das Format steht in segelflug/tools/GELERNTES.md.  Die drei Konstanten
   der Korrektur stehen hier an einer Stelle und sind dieselben wie in
   segelflug/training/nachlernen_versuch.py -- wer sie aendert, misst
   vorher. */
namespace {

const char kKennung[8] = { 'W', 'O', 'L', 'K', 'E', 'N', 'G', '1' };

// Wie scharf die Aehnlichkeit in eine Wahrscheinlichkeit uebersetzt wird.
const float kSchaerfe = 8.0f;
// Ab wann das Gedaechtnis die Haelfte des Gewichts hat: w = n / (n + n0).
const float kHalbwert = 30.0f;
// Darunter bleibt die Korrektur ganz aus -- so wenige Beispiele sind
// Rauschen (im Versuch verschlechterten sie das Ergebnis).
const int kMindestens = 20;
// Ab so vielen Eintraegen wird die letzte Schicht nachtrainiert: darunter
// lernt sie die Irrtuemer des Lehrers mit (im Versuch bei 50 Eintraegen und
// einem 80-%-Lehrer noch leicht negativ, ab 80 deutlich positiv).
const int kKopfAb = 40;
// Das Nachtrainieren: Schritte, Lernrate, Zug zur ausgelieferten Schicht.
// Gemessen in training/nachlernen_versuch.py -- bei 200 Schritten mit
// Lernrate 1 ist es ausgelaufen, mehr aendert nichts.
const int kKopfSchritte = 200;
const float kKopfLernrate = 1.0f;
const float kKopfZug = 0.01f;

bool fehlschlag(QString *fehler, const QString &text)
{
    if (fehler)
        *fehler = text;
    return false;
}

}

struct Eintrag
{
    QString klasse;
    quint8 quelle;          // Nachlernen::Quelle
    quint8 sicherheit;      // 0 keine, 1 niedrig, 2 mittel, 3 hoch
    qint64 zeit;            // Unix-Sekunden
    QByteArray bild;        // JPEG, eingabe x eingabe
    QVector<float> abdruck; // normiert; leer, wenn er neu gerechnet werden muss
    float laenge;           // seine Laenge vor dem Normieren

    Eintrag() : quelle(0), sicherheit(0), zeit(0), laenge(0.f) {}
};

struct Nachlernen::Daten
{
    QList<Eintrag> eintraege;
    QString fingerabdruck;      // des Netzes, mit dem die Abdruecke gelten
    QStringList klassen;
    int abdruckGroesse;

    /* Die Klassenmittel, aus den Eintraegen gerechnet und bei jeder
       Aenderung verworfen.  Je Klasse ein normierter Vektor; leer, wo zu
       einer Klasse nichts (oder nur Eintraege ohne Abdruck) liegt. */
    mutable QList<QVector<float> > mittel;
    mutable QList<int> mittelZahl;
    mutable bool mittelGueltig;

    /* Die ausgelieferte letzte Schicht und, wenn nachtrainiert, die neue.
       Beide als [aus][ein] flach, dazu der Bias [aus]. */
    QVector<float> kopf0, bias0;
    QVector<float> kopf, bias;
    int kopfStand;              // zu welchem Stand die neue Schicht gehoert
    int stand;                  // zaehlt jede Aenderung an den Eintraegen

    Daten() : abdruckGroesse(0), mittelGueltig(false), kopfStand(-1), stand(0) {}

    void verwerfen()
    {
        mittelGueltig = false;
        ++stand;
    }
    void mittelRechnen() const;
};

void Nachlernen::Daten::mittelRechnen() const
{
    mittel.clear();
    mittelZahl.clear();
    for (int k = 0; k < klassen.size(); ++k) {
        mittel.append(QVector<float>());
        mittelZahl.append(0);
    }
    if (abdruckGroesse <= 0) {
        mittelGueltig = true;
        return;
    }
    std::vector< std::vector<double> > summe((size_t)klassen.size());
    for (int i = 0; i < eintraege.size(); ++i) {
        const Eintrag &e = eintraege.at(i);
        const int k = klassen.indexOf(e.klasse);
        if (k < 0 || e.abdruck.size() != abdruckGroesse)
            continue;
        std::vector<double> &s = summe[(size_t)k];
        if (s.empty())
            s.assign((size_t)abdruckGroesse, 0.0);
        for (int j = 0; j < abdruckGroesse; ++j)
            s[(size_t)j] += e.abdruck.at(j);
        mittelZahl[k] += 1;
    }
    for (int k = 0; k < klassen.size(); ++k) {
        if (mittelZahl.at(k) <= 0)
            continue;
        const std::vector<double> &s = summe[(size_t)k];
        double quadrat = 0.0;
        for (int j = 0; j < abdruckGroesse; ++j)
            quadrat += s[(size_t)j] * s[(size_t)j];
        const double norm = quadrat > 1e-12 ? 1.0 / std::sqrt(quadrat) : 1.0;
        QVector<float> v(abdruckGroesse);
        for (int j = 0; j < abdruckGroesse; ++j)
            v[j] = (float)(s[(size_t)j] * norm);
        mittel[k] = v;
    }
    mittelGueltig = true;
}

Nachlernen::Nachlernen()
    : d(new Daten)
{
}

Nachlernen::~Nachlernen()
{
    delete d;
}

void Nachlernen::netzSetzen(const QString &fingerabdruck, const QStringList &klassen)
{
    d->klassen = klassen;
    /* Passt der Fingerabdruck nicht zu dem, mit dem die Abdruecke gerechnet
       wurden, sind sie wertlos -- die Bilder bleiben, die Abdruecke gehen.
       Der Aufrufer rechnet sie dann ueber brauchtNeurechnung() neu. */
    if (!d->fingerabdruck.isEmpty() && d->fingerabdruck != fingerabdruck)
        for (int i = 0; i < d->eintraege.size(); ++i)
            d->eintraege[i].abdruck.clear();
    d->fingerabdruck = fingerabdruck;
    d->verwerfen();
}

QStringList Nachlernen::brauchtNeurechnung() const
{
    QStringList aus;
    for (int i = 0; i < d->eintraege.size(); ++i)
        if (d->eintraege.at(i).abdruck.isEmpty())
            aus << QString::number(i);
    return aus;
}

void Nachlernen::abdruckSetzen(int nummer, const QVector<float> &abdruck, float laenge)
{
    if (nummer < 0 || nummer >= d->eintraege.size() || abdruck.isEmpty())
        return;
    if (d->abdruckGroesse <= 0)
        d->abdruckGroesse = abdruck.size();
    if (abdruck.size() != d->abdruckGroesse)
        return;
    d->eintraege[nummer].abdruck = abdruck;
    d->eintraege[nummer].laenge = laenge;
    d->verwerfen();
}

int Nachlernen::anzahl() const
{
    return d->eintraege.size();
}

int Nachlernen::anzahlJeKlasse(const QString &klasse) const
{
    int n = 0;
    for (int i = 0; i < d->eintraege.size(); ++i)
        if (d->eintraege.at(i).klasse == klasse)
            ++n;
    return n;
}

bool Nachlernen::wirksam() const
{
    return nutzbare() >= kMindestens;
}

int Nachlernen::fehlende() const
{
    const int n = nutzbare();
    return n >= kMindestens ? 0 : kMindestens - n;
}

/* Gezaehlt wird, was auch wirken kann: ein Eintrag ohne Abdruck (weil das
   Netz gewechselt hat und noch nicht nachgerechnet wurde) und einer mit
   unbekannter Klasse zaehlen nicht mit. */
int Nachlernen::nutzbare() const
{
    int n = 0;
    for (int i = 0; i < d->eintraege.size(); ++i) {
        const Eintrag &e = d->eintraege.at(i);
        if (!e.abdruck.isEmpty() && d->klassen.contains(e.klasse))
            ++n;
    }
    return n;
}

QByteArray Nachlernen::bildDaten(int nummer) const
{
    if (nummer < 0 || nummer >= d->eintraege.size())
        return QByteArray();
    return d->eintraege.at(nummer).bild;
}

QVariantList Nachlernen::eintraege() const
{
    QVariantList aus;
    /* Das Neueste zuerst -- wer nachsieht, sucht meist das, was er gerade
       gelernt hat. */
    for (int i = d->eintraege.size() - 1; i >= 0; --i) {
        const Eintrag &e = d->eintraege.at(i);
        QVariantMap m;
        m.insert(QLatin1String("nummer"), i);
        m.insert(QLatin1String("klasse"), e.klasse);
        m.insert(QLatin1String("quelle"), (int)e.quelle);
        m.insert(QLatin1String("sicherheit"), (int)e.sicherheit);
        m.insert(QLatin1String("zeit"), (qlonglong)e.zeit);
        /* Qt 4.7 gibt hier QBool zurueck, nicht bool, und QVariant kann
           damit nichts anfangen -- also ausdruecklich umwandeln. */
        m.insert(QLatin1String("bekannt"), bool(d->klassen.contains(e.klasse)));
        m.insert(QLatin1String("gerechnet"), !e.abdruck.isEmpty());
        aus.append(m);
    }
    return aus;
}

void Nachlernen::merken(const QString &klasse, Quelle quelle, int sicherheit,
                        const QImage &bild, const QVector<float> &abdruck, float laenge)
{
    if (klasse.isEmpty() || bild.isNull() || abdruck.isEmpty())
        return;
    if (d->abdruckGroesse <= 0)
        d->abdruckGroesse = abdruck.size();
    if (abdruck.size() != d->abdruckGroesse)
        return;

    QByteArray jpeg;
    QBuffer puffer(&jpeg);
    puffer.open(QIODevice::WriteOnly);
    if (!bild.save(&puffer, "JPEG", 85))
        return;
    puffer.close();

    /* Dasselbe Bild zweimal zu merken hilft nicht, es verschiebt nur das
       Klassenmittel. */
    for (int i = 0; i < d->eintraege.size(); ++i)
        if (d->eintraege.at(i).bild == jpeg) {
            d->eintraege[i].klasse = klasse;
            d->eintraege[i].quelle = (quint8)quelle;
            d->eintraege[i].sicherheit = (quint8)sicherheit;
            d->eintraege[i].abdruck = abdruck;
            d->eintraege[i].laenge = laenge;
            d->verwerfen();
            return;
        }

    Eintrag e;
    e.klasse = klasse;
    e.quelle = (quint8)quelle;
    e.sicherheit = (quint8)qBound(0, sicherheit, 3);
    e.zeit = QDateTime::currentDateTime().toUTC().toTime_t();
    e.bild = jpeg;
    e.abdruck = abdruck;
    e.laenge = laenge;
    d->eintraege.append(e);
    d->verwerfen();
}

bool Nachlernen::vergessen(int nummer)
{
    if (nummer < 0 || nummer >= d->eintraege.size())
        return false;
    d->eintraege.removeAt(nummer);
    d->verwerfen();
    return true;
}

void Nachlernen::alleVergessen()
{
    d->eintraege.clear();
    d->verwerfen();
}

QVector<float> Nachlernen::korrigieren(const QVector<float> &wahrscheinlichNetz,
                                       const QVector<float> &abdruck) const
{
    if (!wirksam() || abdruck.size() != d->abdruckGroesse
            || wahrscheinlichNetz.size() != d->klassen.size())
        return wahrscheinlichNetz;
    if (!d->mittelGueltig)
        d->mittelRechnen();

    /* Zuerst die nachtrainierte Schicht, wenn es eine gibt: Sie rechnet
       auf dem normierten Abdruck (so wurde sie trainiert) und ersetzt die
       Wahrscheinlichkeiten des Netzes.  Dann kommt das Gedaechtnis
       darueber -- im Versuch war die Verbindung aus beidem das Beste. */
    QVector<float> wahrscheinlich = wahrscheinlichNetz;
    if (kopfAktiv()) {
        const int aus = d->klassen.size();
        const int ein = d->abdruckGroesse;
        QVector<float> l(aus);
        float groesstes = -1e30f;
        for (int o = 0; o < aus; ++o) {
            const float *w = &d->kopf.constData()[(size_t)o * ein];
            float summe = d->bias.at(o);
            for (int j = 0; j < ein; ++j)
                summe += w[j] * abdruck.at(j);
            l[o] = summe;
            if (summe > groesstes) groesstes = summe;
        }
        double z = 0.0;
        for (int o = 0; o < aus; ++o) {
            l[o] = (float)std::exp((double)(l.at(o) - groesstes));
            z += l.at(o);
        }
        if (z > 0.0) {
            for (int o = 0; o < aus; ++o)
                wahrscheinlich[o] = (float)(l.at(o) / z);
        }
    }

    /* Kosinus zum Klassenmittel; beide Vektoren sind normiert, also ist das
       Skalarprodukt schon der Kosinus.  Klassen ohne Eintraege bekommen
       -1, zaehlen also als "sieht gar nicht so aus". */
    QVector<float> aehnlich(d->klassen.size(), -1.0f);
    for (int k = 0; k < d->klassen.size(); ++k) {
        const QVector<float> &m = d->mittel.at(k);
        if (m.size() != d->abdruckGroesse)
            continue;
        float punkt = 0.f;
        for (int j = 0; j < d->abdruckGroesse; ++j)
            punkt += m.at(j) * abdruck.at(j);
        aehnlich[k] = punkt;
    }

    float groesstes = aehnlich.at(0);
    for (int k = 1; k < aehnlich.size(); ++k)
        if (aehnlich.at(k) > groesstes) groesstes = aehnlich.at(k);
    double summe = 0.0;
    QVector<float> gedaechtnis(aehnlich.size());
    for (int k = 0; k < aehnlich.size(); ++k) {
        gedaechtnis[k] = (float)std::exp((double)(kSchaerfe * (aehnlich.at(k) - groesstes)));
        summe += gedaechtnis.at(k);
    }
    if (summe <= 0.0)
        return wahrscheinlich;

    const float n = (float)nutzbare();
    const float w = n / (n + kHalbwert);
    QVector<float> aus(wahrscheinlich.size());
    double gesamt = 0.0;
    for (int k = 0; k < aus.size(); ++k) {
        aus[k] = (1.0f - w) * wahrscheinlich.at(k)
                 + w * (float)(gedaechtnis.at(k) / summe);
        gesamt += aus.at(k);
    }
    if (gesamt > 0.0)
        for (int k = 0; k < aus.size(); ++k)
            aus[k] = (float)(aus.at(k) / gesamt);
    return aus;
}

// ---- Die letzte Schicht nachtrainieren ---------------------------------------

void Nachlernen::kopfSetzen(const QVector<float> &gewichte, const QVector<float> &bias)
{
    d->kopf0 = gewichte;
    d->bias0 = bias;
    d->kopf.clear();
    d->bias.clear();
    d->kopfStand = -1;
}

bool Nachlernen::kopfLohnt() const
{
    return nutzbare() >= kKopfAb && !d->kopf0.isEmpty()
            && d->abdruckGroesse > 0
            && d->kopf0.size() == d->klassen.size() * d->abdruckGroesse
            && d->bias0.size() == d->klassen.size();
}

int Nachlernen::stand() const
{
    return d->stand;
}

bool Nachlernen::kopfAktiv() const
{
    return d->kopfStand == d->stand && !d->kopf.isEmpty();
}

/* Gradientenabstieg auf der letzten Schicht, mit den normierten Abdruecken
   als Eingabe.  Die ausgelieferte Schicht erwartet die rohen (zehn- bis
   zwanzigmal so langen) Abdruecke; sie wird darum einmal mit der mittleren
   Laenge der gemerkten skaliert, dann stimmen die Logits am Anfang, und die
   Lernrate passt fuer jedes Bild gleich.  Start bei der ausgelieferten
   Schicht, Zug zurueck zu ihr -- beides gegen das Auswendiglernen weniger
   Beispiele.  Rechnet nur auf dem Lernstoff, der eine Abschrift ist --
   darf also in einem Arbeitsfaden laufen, waehrend das Gedaechtnis sich
   aendert. */
Nachlernen::Lernstoff Nachlernen::lernstoff() const
{
    Lernstoff s;
    if (!kopfLohnt())
        return s;
    s.aus = d->klassen.size();
    s.ein = d->abdruckGroesse;
    s.stand = d->stand;
    double laengen = 0.0;
    for (int i = 0; i < d->eintraege.size(); ++i) {
        const Eintrag &e = d->eintraege.at(i);
        const int k = d->klassen.indexOf(e.klasse);
        if (k < 0 || e.abdruck.size() != s.ein)
            continue;
        s.x += e.abdruck;
        s.y.append(k);
        laengen += e.laenge;
    }
    s.n = s.y.size();
    s.skala = s.n > 0 ? (float)(laengen / s.n) : 0.f;
    s.kopf0 = d->kopf0;
    s.bias0 = d->bias0;
    return s;
}

bool Nachlernen::kopfNachtrainieren(QVector<float> *gewichte, QVector<float> *bias) const
{
    return kopfNachtrainieren(lernstoff(), gewichte, bias);
}

bool Nachlernen::kopfNachtrainieren(const Lernstoff &s, QVector<float> *gewichte, QVector<float> *bias)
{
    const int aus = s.aus;
    const int ein = s.ein;
    const int n = s.n;
    if (n < 2 || aus <= 0 || ein <= 0 || !(s.skala > 0.f))
        return false;
    if (s.x.size() != n * ein || s.y.size() != n
            || s.kopf0.size() != aus * ein || s.bias0.size() != aus)
        return false;
    const float skala = s.skala;
    std::vector<const float *> x((size_t)n);
    for (int i = 0; i < n; ++i)
        x[(size_t)i] = s.x.constData() + (size_t)i * ein;
    const QVector<int> &y = s.y;

    std::vector<float> w0((size_t)aus * ein), w((size_t)aus * ein);
    for (size_t j = 0; j < w0.size(); ++j)
        w0[j] = s.kopf0.at((int)j) * skala;
    w = w0;
    std::vector<float> b0(aus), b(aus);
    for (int o = 0; o < aus; ++o)
        b0[(size_t)o] = b[(size_t)o] = s.bias0.at(o);

    std::vector<float> p((size_t)n * aus);
    std::vector<float> gw((size_t)aus * ein), gb((size_t)aus);
    for (int schritt = 0; schritt < kKopfSchritte; ++schritt) {
        /* Vorwaerts: Softmax je Beispiel. */
        for (int i = 0; i < n; ++i) {
            float *pi = &p[(size_t)i * aus];
            float groesstes = -1e30f;
            for (int o = 0; o < aus; ++o) {
                const float *wo = &w[(size_t)o * ein];
                float summe = b[(size_t)o];
                for (int j = 0; j < ein; ++j)
                    summe += wo[j] * x[(size_t)i][j];
                pi[o] = summe;
                if (summe > groesstes) groesstes = summe;
            }
            double z = 0.0;
            for (int o = 0; o < aus; ++o) {
                pi[o] = (float)std::exp((double)(pi[o] - groesstes));
                z += pi[o];
            }
            for (int o = 0; o < aus; ++o)
                pi[o] = (float)(pi[o] / z);
        }
        /* Rueckwaerts: (p - y)/n, dazu der Zug zur ausgelieferten Schicht. */
        for (int o = 0; o < aus; ++o) {
            gb[(size_t)o] = kKopfZug * (b[(size_t)o] - b0[(size_t)o]);
            float *gwo = &gw[(size_t)o * ein];
            for (int j = 0; j < ein; ++j)
                gwo[j] = kKopfZug * (w[(size_t)o * ein + j] - w0[(size_t)o * ein + j]);
        }
        for (int i = 0; i < n; ++i) {
            const float *pi = &p[(size_t)i * aus];
            for (int o = 0; o < aus; ++o) {
                const float f = (pi[o] - (y.at(i) == o ? 1.f : 0.f)) / (float)n;
                if (f == 0.f)
                    continue;
                gb[(size_t)o] += f;
                float *gwo = &gw[(size_t)o * ein];
                const float *xi = x[(size_t)i];
                for (int j = 0; j < ein; ++j)
                    gwo[j] += f * xi[j];
            }
        }
        for (size_t j = 0; j < w.size(); ++j)
            w[j] -= kKopfLernrate * gw[j];
        for (int o = 0; o < aus; ++o)
            b[(size_t)o] -= kKopfLernrate * gb[(size_t)o];
    }

    gewichte->resize((int)w.size());
    for (int j = 0; j < gewichte->size(); ++j)
        (*gewichte)[j] = w[(size_t)j];
    bias->resize(aus);
    for (int o = 0; o < aus; ++o)
        (*bias)[o] = b[(size_t)o];
    return true;
}

void Nachlernen::kopfUebernehmen(const QVector<float> &gewichte, const QVector<float> &bias, int stand)
{
    /* Zu einem alten Stand gerechnet: Inzwischen ist etwas dazugekommen
       oder weggefallen, und der Aufrufer rechnet ohnehin neu. */
    if (stand != d->stand)
        return;
    if (gewichte.size() != d->klassen.size() * d->abdruckGroesse
            || bias.size() != d->klassen.size())
        return;
    d->kopf = gewichte;
    d->bias = bias;
    d->kopfStand = stand;
}

// ---- Lesen und Schreiben --------------------------------------------------

namespace {

/* Ein Eintrag aus dem Strom.  Gibt false, wenn der Strom nicht mehr
   hergibt, was das Format verspricht -- dann ist die Datei kaputt und wird
   nicht zur Haelfte uebernommen. */
bool eintragLesen(QDataStream &strom, Eintrag *e, int abdruckGroesse, int fassung)
{
    quint16 kennungLaenge = 0;
    strom >> kennungLaenge;
    if (strom.status() != QDataStream::Ok || kennungLaenge == 0 || kennungLaenge > 200)
        return false;
    QByteArray kennung(kennungLaenge, '\0');
    if (strom.readRawData(kennung.data(), kennungLaenge) != (int)kennungLaenge)
        return false;
    e->klasse = QString::fromUtf8(kennung);

    strom >> e->quelle >> e->sicherheit >> e->zeit;
    quint32 bildLaenge = 0;
    strom >> bildLaenge;
    if (strom.status() != QDataStream::Ok || bildLaenge == 0 || bildLaenge > 4u * 1024 * 1024)
        return false;
    e->bild.resize((int)bildLaenge);
    if (strom.readRawData(e->bild.data(), (int)bildLaenge) != (int)bildLaenge)
        return false;

    quint8 hatAbdruck = 0;
    strom >> hatAbdruck;
    if (strom.status() != QDataStream::Ok)
        return false;
    if (!hatAbdruck)
        return true;
    if (abdruckGroesse <= 0 || abdruckGroesse > 65536)
        return false;
    QByteArray roh(abdruckGroesse, '\0');
    if (strom.readRawData(roh.data(), abdruckGroesse) != abdruckGroesse)
        return false;
    float massstab = 0.f;
    strom >> massstab;
    if (fassung >= 2)
        strom >> e->laenge;
    if (strom.status() != QDataStream::Ok)
        return false;
    /* Fassung 1 (Segelflug 2.11, Sailfish 1.18.x) kannte die Laenge noch
       nicht, und ohne sie taugt der Abdruck nicht zum Nachtrainieren.
       Also keinen uebernehmen: brauchtNeurechnung() holt beides aus dem
       gespeicherten Bild nach, wie bei einem neuen Netz. */
    if (fassung < 2)
        return true;
    e->abdruck.resize(abdruckGroesse);
    const signed char *q = reinterpret_cast<const signed char *>(roh.constData());
    for (int j = 0; j < abdruckGroesse; ++j)
        e->abdruck[j] = (float)q[j] * massstab;
    return true;
}

void eintragSchreiben(QDataStream &strom, const Eintrag &e, int abdruckGroesse)
{
    const QByteArray kennung = e.klasse.toUtf8();
    strom << (quint16)kennung.size();
    strom.writeRawData(kennung.constData(), kennung.size());
    strom << e.quelle << e.sicherheit << e.zeit;
    strom << (quint32)e.bild.size();
    strom.writeRawData(e.bild.constData(), e.bild.size());

    if (e.abdruck.size() != abdruckGroesse || abdruckGroesse <= 0) {
        strom << (quint8)0;
        return;
    }
    strom << (quint8)1;
    float groesstes = 0.f;
    for (int j = 0; j < abdruckGroesse; ++j)
        groesstes = qMax(groesstes, qAbs(e.abdruck.at(j)));
    const float massstab = groesstes > 0.f ? groesstes / 127.f : 1.f;
    QByteArray roh(abdruckGroesse, '\0');
    signed char *z = reinterpret_cast<signed char *>(roh.data());
    for (int j = 0; j < abdruckGroesse; ++j) {
        const int q = (int)(e.abdruck.at(j) / massstab + (e.abdruck.at(j) >= 0 ? 0.5f : -0.5f));
        z[j] = (signed char)qBound(-127, q, 127);
    }
    strom.writeRawData(roh.constData(), abdruckGroesse);
    strom << massstab << e.laenge;
}

}

bool Nachlernen::laden(const QString &pfad, QString *fehler)
{
    d->eintraege.clear();
    d->verwerfen();
    if (!QFile::exists(pfad))
        return true;              // kein Gedaechtnis ist kein Fehler

    QList<Eintrag> gelesen;
    QString fingerabdruck;
    int groesse = 0;
    if (!datenLesen(pfad, &gelesen, &fingerabdruck, &groesse, fehler))
        return false;
    d->eintraege = gelesen;
    d->abdruckGroesse = groesse;
    /* Der Fingerabdruck aus der Datei; netzSetzen() verwirft die Abdruecke,
       wenn er nicht zum geladenen Netz passt. */
    d->fingerabdruck = fingerabdruck;
    d->verwerfen();
    return true;
}

bool Nachlernen::datenLesen(const QString &pfad, QList<Eintrag> *ziel,
                            QString *fingerabdruck, int *abdruckGroesse,
                            QString *fehler) const
{
    QFile datei(pfad);
    if (!datei.open(QIODevice::ReadOnly))
        return fehlschlag(fehler, QString::fromUtf8("%1 laesst sich nicht oeffnen")
                          .arg(QFileInfo(pfad).fileName()));
    QDataStream strom(&datei);
    strom.setByteOrder(QDataStream::LittleEndian);
    strom.setFloatingPointPrecision(QDataStream::SinglePrecision);

    char kennung[8];
    if (strom.readRawData(kennung, 8) != 8 || memcmp(kennung, kKennung, 8) != 0)
        return fehlschlag(fehler, QString::fromUtf8("keine Gelerntes-Datei"));
    quint32 kopfLaenge = 0;
    strom >> kopfLaenge;
    if (strom.status() != QDataStream::Ok || kopfLaenge == 0 || kopfLaenge > 1u << 20)
        return fehlschlag(fehler, QString::fromUtf8("Kopf abgeschnitten"));
    QByteArray kopfRoh((int)kopfLaenge, '\0');
    if (strom.readRawData(kopfRoh.data(), (int)kopfLaenge) != (int)kopfLaenge)
        return fehlschlag(fehler, QString::fromUtf8("Kopf abgeschnitten"));

    QString jsonFehler;
    const QVariant kopfWert = Json::parse(QString::fromUtf8(kopfRoh), &jsonFehler);
    if (!kopfWert.isValid())
        return fehlschlag(fehler, QString::fromUtf8("Kopf unlesbar: ") + jsonFehler);
    const QVariantMap kopf = kopfWert.toMap();
    const int fassung = kopf.value(QLatin1String("format")).toInt();
    if (fassung != 1 && fassung != 2)
        return fehlschlag(fehler, QString::fromUtf8("unbekannte Fassung des Formats"));
    *fingerabdruck = kopf.value(QLatin1String("modell")).toString();
    *abdruckGroesse = kopf.value(QLatin1String("abdruck")).toInt();
    const int erwartet = kopf.value(QLatin1String("eintraege")).toInt();

    ziel->clear();
    for (int i = 0; i < erwartet; ++i) {
        Eintrag e;
        if (!eintragLesen(strom, &e, *abdruckGroesse, fassung))
            return fehlschlag(fehler, QString::fromUtf8("Eintrag %1 von %2 ist kaputt")
                              .arg(i + 1).arg(erwartet));
        ziel->append(e);
    }
    return true;
}

bool Nachlernen::speichern(const QString &pfad, QString *fehler) const
{
    /* Erst daneben schreiben, dann umbenennen: Ein Absturz beim Schreiben
       darf nicht das Gelernte kosten. */
    const QString zwischen = pfad + QLatin1String(".neu");
    QFile datei(zwischen);
    if (!datei.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fehlschlag(fehler, QString::fromUtf8("%1 laesst sich nicht schreiben")
                          .arg(QFileInfo(zwischen).fileName()));

    QVariantMap kopf;
    kopf.insert(QLatin1String("format"), 2);
    kopf.insert(QLatin1String("klassen"), QVariant(d->klassen));
    kopf.insert(QLatin1String("modell"), d->fingerabdruck);
    kopf.insert(QLatin1String("abdruck"), d->abdruckGroesse);
    kopf.insert(QLatin1String("eintraege"), d->eintraege.size());
    kopf.insert(QLatin1String("geschrieben"),
                QDateTime::currentDateTime().toUTC().toString(QLatin1String("yyyy-MM-dd")));
    const QByteArray kopfRoh = Json::stringify(kopf).toUtf8();

    QDataStream strom(&datei);
    strom.setByteOrder(QDataStream::LittleEndian);
    strom.setFloatingPointPrecision(QDataStream::SinglePrecision);
    strom.writeRawData(kKennung, 8);
    strom << (quint32)kopfRoh.size();
    strom.writeRawData(kopfRoh.constData(), kopfRoh.size());
    for (int i = 0; i < d->eintraege.size(); ++i)
        eintragSchreiben(strom, d->eintraege.at(i), d->abdruckGroesse);
    datei.close();
    if (datei.error() != QFile::NoError)
        return fehlschlag(fehler, QString::fromUtf8("Schreiben fehlgeschlagen"));

    QFile::remove(pfad);
    if (!QFile::rename(zwischen, pfad))
        return fehlschlag(fehler, QString::fromUtf8("Umbenennen fehlgeschlagen"));
    return true;
}

int Nachlernen::dazuladen(const QString &pfad, QString *fehler)
{
    QList<Eintrag> fremd;
    QString fremderFinger;
    int fremdeGroesse = 0;
    if (!datenLesen(pfad, &fremd, &fremderFinger, &fremdeGroesse, fehler))
        return -1;

    /* Abdruecke aus einem anderen Netz sind wertlos; die Bilder bleiben, der
       Rest wird nachgerechnet. */
    const bool passt = (fremderFinger == d->fingerabdruck)
            && (d->abdruckGroesse <= 0 || fremdeGroesse == d->abdruckGroesse);
    int neu = 0;
    for (int i = 0; i < fremd.size(); ++i) {
        Eintrag e = fremd.at(i);
        bool schonDa = false;
        for (int j = 0; j < d->eintraege.size(); ++j)
            if (d->eintraege.at(j).bild == e.bild) {
                schonDa = true;
                break;
            }
        if (schonDa)
            continue;
        if (!passt)
            e.abdruck.clear();
        d->eintraege.append(e);
        ++neu;
    }
    if (passt && d->abdruckGroesse <= 0)
        d->abdruckGroesse = fremdeGroesse;
    d->verwerfen();
    return neu;
}
