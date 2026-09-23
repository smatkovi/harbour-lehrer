#!/bin/sh
# Baut die RPMs im Sailfish-SDK-Behaelter auf dem Arch-Rechner.
#
#   tools/build.sh                       # alle drei Kurse, beide Architekturen
#   tools/build.sh clehrer               # nur einen Kurs
#   ARCHES="aarch64" tools/build.sh      # nur eine Architektur
#
# Die Pakete landen in ~/ps/rpms/lehrer/ auf diesem Geraet, bereit fuer
# tools/release.sh. BUILD_HOST, SDK_CONTAINER und SDK_TARGET ueberschreiben
# die Vorgaben.
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
KURSE=${*:-"clehrer segelschein segelflug"}
ARCHES=${ARCHES:-"aarch64 armv7hl"}

if [ -n "$BUILD_HOST" ]; then
    HOST=$BUILD_HOST
elif ssh -o BatchMode=yes -o ConnectTimeout=4 sebastian@192.168.1.21 true 2>/dev/null; then
    HOST=sebastian@192.168.1.21
else
    HOST=arch
fi
CONTAINER=${SDK_CONTAINER:-sfossdk52}
TARGET=${SDK_TARGET:-SailfishOS-5.2.0.15}

echo "== Build-Rechner: $HOST"
ssh "$HOST" "mkdir -p ~/lehrer-build/src ~/lehrer-build/out"
rsync -a --delete --exclude .git --exclude build "$ROOT/" "$HOST:lehrer-build/src/"
ssh "$HOST" "cd ~/lehrer-build/src && tar czf /tmp/lehrer-src.tgz . && \
    docker cp /tmp/lehrer-src.tgz $CONTAINER:/tmp/lehrer-src.tgz"

mkdir -p "$HOME/ps/rpms/lehrer"
for KURS in $KURSE; do
    NAME=harbour-$KURS
    VERSION=$(sed -n 's/^Version: *//p' "$ROOT/rpm/$NAME.spec")
    for ARCH in $ARCHES; do
        RPM=$NAME-$VERSION-1.$ARCH.rpm
        echo "== $NAME $VERSION für $ARCH"
        # Je Kurs und Architektur ein eigener Baum: mb2 legt seine
        # Zwischenstaende neben die Quellen, und zwei Kurse im selben Baum
        # wuerden sich die Uebersetzungsergebnisse gegenseitig ueberschreiben.
        ssh "$HOST" "docker exec $CONTAINER bash -lc '\
            rm -rf ~/lbuild-$KURS-$ARCH && mkdir -p ~/lbuild-$KURS-$ARCH && \
            cd ~/lbuild-$KURS-$ARCH && tar xzf /tmp/lehrer-src.tgz && \
            mb2 -t $TARGET-$ARCH -s rpm/$NAME.spec build' | \
            grep -E '^Wrote:|error:|Fehler|packages and' || true"
        ssh "$HOST" "docker cp $CONTAINER:/home/mersdk/lbuild-$KURS-$ARCH/RPMS/$RPM \
            ~/lehrer-build/out/"
        rsync -a "$HOST:lehrer-build/out/$RPM" "$HOME/ps/rpms/lehrer/"
        echo "  -> ~/ps/rpms/lehrer/$RPM"
    done
done
