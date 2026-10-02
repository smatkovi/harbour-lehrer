Name:       harbour-meteorologie
Version:    1.0.0
Release:    1
Summary:    Meteorology for physicists, every formula derived
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
Meteorologie fuer Physiker: die Atmosphaere aus Thermodynamik, Strahlung
und Mechanik hergeleitet -- Hydrostatik, feuchte Luft und Emagramm,
Strahlungshaushalt, Wolkenphysik, Coriolis-Kraft und geostrophischer Wind,
Zirkulation, Fronten und Zyklonen, Foehn und Gewitter, Optik des Himmels,
Grenzen der Vorhersage. Jede Formel wird hergeleitet, gesetzt und mit
Skizze und Zahlenbeispiel gezeigt. Deutsch oder Englisch.

Dieselbe App traegt mehrere Kurse; dieses Paket ist Meteorologie.

%prep
%setup -q

%build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DKURS=meteorologie
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
* Fri Oct 02 2026 smatkovi <smatkovi@users.noreply.github.com> - 1.0.0-1
- Erste Fassung: elf Kapitel, 30 Lektionen, 71 Aufgaben, alle Formeln
  hergeleitet und gesetzt, 30 gezeichnete Skizzen, deutsch und englisch.
