#ifndef NACHLERNEN_H
#define NACHLERNEN_H

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

/* Was die App dazugelernt hat -- das Format steht in
   segelflug/tools/GELERNTES.md, Pflichtlektuere vor jeder Aenderung.

   Das Netz selbst bleibt fest; gelernt wird im Merkmalsraum. Zu jedem Foto,
   das der Leser deuten liess und bei dem sich Claude und sein eigener Tipp
   einigten, haelt diese Klasse den 1280er-Abdruck des Netzes, die Klasse
   und das gestauchte Bild. Daraus rechnet `korrigieren()` einen Zuschlag
   auf die Wahrscheinlichkeiten des Netzes.

   Die Datei ist uebertragbar: Sie traegt die Bilder mit, also kann eine
   spaetere Fassung der App die Abdruecke mit ihrem eigenen Netz neu
   rechnen (`brauchtNeurechnung()`, `abdruckSetzen()`). */
struct Eintrag;      // ein gemerktes Foto; der Aufbau gehoert der .cpp

class Nachlernen
{
public:
    // Quelle eines Eintrags -- wer die Klasse gesagt hat.
    enum Quelle { Einig = 0, Claude = 1, Leser = 2 };

    Nachlernen();
    ~Nachlernen();

    /* Laden und Speichern. laden() gibt auch dann true, wenn die Datei
       fehlt -- ein leeres Gedaechtnis ist kein Fehler; false nur bei
       kaputter Datei (dann sagt *fehler, was los ist, und die Datei wird
       nicht ueberschrieben, bevor der Leser es weiss). */
    bool laden(const QString &pfad, QString *fehler = 0);
    bool speichern(const QString &pfad, QString *fehler = 0) const;

    /* Wessen Abdruecke drinstehen. Passt der Fingerabdruck nicht zum
       geladenen Netz, sind sie wertlos und muessen neu gerechnet werden. */
    void netzSetzen(const QString &fingerabdruck, const QStringList &klassen);
    QStringList brauchtNeurechnung() const;   // Bilder, deren Abdruck fehlt
    void abdruckSetzen(int nummer, const QVector<float> &abdruck, float laenge);

    int anzahl() const;
    int anzahlJeKlasse(const QString &klasse) const;
    /* Wie viele Eintraege wirken koennen: mit Abdruck und mit einer Klasse,
       die das geladene Netz kennt. */
    int nutzbare() const;
    bool wirksam() const;        // genug Eintraege, dass korrigiert wird
    int fehlende() const;        // wie viele noch bis dahin

    /* Das gemerkte Bild eines Eintrags (JPEG).  Die Oberflaeche braucht
       einen Pfad, nicht die Bytes -- Course schreibt es dafuer in den
       Zwischenspeicher. */
    QByteArray bildDaten(int nummer) const;

    /* Die Eintraege fuer die Oberflaeche, das Neueste zuerst: {nummer,
       klasse, quelle, sicherheit, zeit, bekannt, gerechnet}.  Das Bild
       holt sich der Aufrufer ueber bildDaten(nummer). */
    QVariantList eintraege() const;

    /* Merken. `bild` ist das gestauchte Netzbild (eingabe x eingabe), nicht
       das Originalfoto; `abdruck` sein Merkmalsvektor (normiert) und
       `laenge` dessen Laenge vor dem Normieren. */
    void merken(const QString &klasse, Quelle quelle, int sicherheit,
                const QImage &bild, const QVector<float> &abdruck, float laenge);

    /* Die letzte Schicht des Netzes nachtrainieren -- damit wird das Netz
       wirklich besser, nicht nur korrigiert (GELERNTES.md, "Die Korrektur").
       kopfSetzen() bekommt die ausgelieferte Schicht; kopfNachtrainieren()
       rechnet aus den Eintraegen eine neue (ein paar hundert
       Matrixprodukte, darf in einem Arbeitsfaden laufen, liest nur), und
       kopfUebernehmen() setzt sie ein -- mit dem Stand, zu dem sie
       gehoert: ist inzwischen etwas dazugekommen, wird sie verworfen. */
    void kopfSetzen(const QVector<float> &gewichte, const QVector<float> &bias);
    bool kopfLohnt() const;             // genug Eintraege dafuer?
    int stand() const;                  // zaehlt jede Aenderung
    bool kopfNachtrainieren(QVector<float> *gewichte, QVector<float> *bias) const;
    void kopfUebernehmen(const QVector<float> &gewichte, const QVector<float> &bias, int stand);
    bool kopfAktiv() const;             // rechnet die Korrektur mit der neuen Schicht?
    bool vergessen(int nummer);
    void alleVergessen();

    /* Die Korrektur: Netzwahrscheinlichkeiten hinein, korrigierte heraus
       (Summe 1).  Mit nachtrainierter Schicht ersetzt sie zuerst die
       Wahrscheinlichkeiten durch die der neuen Schicht, dann kommt das
       Gedaechtnis darueber.  Ohne genug Eintraege kommt die Eingabe
       unveraendert zurueck -- der Aufrufer braucht wirksam() nicht zu
       pruefen. */
    QVector<float> korrigieren(const QVector<float> &wahrscheinlich,
                               const QVector<float> &abdruck) const;

    /* Zusammenfuehren statt ersetzen: Eintraege aus einer anderen Datei
       dazunehmen, Doppelte (gleiches Bild) ueberspringen. Gibt zurueck,
       wie viele neu sind, oder -1 bei kaputter Datei. */
    int dazuladen(const QString &pfad, QString *fehler = 0);

private:
    struct Daten;
    Daten *d;

    bool datenLesen(const QString &pfad, QList<Eintrag> *ziel,
                    QString *fingerabdruck, int *abdruckGroesse,
                    QString *fehler) const;

    Nachlernen(const Nachlernen &);
    Nachlernen &operator=(const Nachlernen &);
};

#endif
