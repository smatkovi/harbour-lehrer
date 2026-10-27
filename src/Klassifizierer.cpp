#include "Klassifizierer.h"

#include "Json.h"

#include <QFile>
#include <QImageReader>
#include <QTransform>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>
#include <cstring>
#include <vector>

/* Das Format steht in segelflug/tools/MODELL.md; hier wird es nur
   gelesen.  Alles, was die Datei sagt, wird beim Laden nachgeprueft --
   die Datei kommt zwar aus dem eigenen Paket, aber ein Absturz im
   Bewertungsfaden waere auf dem Telefon kaum zu deuten.

   Zwei Dinge sind eigen:
   * Gewichte sind int8 mit einer Skala je Ausgabekanal.  Dequantisiert
     wird einmal beim Laden nach float, dann rechnet das Netz nur noch mit
     float -- auf dem Cortex-A8 ist jede Umwandlung im inneren Kreis teuer.
   * Zwischentensoren leben nur, solange eine spaetere Schicht sie nennt.
     Die Letztverwendung wird einmal beim Laden ausgerechnet; die N950 hat
     wenig Speicher, und der Faden soll nicht alle Tensoren zugleich halten. */

namespace {

enum Art { ArtConv, ArtAdd, ArtMittel, ArtLinear };

struct Form {
    int c, h, w;                    // h == w == 0: flacher Vektor [c]
    Form() : c(0), h(0), w(0) {}
    Form(int c_, int h_, int w_) : c(c_), h(h_), w(w_) {}
    int groesse() const { return (h == 0 && w == 0) ? c : c * h * w; }
    bool operator==(const Form &o) const { return c == o.c && h == o.h && w == o.w; }
};

struct Schicht {
    Art art;
    int quelle, quelle2;            // quelle2 nur bei add
    int ein, aus, kern, schritt, rand, gruppen;
    bool relu6;
    std::vector<float> gewichte;    // dequantisiert, Layout wie in der Datei
    std::vector<float> bias;
    Schicht() : art(ArtConv), quelle(0), quelle2(0), ein(0), aus(0), kern(1),
                schritt(1), rand(0), gruppen(1), relu6(false) {}
};

inline float relu6(float x)
{
    return x < 0.f ? 0.f : (x > 6.f ? 6.f : x);
}

float leseFloat(const unsigned char *p)
{
    /* memcpy statt Zeigerumdeutung: der Blob liegt in einem QByteArray,
       und ein unausgerichteter float-Zugriff ist auf dem Cortex-A8 ein
       Busfehler, kein langsamer Zugriff. */
    float f;
    std::memcpy(&f, p, 4);
    return f;
}

unsigned int leseU32LE(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

/* Die Faltung in drei Fassungen.  Die beiden schnellen decken MobileNetV2
   fast ganz ab (1x1-Projektion/-Erweiterung und tiefenweise 3x3); die
   allgemeine bleibt fuer die erste Schicht (3->32, 3x3, Schritt 2) und
   fuer alles, was ein spaeteres Modell sonst noch mitbringt. */

void faltung1x1(const Schicht &s, const float *ein, int hw, float *aus)
{
    /* Matrixprodukt [aus x ein] * [ein x HW].  Die innere Schleife laeuft
       ueber die Flaeche, beide Zeiger zusammenhaengend; der Compiler
       vektorisiert das auch ohne Intrinsics. */
    for (int o = 0; o < s.aus; ++o) {
        float *z = aus + (size_t)o * hw;
        const float b = s.bias[o];
        for (int p = 0; p < hw; ++p)
            z[p] = b;
        const float *w = &s.gewichte[(size_t)o * s.ein];
        for (int i = 0; i < s.ein; ++i) {
            const float wi = w[i];
            const float *q = ein + (size_t)i * hw;
            for (int p = 0; p < hw; ++p)
                z[p] += wi * q[p];
        }
        if (s.relu6)
            for (int p = 0; p < hw; ++p)
                z[p] = relu6(z[p]);
    }
}

void faltungTiefenweise3x3(const Schicht &s, const float *ein, int h, int w,
                           float *aus, int ah, int aw)
{
    const int st = s.schritt, rd = s.rand;
    /* Spaltenbereich, in dem alle neun Abgriffe im Bild liegen -- dort
       laeuft die Schleife ohne jede Pruefung; die Raender davor und
       danach bekommen die langsame Fassung mit Grenzpruefung. */
    int xA = (rd + st - 1) / st;                        // erster innerer ox
    int xB = (w - 3 + rd >= 0) ? (w - 3 + rd) / st : -1; // letzter innerer ox
    if (xB >= aw) xB = aw - 1;
    if (xA > aw) xA = aw;
    for (int c = 0; c < s.aus; ++c) {
        const float *k = &s.gewichte[(size_t)c * 9];
        const float *q = ein + (size_t)c * h * w;
        float *z = aus + (size_t)c * ah * aw;
        const float b = s.bias[c];
        for (int oy = 0; oy < ah; ++oy) {
            const int iy0 = oy * st - rd;
            const bool innenY = iy0 >= 0 && iy0 + 2 < h;
            float *zz = z + (size_t)oy * aw;
            for (int ox = 0; ox < aw; ++ox) {
                const int ix0 = ox * st - rd;
                float summe = b;
                if (innenY && ox >= xA && ox <= xB) {
                    const float *r0 = q + (size_t)iy0 * w + ix0;
                    const float *r1 = r0 + w;
                    const float *r2 = r1 + w;
                    summe += k[0] * r0[0] + k[1] * r0[1] + k[2] * r0[2]
                           + k[3] * r1[0] + k[4] * r1[1] + k[5] * r1[2]
                           + k[6] * r2[0] + k[7] * r2[1] + k[8] * r2[2];
                } else {
                    for (int ky = 0; ky < 3; ++ky) {
                        const int iy = iy0 + ky;
                        if (iy < 0 || iy >= h) continue;
                        for (int kx = 0; kx < 3; ++kx) {
                            const int ix = ix0 + kx;
                            if (ix < 0 || ix >= w) continue;
                            summe += k[ky * 3 + kx] * q[(size_t)iy * w + ix];
                        }
                    }
                }
                zz[ox] = s.relu6 ? relu6(summe) : summe;
            }
        }
    }
}

void faltungAllgemein(const Schicht &s, const float *ein, int h, int w,
                      float *aus, int ah, int aw)
{
    const int einJeGruppe = s.ein / s.gruppen;
    const int ausJeGruppe = s.aus / s.gruppen;
    const int k = s.kern;
    for (int o = 0; o < s.aus; ++o) {
        const int g = o / ausJeGruppe;
        float *z = aus + (size_t)o * ah * aw;
        for (int oy = 0; oy < ah; ++oy) {
            for (int ox = 0; ox < aw; ++ox) {
                float summe = s.bias[o];
                for (int ci = 0; ci < einJeGruppe; ++ci) {
                    const int c = g * einJeGruppe + ci;
                    const float *q = ein + (size_t)c * h * w;
                    const float *kw = &s.gewichte[((size_t)o * einJeGruppe + ci) * k * k];
                    for (int ky = 0; ky < k; ++ky) {
                        const int iy = oy * s.schritt - s.rand + ky;
                        if (iy < 0 || iy >= h) continue;
                        for (int kx = 0; kx < k; ++kx) {
                            const int ix = ox * s.schritt - s.rand + kx;
                            if (ix < 0 || ix >= w) continue;
                            summe += kw[ky * k + kx] * q[(size_t)iy * w + ix];
                        }
                    }
                }
                z[(size_t)oy * aw + ox] = s.relu6 ? relu6(summe) : summe;
            }
        }
    }
}

}

struct Klassifizierer::Daten
{
    bool bereit;
    int eingabe;
    float faktor[3], versatz[3];    // Normalisierung: wert = byte*faktor + versatz
    QStringList klassen;
    std::vector<Schicht> schichten;
    std::vector<Form> formen;       // je Tensor, Index 0 = Eingabe
    std::vector<int> letzteNutzung; // je Tensor: Index der letzten Schicht, die ihn liest

