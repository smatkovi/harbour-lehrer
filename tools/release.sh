#!/bin/sh
# Veroeffentlicht die Pakete einer Fassung als Release dieses Repos.
#
#   tools/release.sh <version> <paket> [<paket> ...]
#
# gh ist auf dem Arch-Rechner angemeldet, also gehen die Dateien zuerst
# dorthin. Ein zweiter Aufruf mit weiteren Dateien haengt sie an das schon
# vorhandene Release an.
set -e
VERSION=$1
shift 2>/dev/null || true
if [ -z "$VERSION" ] || [ $# -eq 0 ]; then
    echo "Aufruf: tools/release.sh <version> <rpm> [<rpm> ...]" >&2
    exit 2
fi
for DATEI in "$@"; do
    [ -f "$DATEI" ] || { echo "keine solche Datei: $DATEI" >&2; exit 2; }
done

if [ -n "$BUILD_HOST" ]; then
    HOST=$BUILD_HOST
elif ssh -o BatchMode=yes -o ConnectTimeout=4 sebastian@192.168.1.21 true 2>/dev/null; then
    HOST=sebastian@192.168.1.21
else
    HOST=arch
fi
REPO=${REPO:-smatkovi/harbour-lehrer}
TAG=v$VERSION
WORK=/tmp/lehrer-release

NOTIZ=$(mktemp)
cat > "$NOTIZ" <<NOTE
Lehrer $VERSION

Drei Kurse, ein Programm: **C-Lehrer** (C, C++, Rust und Python mit Blick auf
Simulation), **Segelschein** (Theorie für den Segelschein A) und **Segelflug**
(Wolken lesen und Segelflugtheorie).

Für jeden Kurs ein Paket, je für \`aarch64\` und \`armv7hl\`. Installiert wird
mit \`pkcon install-local <datei>.rpm\` oder \`rpm -Uvh <datei>.rpm\`.

Die C- und Rust-Lektionen führen den eigenen Code wirklich aus: \`crun\`
(picoc) und \`rrun\`, ein eigener Deuter für den Rust-Ausschnitt des Kurses,
liegen im Paket. C++ und Python werden gelesen und vorhergesagt — ein
Ausführen-Knopf ohne Übersetzer dahinter wäre eine Lüge.
NOTE

ssh "$HOST" "mkdir -p $WORK"
for DATEI in "$@"; do
    scp -q "$DATEI" "$HOST:$WORK/"
done
scp -q "$NOTIZ" "$HOST:$WORK/notiz.md"
rm -f "$NOTIZ"

NAMEN=""
for DATEI in "$@"; do
    NAMEN="$NAMEN $WORK/$(basename "$DATEI")"
done

ssh "$HOST" "cd $WORK && \
    if gh release view $TAG --repo $REPO >/dev/null 2>&1; then \
        gh release upload $TAG $NAMEN --repo $REPO --clobber; \
    else \
        gh release create $TAG $NAMEN --repo $REPO --title 'Lehrer $VERSION' \
            --notes-file notiz.md; \
    fi"
echo "== Release $TAG: https://github.com/$REPO/releases/tag/$TAG"
