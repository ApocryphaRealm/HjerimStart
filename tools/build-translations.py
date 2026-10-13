#!/usr/bin/env python3
r"""Write the eleven translation files for Alternate Perspective Hjerim Addon.

Every string the AP menu shows lives here, English first and each of the other
ten languages beside it. Output: dist\Interface\Translations\HjerimStart_<language>.txt, UTF-16LE with
BOM, one "$key<TAB>text" per line - the SKSE/SkyUI shape the game reads.
A key missing from a language falls back to English at runtime (utils/Strings.h), and this script says
so at build time so a half-finished table never ships silently.

Usage:  python build-translations.py
"""
import io
import os

E = {
    'HJS_Card': 'Windhelm',
    'HJS_OwnCard': 'Hjerim',
    'HJS_CardDescr': "The house in Windhelm is already yours - deed, key and every room furnished. Begin at home, on the Butcher's own doorstep.",
}

T = {
    'german': {
        'HJS_Card': 'Windhelm',
        'HJS_OwnCard': 'Hjerim',
        'HJS_CardDescr': 'Das Haus in Windhelm gehört bereits dir - Urkunde, Schlüssel und jeder Raum eingerichtet. Beginne zu Hause, direkt vor der Tür des Schlächters.',
    },
    'french': {
        'HJS_Card': 'Vendeaume',
        'HJS_OwnCard': 'Hjerim',
        'HJS_CardDescr': 'La maison de Vendeaume est déjà à vous : acte, clé et chaque pièce meublée. Commencez chez vous, sur le seuil même du Boucher.',
    },
    'spanish': {
        'HJS_Card': 'Ventalia',
        'HJS_OwnCard': 'Hjerim',
        'HJS_CardDescr': 'La casa de Ventalia ya es tuya: escritura, llave y cada habitación amueblada. Empieza en casa, en el mismo umbral del Carnicero.',
    },
    'italian': {
        'HJS_Card': 'Windhelm',
        'HJS_OwnCard': 'Hjerim',
        'HJS_CardDescr': 'La casa di Windhelm è già tua: atto, chiave e ogni stanza arredata. Inizia a casa, sulla soglia stessa del Macellaio.',
    },
    'polish': {
        'HJS_Card': 'Wichrowy Tron',
        'HJS_OwnCard': 'Hjerim',
        'HJS_CardDescr': 'Dom w Wichrowym Tronie już należy do ciebie: akt, klucz i każdy pokój umeblowany. Zacznij w domu, na progu samego Rzeźnika.',
    },
    'czech': {
        'HJS_Card': 'Windhelm',
        'HJS_OwnCard': 'Hjerim',
        'HJS_CardDescr': 'Dům ve Windhelmu je už tvůj: listina, klíč a každý pokoj zařízený. Začni doma, na prahu samotného Řezníka.',
    },
    'russian': {
        'HJS_Card': 'Виндхельм',
        'HJS_OwnCard': 'Хьерим',
        'HJS_CardDescr': 'Дом в Виндхельме уже ваш: купчая, ключ и каждая комната обставлена. Начните дома, на пороге самого Мясника.',
    },
    'japanese': {
        'HJS_Card': 'ウィンドヘルム',
        'HJS_OwnCard': 'ヒェリム',
        'HJS_CardDescr': 'ウィンドヘルムの家はすでにあなたのもの。権利書、鍵、そして全室が家具付き。屠殺者の玄関先から、我が家で始めよう。',
    },
    'korean': {
        'HJS_Card': '윈드헬름',
        'HJS_OwnCard': '히에림',
        'HJS_CardDescr': '윈드헬름의 집은 이미 당신의 것: 증서, 열쇠, 그리고 모든 방이 가구까지 갖춰져 있다. 도살자의 문턱, 바로 그 집에서 시작하라.',
    },
    'chinese': {
        'HJS_Card': '风舵城',
        'HJS_OwnCard': '耶里姆',
        'HJS_CardDescr': '风舵城的这栋房子已属于你：地契、钥匙，每个房间都已布置好家具。从屠夫的家门口开始你的旅程。',
    },
}


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.normpath(os.path.join(here, "..", "dist", "Interface", "Translations"))
    os.makedirs(out, exist_ok=True)

    def write(lang, table):
        lines = ["$%s\t%s" % (k, table.get(k, E[k])) for k in E]
        with io.open(os.path.join(out, "HjerimStart_%s.txt" % lang), "w", encoding="utf-16-le", newline="\r\n") as f:
            f.write("﻿" + "\n".join(lines) + "\n")

    write("english", E)
    bad = 0
    for lang, table in T.items():
        missing = [k for k in E if k not in table]
        extra = [k for k in table if k not in E]
        if missing or extra:
            print("%s: missing %s extra %s" % (lang, missing, extra))
            bad += 1
        write(lang, table)
    print("wrote %d files, %d keys each, to %s" % (1 + len(T), len(E), out))
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