    Daten() : bereit(false), eingabe(0)
    {
        for (int c = 0; c < 3; ++c) { faktor[c] = 0.f; versatz[c] = 0.f; }
    }

    void leeren()
    {
        bereit = false;
        eingabe = 0;
        klassen.clear();
        std::vector<Schicht>().swap(schichten);
        std::vector<Form>().swap(formen);
        std::vector<int>().swap(letzteNutzung);
    }

    bool kopfLesen(const QVariantMap &kopf, const unsigned char *blob,
                   qint64 blobLaenge, QString *fehler);
    bool tensorHolen(const QVariantMap &m, const char *schluessel,
                     const unsigned char *blob, qint64 blobLaenge,
                     qint64 laenge, const unsigned char **ziel,
                     QString *fehler, int schicht) const;
    QVector<float> rechnen(const std::vector<float> &eingabeTensor) const;
};

namespace {

bool fehlschlag(QString *fehler, const QString &text)
{
    if (fehler)
        *fehler = text;
    return false;
}

QString schichtText(int i, const QString &was)
{
    return QString::fromUtf8("Schicht %1: %2").arg(i).arg(was);
}

}

bool Klassifizierer::Daten::tensorHolen(const QVariantMap &m, const char *schluessel,
                                        const unsigned char *blob, qint64 blobLaenge,
                                        qint64 laenge, const unsigned char **ziel,
                                        QString *fehler, int schicht) const
{
    if (!m.contains(QLatin1String(schluessel)))
        return fehlschlag(fehler, schichtText(schicht, QString::fromUtf8("%1 fehlt").arg(QLatin1String(schluessel))));
    const qint64 versatz = (qint64)m.value(QLatin1String(schluessel)).toDouble();
    if (versatz < 0 || laenge < 0 || versatz > blobLaenge || laenge > blobLaenge - versatz)
        return fehlschlag(fehler, schichtText(schicht, QString::fromUtf8("%1 liegt ausserhalb des Blobs (%2+%3 > %4)")
                                               .arg(QLatin1String(schluessel)).arg(versatz).arg(laenge).arg(blobLaenge)));
    *ziel = blob + versatz;
    return true;
}

bool Klassifizierer::Daten::kopfLesen(const QVariantMap &kopf, const unsigned char *blob,
                                      qint64 blobLaenge, QString *fehler)
{
    if (kopf.value(QLatin1String("format")).toInt() != 1)
        return fehlschlag(fehler, QString::fromUtf8("unbekanntes Format %1")
                          .arg(kopf.value(QLatin1String("format")).toString()));
    eingabe = kopf.value(QLatin1String("eingabe")).toInt();
    if (eingabe <= 0 || eingabe > 4096)
        return fehlschlag(fehler, QString::fromUtf8("eingabe unbrauchbar"));

    const QVariantList mittel = kopf.value(QLatin1String("mittel")).toList();
    const QVariantList streuung = kopf.value(QLatin1String("streuung")).toList();
    if (mittel.size() != 3 || streuung.size() != 3)
        return fehlschlag(fehler, QString::fromUtf8("mittel/streuung brauchen drei Werte"));
    for (int c = 0; c < 3; ++c) {
        const double s = streuung.at(c).toDouble();
        if (!(s > 0.0))
            return fehlschlag(fehler, QString::fromUtf8("streuung muss positiv sein"));
        /* (x/255 - m)/s als eine Multiplikation und eine Addition je Pixel. */
        faktor[c] = (float)(1.0 / (255.0 * s));
        versatz[c] = (float)(-mittel.at(c).toDouble() / s);
    }

    const QVariantList klassenListe = kopf.value(QLatin1String("klassen")).toList();
    for (int i = 0; i < klassenListe.size(); ++i) {
        const QString k = klassenListe.at(i).toString();
        if (k.isEmpty())
            return fehlschlag(fehler, QString::fromUtf8("leere Klassenkennung"));
        klassen.append(k);
    }
    if (klassen.isEmpty())
        return fehlschlag(fehler, QString::fromUtf8("keine Klassen"));

    const QVariantList liste = kopf.value(QLatin1String("schichten")).toList();
    if (liste.isEmpty())
        return fehlschlag(fehler, QString::fromUtf8("keine Schichten"));

    formen.push_back(Form(3, eingabe, eingabe));
    for (int i = 0; i < liste.size(); ++i) {
        const QVariantMap m = liste.at(i).toMap();
        const QString art = m.value(QLatin1String("art")).toString();
        Schicht s;
        Form ergebnis;
        const int tensoren = (int)formen.size();

        if (art == QLatin1String("add")) {
            s.art = ArtAdd;
            const QVariantList q = m.value(QLatin1String("quellen")).toList();
            if (q.size() != 2)
                return fehlschlag(fehler, schichtText(i, QString::fromUtf8("add braucht zwei Quellen")));
            s.quelle = q.at(0).toInt();
            s.quelle2 = q.at(1).toInt();
            if (s.quelle < 0 || s.quelle >= tensoren || s.quelle2 < 0 || s.quelle2 >= tensoren)
                return fehlschlag(fehler, schichtText(i, QString::fromUtf8("Quelle ausserhalb")));
            if (!(formen[s.quelle] == formen[s.quelle2]))
                return fehlschlag(fehler, schichtText(i, QString::fromUtf8("add mit ungleichen Formen")));
            ergebnis = formen[s.quelle];
        } else {
            s.quelle = m.value(QLatin1String("quelle"), QVariant(-1)).toInt();
            if (s.quelle < 0 || s.quelle >= tensoren)
                return fehlschlag(fehler, schichtText(i, QString::fromUtf8("Quelle ausserhalb")));
            const Form &q = formen[s.quelle];

            if (art == QLatin1String("mittel")) {
                s.art = ArtMittel;
                if (q.h == 0)
                    return fehlschlag(fehler, schichtText(i, QString::fromUtf8("mittel braucht eine Flaeche")));
                ergebnis = Form(q.c, 0, 0);
            } else if (art == QLatin1String("conv") || art == QLatin1String("linear")) {
                s.ein = m.value(QLatin1String("ein")).toInt();
                s.aus = m.value(QLatin1String("aus")).toInt();
                if (s.ein <= 0 || s.aus <= 0 || s.ein != q.c)
                    return fehlschlag(fehler, schichtText(i, QString::fromUtf8("ein/aus passen nicht zur Quelle (%1 Kanaele)").arg(q.c)));
                qint64 gewichteZahl;
                if (art == QLatin1String("conv")) {
                    s.art = ArtConv;
                    s.kern = m.value(QLatin1String("kern"), QVariant(1)).toInt();
                    s.schritt = m.value(QLatin1String("schritt"), QVariant(1)).toInt();
                    s.rand = m.value(QLatin1String("rand"), QVariant(0)).toInt();
                    s.gruppen = m.value(QLatin1String("gruppen"), QVariant(1)).toInt();
                    s.relu6 = m.value(QLatin1String("relu6"), QVariant(false)).toBool();
                    if (q.h == 0)
                        return fehlschlag(fehler, schichtText(i, QString::fromUtf8("conv braucht eine Flaeche")));
                    if (s.kern <= 0 || s.schritt <= 0 || s.rand < 0 || s.gruppen <= 0
                        || s.ein % s.gruppen != 0 || s.aus % s.gruppen != 0)
                        return fehlschlag(fehler, schichtText(i, QString::fromUtf8("Kern/Schritt/Rand/Gruppen unbrauchbar")));
                    const int ah = (q.h + 2 * s.rand - s.kern) / s.schritt + 1;
                    const int aw = (q.w + 2 * s.rand - s.kern) / s.schritt + 1;
                    if (q.h + 2 * s.rand < s.kern || ah <= 0 || aw <= 0)
                        return fehlschlag(fehler, schichtText(i, QString::fromUtf8("Kern groesser als die Quelle")));
                    ergebnis = Form(s.aus, ah, aw);
                    gewichteZahl = (qint64)s.aus * (s.ein / s.gruppen) * s.kern * s.kern;
                } else {
                    s.art = ArtLinear;
                    if (q.h != 0)
                        return fehlschlag(fehler, schichtText(i, QString::fromUtf8("linear braucht einen flachen Vektor")));
                    ergebnis = Form(s.aus, 0, 0);
                    gewichteZahl = (qint64)s.aus * s.ein;
                }

                const unsigned char *g = 0, *sk = 0, *b = 0;
                if (!tensorHolen(m, "gewichte", blob, blobLaenge, gewichteZahl, &g, fehler, i)
                    || !tensorHolen(m, "skalen", blob, blobLaenge, (qint64)s.aus * 4, &sk, fehler, i)
                    || !tensorHolen(m, "bias", blob, blobLaenge, (qint64)s.aus * 4, &b, fehler, i))
                    return false;

                /* Dequantisieren: w = q * skala[aus].  int8 ausdruecklich als
                   signed char -- auf ARM ist char ohne Vorzeichen. */
                s.gewichte.resize((size_t)gewichteZahl);
                s.bias.resize(s.aus);
                const qint64 jeAus = gewichteZahl / s.aus;
                const signed char *q8 = reinterpret_cast<const signed char *>(g);
                for (int o = 0; o < s.aus; ++o) {
                    const float skala = leseFloat(sk + 4 * o);
                    s.bias[o] = leseFloat(b + 4 * o);
                    float *ziel = &s.gewichte[(size_t)o * jeAus];
                    const signed char *quelle = q8 + (qint64)o * jeAus;
                    for (qint64 n = 0; n < jeAus; ++n)
                        ziel[n] = (float)quelle[n] * skala;
                }
            } else {
                return fehlschlag(fehler, schichtText(i, QString::fromUtf8("unbekannte Art \"%1\"").arg(art)));
            }
        }
        schichten.push_back(s);
        formen.push_back(ergebnis);
    }

    const Form &letzte = formen.back();
    if (letzte.h != 0 || letzte.c != klassen.size())
        return fehlschlag(fehler, QString::fromUtf8("Ausgabe (%1) passt nicht zur Klassenzahl (%2)")
                          .arg(letzte.groesse()).arg(klassen.size()));

    /* Letztverwendung: die Eingabe und das Ergebnis zaehlen nicht mit. */
    letzteNutzung.assign(formen.size(), -1);
    for (size_t i = 0; i < schichten.size(); ++i) {
        letzteNutzung[schichten[i].quelle] = (int)i;
        if (schichten[i].art == ArtAdd)
            letzteNutzung[schichten[i].quelle2] = (int)i;
    }
    return true;
}

QVector<float> Klassifizierer::Daten::rechnen(const std::vector<float> &eingabeTensor) const
{
    std::vector< std::vector<float> > tensoren(formen.size());
    tensoren[0] = eingabeTensor;

    for (size_t i = 0; i < schichten.size(); ++i) {
        const Schicht &s = schichten[i];
        const Form &qf = formen[s.quelle];
        const Form &zf = formen[i + 1];
        const std::vector<float> &q = tensoren[s.quelle];
        std::vector<float> &z = tensoren[i + 1];
        z.resize((size_t)zf.groesse());

        switch (s.art) {
        case ArtConv:
            if (s.kern == 1 && s.schritt == 1 && s.rand == 0 && s.gruppen == 1)
                faltung1x1(s, &q[0], qf.h * qf.w, &z[0]);
            else if (s.kern == 3 && s.gruppen == s.ein && s.gruppen == s.aus)
                faltungTiefenweise3x3(s, &q[0], qf.h, qf.w, &z[0], zf.h, zf.w);
            else
                faltungAllgemein(s, &q[0], qf.h, qf.w, &z[0], zf.h, zf.w);
            break;
        case ArtAdd: {
            const std::vector<float> &q2 = tensoren[s.quelle2];
            const size_t n = z.size();
            for (size_t p = 0; p < n; ++p)
                z[p] = q[p] + q2[p];
            break;
        }
        case ArtMittel: {
            const int hw = qf.h * qf.w;
            const float teiler = 1.f / (float)hw;
            for (int c = 0; c < qf.c; ++c) {
                const float *p = &q[(size_t)c * hw];
                float summe = 0.f;
                for (int k = 0; k < hw; ++k)
                    summe += p[k];
                z[c] = summe * teiler;
            }
            break;
        }
        case ArtLinear:
            for (int o = 0; o < s.aus; ++o) {
                const float *w = &s.gewichte[(size_t)o * s.ein];
                float summe = s.bias[o];
                for (int k = 0; k < s.ein; ++k)
                    summe += w[k] * q[k];
                z[o] = summe;
            }
            break;
        }

        /* Was keine spaetere Schicht mehr liest, geht sofort weg. */
        for (size_t t = 0; t < tensoren.size(); ++t)
            if (letzteNutzung[t] == (int)i)
                std::vector<float>().swap(tensoren[t]);
    }

    const std::vector<float> &ergebnis = tensoren.back();
    QVector<float> aus((int)ergebnis.size());
    for (int k = 0; k < aus.size(); ++k)
        aus[k] = ergebnis[(size_t)k];
    return aus;
}

Klassifizierer::Klassifizierer()
    : d(new Daten)
{
}

Klassifizierer::~Klassifizierer()
{
    delete d;
}

bool Klassifizierer::laden(const QString &pfad, QString *fehler)
{
    d->leeren();

    QFile datei(pfad);
    if (!datei.open(QIODevice::ReadOnly))
        return fehlschlag(fehler, QString::fromUtf8("%1 laesst sich nicht oeffnen").arg(pfad));
    const QByteArray inhalt = datei.readAll();
    datei.close();

    if (inhalt.size() < 12)
        return fehlschlag(fehler, QString::fromUtf8("Datei zu kurz"));
    const unsigned char *roh = reinterpret_cast<const unsigned char *>(inhalt.constData());
    if (std::memcmp(roh, "WOLKEN01", 8) != 0)
        return fehlschlag(fehler, QString::fromUtf8("keine Modelldatei (Kennung fehlt)"));
    const qint64 kopfLaenge = leseU32LE(roh + 8);
    if (kopfLaenge <= 0 || kopfLaenge > (qint64)inhalt.size() - 12)
        return fehlschlag(fehler, QString::fromUtf8("Kopf abgeschnitten"));

    QString jsonFehler;
    const QVariant kopfWert = Json::parse(QString::fromUtf8(inhalt.constData() + 12, (int)kopfLaenge), &jsonFehler);
    if (kopfWert.type() != QVariant::Map)
        return fehlschlag(fehler, QString::fromUtf8("Kopf unlesbar: %1").arg(jsonFehler));
    const QVariantMap kopf = kopfWert.toMap();

    const qint64 blobLaenge = (qint64)inhalt.size() - 12 - kopfLaenge;
    const qint64 erwartet = (qint64)kopf.value(QLatin1String("bytes")).toDouble();
    if (erwartet != blobLaenge)
        return fehlschlag(fehler, QString::fromUtf8("Blob hat %1 Byte, der Kopf nennt %2").arg(blobLaenge).arg(erwartet));

    if (!d->kopfLesen(kopf, roh + 12 + kopfLaenge, blobLaenge, fehler)) {
        d->leeren();
        return false;
    }
    d->bereit = true;
    return true;
}

bool Klassifizierer::bereit() const
{
    return d->bereit;
}

QStringList Klassifizierer::klassen() const
{
    return d->klassen;
}

int Klassifizierer::eingabeGroesse() const
{
    return d->eingabe;
}

QVector<float> Klassifizierer::logits(const unsigned char *rgb) const
{
    if (!d->bereit || !rgb)
        return QVector<float>();
    const int e = d->eingabe;
    const size_t flaeche = (size_t)e * e;
    std::vector<float> tensor(3 * flaeche);
    /* HWC-Bytes nach CHW-float, mit der Normalisierung aus dem Kopf. */
    for (int c = 0; c < 3; ++c) {
        const float f = d->faktor[c], v = d->versatz[c];
        float *ziel = &tensor[c * flaeche];
        const unsigned char *quelle = rgb + c;
        for (size_t p = 0; p < flaeche; ++p)
            ziel[p] = (float)quelle[p * 3] * f + v;
    }
    return d->rechnen(tensor);
}

QVector<float> Klassifizierer::bewerten(const QImage &bild) const
{
    if (!d->bereit || bild.isNull())
        return QVector<float>();
    const int e = d->eingabe;
    /* Stauchen, nicht beschneiden -- so hat es auch das Training gesehen
       (MODELL.md, Vorverarbeitung). */
    QImage klein = bild.convertToFormat(QImage::Format_RGB32)
                       .scaled(e, e, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (klein.isNull() || klein.width() != e || klein.height() != e)
        return QVector<float>();

    std::vector<unsigned char> rgb((size_t)e * e * 3);
    for (int y = 0; y < e; ++y) {
        const QRgb *zeile = reinterpret_cast<const QRgb *>(klein.constScanLine(y));
        unsigned char *ziel = &rgb[(size_t)y * e * 3];
        for (int x = 0; x < e; ++x) {
            const QRgb p = zeile[x];
            ziel[3 * x] = (unsigned char)qRed(p);
            ziel[3 * x + 1] = (unsigned char)qGreen(p);
            ziel[3 * x + 2] = (unsigned char)qBlue(p);
        }
    }

    QVector<float> l = logits(&rgb[0]);
    if (l.isEmpty())
        return l;
    /* Softmax, numerisch stabil: das Maximum abziehen, sonst laeuft exp()
       bei grossen Logits ueber. */
    float groesstes = l[0];
    for (int i = 1; i < l.size(); ++i)
        if (l[i] > groesstes) groesstes = l[i];
    double summe = 0.0;
    for (int i = 0; i < l.size(); ++i) {
        l[i] = (float)std::exp((double)(l[i] - groesstes));
        summe += l[i];
    }
    for (int i = 0; i < l.size(); ++i)
        l[i] = (float)(l[i] / summe);
    return l;
}

namespace {

/* Das Orientation-Tag (0x0112) aus dem EXIF-Block eines JPEG.  Qt 4 kennt
   kein EXIF, Qt 5.6 dreht nur mit setAutoTransform -- also selbst lesen,
   auf beiden Plattformen gleich.  0, wenn nichts da ist. */
int exifOrientierung(const QString &pfad)
{
    QFile datei(pfad);
    if (!datei.open(QIODevice::ReadOnly))
        return 0;
    /* APP1 steht ganz vorn; 64 KB decken jeden Marker vor dem Bild ab. */
    const QByteArray kopf = datei.read(65536);
    datei.close();
    const unsigned char *p = reinterpret_cast<const unsigned char *>(kopf.constData());
    const int n = kopf.size();
    if (n < 4 || p[0] != 0xFF || p[1] != 0xD8)
        return 0;
    int pos = 2;
    while (pos + 4 <= n) {
        if (p[pos] != 0xFF)
            return 0;
        const int marker = p[pos + 1];
        if (marker == 0xFF) { ++pos; continue; }     // Fuellbytes vor einem Marker
        if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
            pos += 2;                                 // Marker ohne Laenge
            continue;
        }
        if (marker == 0xDA || marker == 0xD9)
            return 0;                                 // Bilddaten: kein EXIF mehr
        const int laenge = (p[pos + 2] << 8) | p[pos + 3];
        if (laenge < 2 || pos + 2 + laenge > n)
            return 0;
        if (marker == 0xE1 && laenge >= 2 + 6 + 8
            && std::memcmp(p + pos + 4, "Exif\0\0", 6) == 0) {
            const unsigned char *t = p + pos + 10;    // TIFF-Kopf
            const int tn = laenge - 8;
            bool gross;                               // Byte-Reihenfolge: MM = Big Endian
            if (t[0] == 'I' && t[1] == 'I') gross = false;
            else if (t[0] == 'M' && t[1] == 'M') gross = true;
            else return 0;
            #define EXIF16(q) (gross ? (((q)[0] << 8) | (q)[1]) : (((q)[1] << 8) | (q)[0]))
            #define EXIF32(q) (gross ? (((unsigned)(q)[0] << 24) | ((q)[1] << 16) | ((q)[2] << 8) | (q)[3]) \
                                     : (((unsigned)(q)[3] << 24) | ((q)[2] << 16) | ((q)[1] << 8) | (q)[0]))
            if (EXIF16(t + 2) != 0x2A)
                return 0;
            const unsigned ifd = EXIF32(t + 4);
            if (ifd + 2 > (unsigned)tn)
                return 0;
            const int eintraege = EXIF16(t + ifd);
            for (int i = 0; i < eintraege; ++i) {
                const unsigned e = ifd + 2 + 12 * (unsigned)i;
                if (e + 12 > (unsigned)tn)
                    return 0;
                if (EXIF16(t + e) == 0x0112) {
                    if (EXIF16(t + e + 2) != 3)       // SHORT
                        return 0;
                    const int wert = EXIF16(t + e + 8);
                    return (wert >= 1 && wert <= 8) ? wert : 0;
                }
            }
            #undef EXIF16
            #undef EXIF32
            return 0;
        }
        pos += 2 + laenge;
    }
    return 0;
}

}

