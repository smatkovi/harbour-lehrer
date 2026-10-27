#ifndef KLASSIFIZIERER_H
#define KLASSIFIZIERER_H

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

/* Der Wolkenklassifizierer: ein kleines Faltungsnetz (MobileNetV2, Eingabe
   160x160), das data/modell.bin beschreibt -- das Format steht in
   segelflug/tools/MODELL.md.  Ganz ohne Fremdbibliothek, damit es auf
   Harmattan (Qt 4.7, GCC 14) und Sailfish (Qt 5) aus demselben Text
   uebersetzt.

   Nach laden() ist das Objekt unveraenderlich: bewerten() und logits()
   duerfen aus einem Arbeitsfaden gerufen werden, auch mehrfach
   nebeneinander. */
class Klassifizierer
{
public:
    Klassifizierer();
    ~Klassifizierer();

    bool laden(const QString &pfad, QString *fehler = 0);
    bool bereit() const;
    QStringList klassen() const;        // Kennungen in Ausgabereihenfolge
    int eingabeGroesse() const;         // Kantenlaenge der Eingabe, z.B. 160

    /* Ein beliebiges Bild: wird auf eingabeGroesse() x eingabeGroesse()
       gestaucht (nicht beschnitten), normalisiert und durchgerechnet.
       Ergebnis: Softmax-Wahrscheinlichkeiten je Klasse, Summe 1; leer,
       wenn kein Modell geladen ist oder das Bild leer. */
    QVector<float> bewerten(const QImage &bild) const;

    /* Fuer den Pruefstand: rohe Logits aus einem fertigen Puffer von
       eingabe*eingabe*3 Byte RGB, zeilenweise (HWC), ohne Polsterung. */
    QVector<float> logits(const unsigned char *rgb) const;

    /* Ein Foto von der Platte: mit EXIF-Drehung (Qt 4 kennt sie nicht,
       die Kamera schreibt sie aber) und beim Laden schon verkleinert --
       die Kamera liefert 8 MP, gebraucht wird eine Handvoll Pixel.  Die
       laengere Kante wird auf hoechstens `kante` Pixel gebracht. */
    static QImage bildLaden(const QString &pfad, int kante = 640,
                            QString *fehler = 0);

private:
    struct Daten;
    Daten *d;

    Klassifizierer(const Klassifizierer &);
    Klassifizierer &operator=(const Klassifizierer &);
};

#endif
