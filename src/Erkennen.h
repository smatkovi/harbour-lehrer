#ifndef ERKENNEN_H
#define ERKENNEN_H

#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

/* Der Erkennungsmodus: ein Foto, zwei Fragen, und das viele Male.

   Mit der Einstufung (Placement) hat das bewusst nichts gemeinsam. Die
   sucht in zehn Fragen die Hoehe des Einstiegs und wird deshalb adaptiv
   gefuehrt. Hier ist das Ziel das Gegenteil: moeglichst viele verschiedene
   Bilder sehen. Also ein gemischter Durchlauf durch den Stapel, gleich
   verteilt ueber die Klassen, mit Rueckmeldung nach jedem Tipp -- ohne die
   waere es kein Ueben, sondern Raten.

   Zu jedem Foto werden zwei Dinge gefragt:
     Stufe 0  Was ist das fuer eine Wolke?
     Stufe 1  Was heisst das fuers Segelfliegen?

   Die Auswahl der ersten Frage kommt aus `verwechselt` der richtigen
   Klasse -- den Wolken, mit denen diese tatsaechlich verwechselt wird.
   Die der zweiten ist immer dieselbe kurze Liste von Bedeutungen. */
class Erkennen
{
public:
    void begin(const QVariantList &fotos, const QVariantList &klassen,
               const QVariantList &bedeutungen, int wieviele, unsigned int saat);

    bool isRunning() const { return m_index >= 0 && m_index < m_fotos.size(); }
    bool done() const { return m_index >= m_fotos.size(); }

    QVariantMap foto() const;              // das aktuelle Foto, leer am Ende
    QVariantMap klasse() const;            // seine richtige Klasse
    QVariantList options() const { return m_optionen; }   // roh, je Stufe
    int phase() const { return m_phase; }
    int answerIndex() const { return m_antwort; }
    int chosen() const { return m_gewaehlt; }
    bool checked() const { return m_gewaehlt >= 0; }
    bool wasRight() const { return m_gewaehlt == m_antwort; }
    int asked() const { return m_index; }
    int total() const { return m_fotos.size(); }

    void answer(int chosen);               // bewertet, geht noch nicht weiter
    void next();                           // naechste Stufe oder naechstes Foto

    QVariantMap result() const;

private:
    void stellen();                        // Auswahl fuer die aktuelle Stufe
    QVariantMap klasseVon(const QString &id) const;
    unsigned int wuerfel();

    QVariantList m_fotos;
    QVariantList m_klassen;
    QVariantList m_bedeutungen;
    QVariantList m_optionen;               // {id, text}
    QVariantList m_verlauf;                // {klasse, stufe, richtig}
    int m_index = -1;
    int m_phase = 0;
    int m_antwort = -1;
    int m_gewaehlt = -1;
    int m_richtigArt = 0;
    int m_richtigSinn = 0;
    unsigned int m_zustand = 1u;
};

#endif
