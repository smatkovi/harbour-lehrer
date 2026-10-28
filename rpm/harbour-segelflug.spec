Name:       harbour-segelflug
Version:    1.19.6
Release:    1
Summary:    Gliding theory with a focus on reading clouds
License:    GPLv3+
URL:        https://github.com/smatkovi/harbour-lehrer
Source0:    %{name}-%{version}.tar.gz

Requires:       sailfishsilica-qt5
BuildRequires:  cmake
BuildRequires:  pkgconfig(sailfishapp)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)

%description
Segelflug ist ein Kurs zum Mitmachen: kurze Lektionen, ein Beispiel, das
wirklich laeuft, und Aufgaben, die vom Lesen zum Selberschreiben
fuehren. Der Fortschritt wird in wachsenden Abstaenden wiederholt.

Dieselbe App traegt drei Kurse; dieses Paket ist Segelflug. Fuer die
Programmierkurse liegen zwei Deuter daneben: picoc fuer C und rrun,
ein eigener Deuter fuer den Rust-Ausschnitt, den der Kurs lehrt.

%prep
%setup -q

%build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DKURS=segelflug
make -j$(nproc)

%install
cd build
make DESTDIR=%{buildroot} install
# Ohne Symboltabellen: das spart auf einem Telefon ein paar hundert Kilobyte
# je Paket, und zum Suchen nach Fehlern wird ohnehin neu gebaut.
# Nur die Programme strippen -- crunxx ist ein Skript.
strip %{buildroot}%{_bindir}/%{name} %{buildroot}%{_libexecdir}/%{name}/crun \
      %{buildroot}%{_libexecdir}/%{name}/rrun

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_libexecdir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%changelog
* Wed Oct 28 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.19.6-1
- Das naechste Foto waehlen bricht die laufende Claude-Anfrage nicht mehr
  ab: ihr Urteil wird aus einer Abschrift des vorigen Fotos gelernt
- Lernen von Fable ist jetzt der Normalfall (an), bei Sicherheit hoch und
  mittel; der Schalter schaltet es aus

* Wed Oct 28 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.19.5-1
- Claude fragen auf Fable zugeschnitten: kein thinking-Feld mehr (Fable
  lehnt "disabled" ab), kein erzwungenes Werkzeug (lehnt Fable 5.1 auch
  ab) -- das Urteil kommt als structured output nach Schema; verweigert
  Fable das Foto, antwortet Opus

* Wed Oct 28 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.19.4-1
- Claude fragen: zuerst Fable (claude-fable-5-1), bei Ablehnung dieselbe
  Anfrage mit Opus (claude-opus-5); die Antwort nennt das Modell; von
  selbst gemerkt wird nur, was Fable sagt, bei Opus nur per Knopf
- Schalter "Von Claude lernen, unabhaengig von meinem Tipp": Claudes Klasse
  wird bei Sicherheit "hoch" gemerkt, auch gegen den eigenen Tipp

* Wed Oct 28 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.19.3-1
- Neue Lektion "Wie und wann Wolken entstehen": Blase, Rampe, Welle, Nacht --
  und was davon im Namen steht; Skizze dazu in der Loesung
- Schalter "Auch ohne meinen Tipp von Claude lernen" (nur bei Sicherheit
  "hoch", auf der Gelerntes-Seite)

* Wed Oct 28 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.19.2-1
- Claude fragen: das Urteil kommt als Werkzeugaufruf mit Schema zurueck
  (statt JSON im Text, das manchmal "Antwort nicht lesbar" ergab); Platz
  fuer die Antwort verdoppelt; eine unlesbare Antwort wird mit ihrem Anfang
  benannt und zum Nachlesen abgelegt
- "Foto waehlen" zeigt wieder die Bilder: der Bildwaehler liest den
  Tracker-Index und braucht dafuer das Sailjail-Recht MediaIndexing; dazu
  RemovableMedia fuer Fotos auf der Speicherkarte

* Tue Oct 27 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.19.1-1
- Gelerntes aus 1.18.x bleibt erhalten: die Datei wird als Fassung 1 gelesen,
  ihre Abdruecke werden aus den Bildern nachgerechnet, geschrieben wird
  Fassung 2 (1.19.0 haette sie verworfen)
- "Sichern" auf der Gelerntes-Seite erreicht jetzt ~/Documents (Sailjail-Recht
  Documents fehlte)
- Der Lernfaden rechnet auf einer Abschrift statt im lebenden Gedaechtnis
  (Vergessen waehrend der Rechnung war ein Wettlauf)
- Kamera: Tippen stellt jetzt wirklich scharf und belichtet. Die alte Seite
  rief eine Methode auf, die es im QML-Typ nicht gibt, und brach vor dem
  Fokusbefehl ab; der Tipp-Punkt wird jetzt wie in der Jolla-Kamera in die
  Sensorlage gedreht; kann die Kamera Unendlich-Fokus, bleibt die Schaerfe
  dort und der Tipp setzt die Messflaeche auf die Wolke

