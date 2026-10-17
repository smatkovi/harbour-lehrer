#!/bin/sh
# Einen Kurs aus seinem eigenen Baum herueberholen.
#
#   tools/kurs-holen.sh segelflug
#
# Die Kurse werden woanders autoriert (~/ps/segelflug, ~/ps/meteorologie,
# ...) und hier nur mitgeliefert. Bis hierher wurde das von Hand kopiert,
# und das ging zweimal daneben: einmal lag eine alte kurs.json im Paket,
# einmal fehlten die neuen Bilder.
set -e
KURS=${1:?Kurs angeben}
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
QUELLE=${2:-$HOME/ps/$KURS}
ZIEL=$ROOT/kurse/$KURS

[ -f "$QUELLE/data/kurs.json" ] || { echo "kein Kurs in $QUELLE" >&2; exit 1; }
mkdir -p "$ZIEL/bilder"
cp "$QUELLE/data/kurs.json" "$ZIEL/kurs.json"
[ -f "$QUELLE/CREDITS" ] && cp "$QUELLE/CREDITS" "$ZIEL/CREDITS"
rsync -a --delete "$QUELLE/bilder/" "$ZIEL/bilder/"
echo "$KURS: $(ls "$ZIEL/bilder" | wc -l) Bilder, kurs.json $(stat -c %s "$ZIEL/kurs.json") B"
