#include "Erkennen.h"

namespace {

QString idVon(const QVariantMap &m)
{
    return m.value(QLatin1String("id")).toString();
}

}

unsigned int Erkennen::wuerfel()
{
    // Ein eigener kleiner Zufall, damit die Reihenfolge nicht davon
    // abhaengt, wer sonst noch qrand() aufgerufen hat.
    m_zustand = m_zustand * 1103515245u + 12345u;
    return m_zustand >> 16;
}

void Erkennen::begin(const QVariantList &fotos, const QVariantList &klassen,
                     const QVariantList &bedeutungen, int wieviele,
                     unsigned int saat)
{
    m_klassen = klassen;
    m_bedeutungen = bedeutungen;
    m_verlauf.clear();
    m_richtigArt = 0;
    m_richtigSinn = 0;
    m_zustand = saat + 1u;

    /* Erst je Klasse mischen, dann reihum eines nehmen. Ein einfaches
       Mischen ueber den ganzen Stapel bringt bei zwanzig Fotos leicht
       fuenf Cumulus hintereinander -- und wer fuenfmal dasselbe sieht,
       raet beim sechsten Mal richtig, ohne hinzusehen. */
    QMap<QString, QVariantList> nachKlasse;
    for (const QVariant &value : fotos) {
        const QVariantMap foto = value.toMap();
        nachKlasse[foto.value(QLatin1String("klasse")).toString()].append(foto);
    }
    QList<QVariantList> stapel;
    for (QMap<QString, QVariantList>::iterator it = nachKlasse.begin();
         it != nachKlasse.end(); ++it) {
        QVariantList liste = it.value();
        for (int i = liste.size() - 1; i > 0; --i)
            liste.swap(i, int(wuerfel() % unsigned(i + 1)));
        stapel.append(liste);
    }
    for (int i = stapel.size() - 1; i > 0; --i)
        stapel.swap(i, int(wuerfel() % unsigned(i + 1)));

    m_fotos.clear();
    bool nochWas = true;
    for (int runde = 0; nochWas; ++runde) {
        nochWas = false;
        for (int s = 0; s < stapel.size(); ++s) {
            if (runde < stapel.at(s).size()) {
                m_fotos.append(stapel.at(s).at(runde));
                nochWas = true;
            }
        }
    }
    if (wieviele > 0 && m_fotos.size() > wieviele) {
        while (m_fotos.size() > wieviele)
            m_fotos.removeLast();
    }

    m_index = m_fotos.isEmpty() ? 0 : 0;
    m_phase = 0;
    m_gewaehlt = -1;
    stellen();
}

QVariantMap Erkennen::foto() const
{
    if (!isRunning())
        return QVariantMap();
    return m_fotos.at(m_index).toMap();
}

QVariantMap Erkennen::klasseVon(const QString &id) const
{
    for (const QVariant &value : m_klassen) {
        const QVariantMap k = value.toMap();
        if (idVon(k) == id)
            return k;
    }
    return QVariantMap();
}

QVariantMap Erkennen::klasse() const
{
    return klasseVon(foto().value(QLatin1String("klasse")).toString());
}