* Tue Oct 27 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.19.0-1
- Dazulernen trainiert jetzt die letzte Schicht des Netzes wirklich nach
  (ab 40 gemerkten Fotos, im Arbeitsfaden); im Versuch verdoppelt das den
  Gewinn des Gedaechtnisses
- Die Fotoseite sagt, wenn das Gelernte ein Urteil geaendert hat

* Tue Oct 27 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.18.1-1
- Kamera: Der Ring erscheint jetzt immer beim Tippen, und wenn die Kamera
  auf Tippen nicht scharfstellt, steht es da, statt dass man wartet

* Tue Oct 27 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.18.0-1
- Dazulernen: Was Claude und der eigene Tipp uebereinstimmend sagen, merkt
  sich die App und bezieht es beim naechsten Foto mit ein -- uebertragbar
  als gelerntes.bin, einsehbar und einzeln loeschbar
- Kamera: Tippen stellt scharf und belichtet auf die Wolke
- Glossar: 98 Fachwoerter mit Definition, Verweisen und Zeichnungen
- Netz neu trainiert; App und Training stauchen jetzt gleich (64 % statt
  60 % Top-1 in der Kreuzprobe)
- Servergelieferter Text wird als Klartext gezeigt, nicht als Auszeichnung

* Tue Oct 27 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.17.0-1
- Foto deuten: ein eigenes Himmelsfoto (Galerie oder Kamera) wird im
  Geraet von einem kleinen Faltungsnetz (MobileNetV2, 160 px, 14 Klassen)
  eingeordnet, mit Bedeutung fuers Segelfliegen; erst der eigene Tipp,
  dann die drei wahrscheinlichsten Klassen
- Dazu auf Wunsch der zweite Blick von Claude (Anthropic-API, eigener
  Schluessel); beide Urteile bleiben nebeneinander sichtbar
- Sechs Hinweise, wie das Foto gelingt (Sonne im Ruecken, Basis im Bild)
- Neue Lektion: Tagesverlaeufe -- wie ein Flugtag laufen kann und woran
  man es am Morgen an den Wolken sieht
- Vier weitere Klassen mit Bedeutung: Altostratus, Nimbostratus,
  Cirrocumulus, Kondensstreifen

* Tue Oct 27 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.16.0-1
- Erkennungsmodus noch einmal Bild fuer Bild durchgesehen: 19 Bilder
  raus, drei in die richtige Klasse, 134 bleiben
- Das Lektionsfoto der Wolkenstrasse zeigt jetzt Cumulus-Reihen, nicht
  Cirrus-Baender

* Sat Oct 17 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.15.0-1
- Der Erkennungsmodus hat jetzt 153 statt 81 Fotos
- Eine Runde sind zwanzig Bilder; der ganze Stapel steht im Menue

* Sat Oct 17 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.14.0-1
- Neuer Modus "Wolken erkennen": 81 Fotos einzeln, zu jedem zwei Fragen --
  was fuer eine Wolke das ist und was sie fuers Segelfliegen heisst
- Das Foto unter "Wolkenstrassen" zeigte eine Vorstadtstrasse namens
  Cloud Street; ersetzt

* Fri Oct 09 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.13.0-1
- Die Fotos stehen jetzt auch bei den Aufgaben und auf den Karteikarten:
  ueber der Frage neben der Zeichnung und noch einmal in der Loesung

* Fri Oct 09 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.12.0-1
- Zu jeder gezeichneten Wolke stehen jetzt zwei bis drei echte Fotos, jedes
  mit einem Satz, der sagt, wohin zu schauen ist (Wikimedia Commons, frei
  lizenziert; Urheber und Lizenz stehen unter dem Bild und in CREDITS)

* Fri Oct 02 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.11.2-1
- Die Durchsicht nach dem Einstufungstest lud nicht (Column.bottomPadding
  gibt es in QtQuick 2.0 nicht); "Deine Wahl" auch auf Englisch
- Herleitungen: ϑ durch θ ersetzt, damit keine Kästchen erscheinen

* Thu Sep 24 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.11.0-1
- Die Formeln auf den Karteikarten sind hergeleitet: Wolkenbasis (woher die
  125 m je Grad kommen), Gleitzahl, Lastvielfaches und Überziehgeschwindigkeit
  in der Kurve, Schwerpunktrechnung, 1-in-60-Regel und die Missweisung. Jede
  mit der Skizze, die dazugehört, und jede in der Lösung — vor der Antwort
  wäre sie die Antwort.
* Wed Sep 23 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.0.0-1
- Erste Sailfish-Fassung, portiert von der Harmattan-Ausgabe
