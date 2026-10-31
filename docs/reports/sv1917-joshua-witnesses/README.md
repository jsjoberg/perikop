# Josua: kompletterande originalskanningar

EFS Stockholm 1926, Internet Archive `bibelnellerdenhe0000unse`. De vanliga sidbilderna klipper text nära bokryggen. Originalkamerans bild av den motstående sidan visar de saknade tecknen. Dessa tre JP2-filer är oförändrade utdrag ur arkivets `_orig_jp2.tar`; PNG-filerna är förstoringar av riktade läsutsnitt och ger ingen extra kapitelgranskning.

Rotera JP2-filerna 90 grader medurs (Pillow `rotate(-90, expand=True)`) för att läsa. Utsnittskoordinaterna nedan avser den roterade bilden på 4000 × 6000 pixlar. Originalfiler och läsutsnitt har separata SHA-256 i [bokliggaren](../sv1917-verification-joshua.json).

| Original | Utsnitt | Koordinater (vänster, övre, höger, nedre) | Beslut |
| --- | --- | --- | --- |
| [orig0291](ia-orig0291.jp2) | [6:8](orig0291-6-8-last.png) | 215, 4500, 315, 4640; förstorat 8× | 1926 har **präster** utan -na och utan komma; den befintliga läsningen behålls mot 1927:s **prästerna,**. |
| [orig0303](ia-orig0303.jp2) | [14:1](orig0303-facing-detail.png) | 0, 4060, 350, 4350; förstorat 3× | Komma efter **arvslotter** syns; samma rättelse stöds av 1927. |
| [orig0305](ia-orig0305.jp2) | [15:16](orig0305-resolved.png) | 0, 2260, 340, 2480; förstorat 3× | Hela **den** och blank marginal syns utan komma; korpusens **Åt den som** behålls mot 1927. |

Källadresserna och exakta filhashar finns i JSON-rapportens `original_camera_witnesses`. Den vanliga n-numreringen är ett mindre än dessa originalkameranummer: original 0291 visar främst sidan n290 samt kanten av n289, där 6:8 står. Original 0303 visar främst n302 samt kanten av n301; original 0305 främst n304 samt kanten av n303. En motstående sidbild ger endast evidens för de uttryckligen beskrivna läsningarna.