void Erkennen::stellen()
{
    m_optionen.clear();
    m_antwort = -1;
    if (!isRunning())
        return;
    const QVariantMap richtige = klasse();
    QVariantList auswahl;

    if (m_phase == 0) {
        auswahl.append(richtige);
        /* Zuerst die, mit denen diese Wolke wirklich verwechselt wird.
           Erst wenn die nicht reichen, irgendwelche anderen: zwischen
           Cirrus und Stratus zu unterscheiden uebt niemand. */
        QStringList genommen;
        genommen << idVon(richtige);
        const QVariantList nah =
                richtige.value(QLatin1String("verwechselt")).toList();
        for (const QVariant &value : nah) {
            if (auswahl.size() >= 4)
                break;
            const QString id = value.toString();
            if (genommen.contains(id))
                continue;
            const QVariantMap k = klasseVon(id);
            if (k.isEmpty())
                continue;
            genommen << id;
            auswahl.append(k);
        }
        int schutz = 0;
        while (auswahl.size() < 4 && m_klassen.size() > auswahl.size()
               && schutz++ < 200) {
            const QVariantMap k =
                    m_klassen.at(int(wuerfel() % unsigned(m_klassen.size()))).toMap();
            if (genommen.contains(idVon(k)))
                continue;
            genommen << idVon(k);
            auswahl.append(k);
        }
    } else {
        const QString richtig = richtige.value(QLatin1String("bedeutung")).toString();
        QVariantMap treffer;
        QVariantList andere;
        for (const QVariant &value : m_bedeutungen) {
            const QVariantMap b = value.toMap();
            if (idVon(b) == richtig)
                treffer = b;
            else
                andere.append(b);
        }
        auswahl.append(treffer);
        for (int i = andere.size() - 1; i > 0; --i)
            andere.swap(i, int(wuerfel() % unsigned(i + 1)));
        for (int i = 0; i < andere.size() && auswahl.size() < 4; ++i)
            auswahl.append(andere.at(i));
    }

    // Die richtige Antwort steht bis hier immer vorn -- jetzt mischen.
    for (int i = auswahl.size() - 1; i > 0; --i)
        auswahl.swap(i, int(wuerfel() % unsigned(i + 1)));
    m_optionen = auswahl;
    const QString richtigeId = m_phase == 0
            ? idVon(richtige)
            : richtige.value(QLatin1String("bedeutung")).toString();
    for (int i = 0; i < m_optionen.size(); ++i) {
        if (idVon(m_optionen.at(i).toMap()) == richtigeId)
            m_antwort = i;
    }
}

void Erkennen::answer(int chosen)
{
    if (!isRunning() || m_gewaehlt >= 0)
        return;
    m_gewaehlt = chosen;
    const bool richtig = chosen == m_antwort;
    if (richtig) {
        if (m_phase == 0)
            ++m_richtigArt;
        else
            ++m_richtigSinn;
    }
    QVariantMap eintrag;
    eintrag.insert(QLatin1String("klasse"),
                   foto().value(QLatin1String("klasse")));
    eintrag.insert(QLatin1String("stufe"), m_phase);
    eintrag.insert(QLatin1String("richtig"), richtig);
    m_verlauf.append(eintrag);
}

void Erkennen::next()
{
    if (!isRunning())
        return;
    if (m_gewaehlt < 0)        // ohne Antwort geht es nicht weiter
        return;
    m_gewaehlt = -1;
    if (m_phase == 0) {
        m_phase = 1;
    } else {
        m_phase = 0;
        ++m_index;
    }
    stellen();
}

QVariantMap Erkennen::result() const
{
    QVariantMap out;
    const int fotos = m_index;
    out.insert(QLatin1String("fotos"), fotos);
    out.insert(QLatin1String("gesamt"), m_fotos.size());
    out.insert(QLatin1String("art"), m_richtigArt);
    out.insert(QLatin1String("sinn"), m_richtigSinn);

    /* Was noch zu ueben ist: die Klassen, bei denen die Frage nach der
       Wolke daneben ging. Ohne diese Liste sagt das Ergebnis nur eine
       Zahl, und eine Zahl sagt nicht, welches Bild man sich noch einmal
       ansehen sollte. */
    QMap<QString, QPair<int, int> > proKlasse;   // {richtig, gesamt}
    for (const QVariant &value : m_verlauf) {
        const QVariantMap e = value.toMap();
        if (e.value(QLatin1String("stufe")).toInt() != 0)
            continue;
        const QString id = e.value(QLatin1String("klasse")).toString();
        QPair<int, int> &zahl = proKlasse[id];
        if (e.value(QLatin1String("richtig")).toBool())
            ++zahl.first;
        ++zahl.second;
    }
    QVariantList schwach;
    for (QMap<QString, QPair<int, int> >::const_iterator it = proKlasse.begin();
         it != proKlasse.end(); ++it) {
        if (it.value().first == it.value().second)
            continue;
        QVariantMap eintrag;
        eintrag.insert(QLatin1String("klasse"), it.key());
        eintrag.insert(QLatin1String("richtig"), it.value().first);
        eintrag.insert(QLatin1String("gesamt"), it.value().second);
        schwach.append(eintrag);
    }
    out.insert(QLatin1String("schwach"), schwach);
    return out;
}
