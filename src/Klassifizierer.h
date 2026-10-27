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

    /* Dasselbe, aber zusaetzlich mit dem Merkmalsvektor (den 1280 Zahlen
       vor der letzten Schicht, dem `mittel`-Tensor aus MODELL.md). Er ist
       der Fingerabdruck des Bildes und die Grundlage des Nachlernens
       (GELERNTES.md); `abdruck` wird auf Laenge 1 normiert, und `laenge`
       bekommt, wenn gesetzt, die Laenge vor dem Normieren. */
    QVector<float> bewerten(const QImage &bild, QVector<float> *abdruck,
                            float *laenge = 0) const;
    int abdruckGroesse() const;

    /* Die ausgelieferte letzte Schicht: Gewichte [aus][ein] flach und Bias
       [aus].  Das Nachlernen trainiert sie weiter (GELERNTES.md); das Netz
       selbst behaelt seine. */
    QVector<float> kopfGewichte() const;
    QVector<float> kopfBias() const;

    /* Das Bild, wie das Netz es sieht: auf eingabeGroesse() gestaucht,
       RGB32. Das Nachlernen legt genau dieses in seine Datei, damit eine
       spaetere Fassung die Abdruecke neu rechnen kann. */
    QImage netzBild(const QImage &bild) const;

    /* Fingerabdruck der geladenen Gewichte (SHA-1 ueber den Blob, hex).
       Steht im Kopf von gelerntes.bin: Passt er nicht, sind die dort
       gespeicherten Abdruecke wertlos. */
    QString fingerabdruck() const;

    /* Fuer den Pruefstand: rohe Logits aus einem fertigen Puffer von
       eingabe*eingabe*3 Byte RGB, zeilenweise (HWC), ohne Polsterung. */
    QVector<float> logits(const unsigned char *rgb) const;
    QVector<float> logits(const unsigned char *rgb, QVector<float> *abdruck,
                          float *laenge = 0) const;

    /* Ein Foto von der Platte: mit EXIF-Drehung (Qt 4 kennt sie nicht,
       die Kamera schreibt sie aber) und beim Laden schon verkleinert --
       die Kamera liefert 8 MP, gebraucht wird eine Handvoll Pixel.  Die
       laengere Kante wird auf hoechstens `kante` Pixel gebracht. */
    static QImage bildLaden(const QString &pfad, int kante = 640,
                            QString *fehler = 0);

    /* Die EXIF-Drehung eines Fotos in Grad (0, 90, 180, 270).  bildLaden()
       wendet sie selbst an; die Oberflaeche braucht die Zahl, weil Qt 4
       QML-Bilder nicht nach EXIF dreht. */
    static int exifDrehung(const QString &pfad);

private:
    struct Daten;
    Daten *d;

    Klassifizierer(const Klassifizierer &);
    Klassifizierer &operator=(const Klassifizierer &);
};

#endif
