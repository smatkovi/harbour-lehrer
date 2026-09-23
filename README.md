# Lehrer — drei Kurse für Sailfish OS

Ein Programm, drei Kurse: **C-Lehrer** (C, C++, Rust und Python mit Blick auf
Simulation), **Segelschein** (Theorie für den Segelschein A) und **Segelflug**
(Wolken lesen und Segelflugtheorie).

Kurze Lektionen, ein Beispiel, das wirklich läuft, dann Aufgaben, die vom
Lesen über das Ergänzen zum Selberschreiben führen. Was einmal saß, kommt in
wachsenden Abständen zur Wiederholung wieder.

Der Kurs steckt vollständig in `kurse/<name>/kurs.json`; das Programm weiß
nichts über sein Fach. Deshalb ist ein weiterer Kurs ein weiteres Paket und
kein zweites Programm.

## Portiert, nicht neu erfunden

Die Maschine darunter — Kurs, Prüfer, Einstufung, Läufer, Zeichner — stammt
aus der Harmattan-Fassung für das Nokia N9 (Qt 4.7, QtQuick 1.1) und ist
dieselbe geblieben. Neu geschrieben wurde die Oberfläche in Silica, dazu drei
Stellen, an denen Qt 5 es einfacher macht:

* JSON kommt von `QJsonDocument` statt über die ECMAScript-Maschine von QtScript,
* der Zeichner hängt als `QQuickImageProvider` statt als `QDeclarativeImageProvider`,
* der Fortschritt liegt in `QStandardPaths::AppDataLocation` — dem einen Ort,
  den die Sandbox der App freigibt.

## Die zwei Deuter

Die Programmierkurse führen fremden Code aus, und das geht auf einem Telefon
nur mit einem Deuter, nicht mit einem Übersetzer:

* **crun** ist [picoc](https://gitlab.com/zsaleeba/picoc) (New BSD) und führt
  die C-Lektionen aus. Eine Endlosschleife im ersten eigenen Programm kostet
  damit einen Kindprozess, nicht die App.
* **rrun** ist ein eigener Deuter für genau den Rust-Ausschnitt, den der Kurs
  lehrt: `let`/`let mut`, Funktionen, `if`, Schleifen, `Vec`, Strukturen,
  Besitz und Ausleihen. Er meldet die drei Fehler, um die es in Rust geht, mit
  den echten Nummern — E0382 (Verwendung nach der Besitzübergabe), E0499 und
  E0502 (zwei Ausleihen zugleich). Er prüft während des Laufs, nicht vorher:
  wer hier ein Programm zum Laufen bringt, hat es noch nicht an rustc vorbei.
  Das steht auch so in der ersten Rust-Lektion.

C++ hat auf dem Gerät keinen Übersetzer und Python kein NumPy; diese Kapitel
werden gelesen und vorhergesagt. Ein „Ausführen“-Knopf, hinter dem nichts
läuft, wäre eine Lüge.

## Bauen

    tools/build.sh                  # alle drei Kurse, aarch64 und armv7hl
    tools/build.sh clehrer          # nur einen
    ARCHES=aarch64 tools/build.sh   # nur eine Architektur

Gebaut wird im Sailfish-SDK-Behälter auf dem Baurechner; die Pakete landen in
`~/ps/rpms/lehrer/`.

## Wie geprüft wird

Der Kurs selbst hat eine Probe: `~/ps/c-lehrer/tools/test-curriculum.py` führt
jedes Beispiel, jede Musterlösung und jede Aufgabenvorlage durch den echten
Deuter und vergleicht mit der hinterlegten Ausgabe. Nichts geht hinaus, was
dort nicht durchgelaufen ist.
