#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Macht aus den vorhandenen Harmattan-Icons die Sailfish-Groessen.

Die Icons bleiben dieselben -- nur die Groessen sind andere: Sailfish will
86, 108, 128 und 172 Pixel, Harmattan hatte 64 und 80. Hochskaliert wird mit
Lanczos; das ist bei einem gezeichneten Symbol das Verfahren, das die Kanten
am wenigsten ausfranst.

    tools/icons.py
"""
import os

from PIL import Image

HIER = os.path.dirname(os.path.abspath(__file__))
WURZEL = os.path.dirname(HIER)
PS = os.path.dirname(WURZEL)

QUELLEN = {
    "clehrer": os.path.join(PS, "c-lehrer", "icons", "icon-80.png"),
    "segelschein": os.path.join(PS, "segelschein", "icons", "icon-80.png"),
    "segelflug": os.path.join(PS, "segelflug", "icons", "icon-80.png"),
}
GROESSEN = (86, 108, 128, 172)


def main():
    for kurs, quelle in QUELLEN.items():
        bild = Image.open(quelle).convert("RGBA")
        ziel = os.path.join(WURZEL, "kurse", kurs, "icons")
        os.makedirs(ziel, exist_ok=True)
        for groesse in GROESSEN:
            neu = bild.resize((groesse, groesse), Image.LANCZOS)
            neu.save(os.path.join(ziel, "%d.png" % groesse))
        print("%-12s %s -> %s" % (kurs, os.path.basename(quelle),
                                  ", ".join(str(g) for g in GROESSEN)))


if __name__ == "__main__":
    main()
