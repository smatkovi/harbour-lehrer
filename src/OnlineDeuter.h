#ifndef ONLINEDEUTER_H
#define ONLINEDEUTER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/* Der zweite Blick: dasselbe Foto an Claude (Anthropic-API), zusammen mit
   der Klassenliste des Kurses.  Die Antwort kommt als Karte ueber fertig():

     ok           bool
     klasse       Kennung aus der Liste; "" wenn keine passt
     sicherheit   "hoch" | "mittel" | "niedrig"
     begruendung  ein, zwei Saetze in `sprache` (de/en): woran es zu
                  erkennen ist und was es fuer den Tag heisst
     fehler       Text, wenn ok falsch ist (kein Netz, Schluessel
                  abgelehnt, Antwort unlesbar ...)

   Auf Qt 5 geht die Anfrage ueber QNetworkAccessManager.  Auf Harmattan
   (Qt 4.7, dessen TLS zu alt ist) ueber einen Hilfsprozess -- wget aus
   /opt/wunderw wie bei Lichess im Schachlehrer.  In beiden Faellen gilt:
   der Schluessel steht nie in argv (ps liest mit), die Anfrage und die
   Antwort gehen nie ins Protokoll, und das Bild wird vor dem Senden auf
   eine lange Kante von 1024 Pixeln verkleinert.

   Der oeffentliche Teil ist die Abmachung mit Course.cpp; private Slots
   und Felder gehoeren der Umsetzung. */
class OnlineDeuter : public QObject
{
    Q_OBJECT
public:
    explicit OnlineDeuter(QObject *parent = 0);
    ~OnlineDeuter();

    /* Gibt es ueberhaupt einen Weg ins Netz?  Qt 5: immer.  Qt 4: nur,
       wenn der Hilfsprozess da ist. */
    static bool verfuegbar();

    /* Der Schluessel aus ~/.profile ("export ANTHROPIC_API_KEY=..."), fuer
       Geraete, auf denen er dort schon steht; "" wenn nicht. */
    static QString schluesselAusProfil();

    bool laeuft() const;

    /* klassen: [{id, name}] in Anzeigesprache; sprache "de" oder "en". */
    void anfragen(const QString &bildPfad, const QVariantList &klassen,
                  const QString &sprache, const QString &schluessel);
    void abbrechen();

signals:
    void fertig(const QVariantMap &antwort);

private slots:
    void prozessFertig(int code);
    void antwortFertig();
    void zeitAbgelaufen();
    void fehlerMelden();

private:
    struct Daten;
    Daten *d;

    void melden(QVariantMap karte);
    void aufraeumen();
};

#endif
