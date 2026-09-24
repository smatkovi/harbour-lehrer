Name:       harbour-clehrer
Version:    1.12.0
Release:    1
Summary:    Learn C, C++, Rust and Python with an eye on simulation
License:    GPLv3+
URL:        https://github.com/smatkovi/harbour-lehrer
Source0:    %{name}-%{version}.tar.gz

Requires:       sailfishsilica-qt5
# Die C- und Rust-Lektionen laufen mit eigenen Deutern, die im Paket
# stecken. C++ laesst sich nicht deuten, dafuer braucht es einen
# Uebersetzer -- er ist groesser als der ganze Kurs und deshalb nur
# empfohlen, nicht verlangt: Ohne ihn bleiben die C++-Lektionen lesbar.
Recommends:     gcc-c++
# Ein einziges Kapitel rechnet mit ganzen Feldern statt mit Schleifen. NumPy
# liegt nicht bei Jolla, sondern in Chum -- ist es nicht da, bleibt das
# Kapitel lesbar, und die Lektion sagt, woher es kommt.
Recommends:     python3-numpy
BuildRequires:  cmake
BuildRequires:  pkgconfig(sailfishapp)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)

%description
C-Lehrer ist ein Kurs zum Mitmachen: kurze Lektionen, ein Beispiel, das
wirklich laeuft, und Aufgaben, die vom Lesen zum Selberschreiben
fuehren. Der Fortschritt wird in wachsenden Abstaenden wiederholt.

Dieselbe App traegt drei Kurse; dieses Paket ist C-Lehrer. Fuer die
Programmierkurse liegen zwei Deuter daneben: picoc fuer C und rrun,
ein eigener Deuter fuer den Rust-Ausschnitt, den der Kurs lehrt.

%prep
%setup -q

%build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DKURS=clehrer
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
* Thu Sep 25 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.12.0-1
- Die Spielwiese sagt jetzt, welche Sprache gedeutet und welche übersetzt
  wird, und warum das die Wartezeit erklärt, die man tatsächlich sieht: C und
  Rust laufen sofort los (eigene Deuter), Python muss erst hochkommen, C++
  wird wirklich übersetzt. Aufgeklappt steht dazu, was der Unterschied ist —
  und dass er nichts darüber sagt, welche Sprache schnell ist.
* Thu Sep 25 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.11.1-1
- C-Lehrer startet wieder aus dem App-Raster. Seit die App ohne Sandkasten
  läuft (1.10.0, damit Python und C++ überhaupt da sind), ging ihr Start über
  den Booster von mapplauncherd — und der lädt die Anwendung mit dlopen, was
  diese Binärdatei nicht kann. Im Raster passierte daraufhin gar nichts; der
  Grund stand nur im Journal. Die Startdatei sagt jetzt "no-invoker".
* Wed Sep 24 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.11.0-1
- Gleicher Stand wie Segelschein und Segelflug: der Deuter kann einer Aufgabe
  eine Herleitung und eine Skizze mitgeben und zeigt beide in der Lösung. Der
  C-Kurs nutzt das noch nicht — keine seiner Karten nennt eine Formel —, aber
  alle drei Pakete tragen damit denselben Deuter.
* Tue Sep 23 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.0.0-1
- Erste Sailfish-Fassung, portiert von der Harmattan-Ausgabe
