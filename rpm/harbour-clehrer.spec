Name:       harbour-clehrer
Version:    1.7.0
Release:    1
Summary:    Learn C, C++, Rust and Python with an eye on simulation
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
strip %{buildroot}%{_bindir}/%{name} %{buildroot}%{_libexecdir}/%{name}/*

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_libexecdir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%changelog
* Tue Sep 23 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.0.0-1
- Erste Sailfish-Fassung, portiert von der Harmattan-Ausgabe