QImage Klassifizierer::bildLaden(const QString &pfad, int kante, QString *fehler)
{
    QImageReader leser(pfad);
    const QSize voll = leser.size();
    if (kante > 0 && voll.isValid() && (voll.width() > kante || voll.height() > kante)) {
        /* Seitenverhaeltnis selbst rechnen; der JPEG-Leser kann dann mit
           grobem DCT-Mass verkleinern, statt 8 MP ganz zu entpacken. */
        QSize ziel = voll;
        ziel.scale(kante, kante, Qt::KeepAspectRatio);
        if (ziel.width() < 1) ziel.setWidth(1);
        if (ziel.height() < 1) ziel.setHeight(1);
        leser.setScaledSize(ziel);
    }
    QImage bild = leser.read();
    if (bild.isNull()) {
        if (fehler)
            *fehler = leser.errorString();
        return bild;
    }
    if (kante > 0 && (bild.width() > kante || bild.height() > kante))
        bild = bild.scaled(kante, kante, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QTransform t;
    switch (exifOrientierung(pfad)) {
    case 2: t.scale(-1, 1); break;
    case 3: t.rotate(180); break;
    case 4: t.scale(1, -1); break;
    case 5: t.scale(-1, 1); t.rotate(90); break;   // Transposition
    case 6: t.rotate(90); break;                   // Kamera nach rechts gekippt
    case 7: t.rotate(90); t.scale(-1, 1); break;   // Transversion
    case 8: t.rotate(-90); break;
    default: return bild;
    }
    return bild.transformed(t);
}
