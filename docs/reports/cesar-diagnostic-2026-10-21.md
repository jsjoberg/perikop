# Cesar diagnostic listening test

2026-10-21. A native listener heard 22 short items from Cesar (production settings), typing what they heard before seeing the text. Björn rendered the same front-end output as a reference. The page and audio come from [`tools/speech/cesar_diagnostic.py`](../../tools/speech/cesar_diagnostic.py). Raw answers are in [data/cesar-diagnostic-2026-10-21.json](data/cesar-diagnostic-2026-10-21.json).

## Result

- **Our pronunciation data is not the bottleneck.** Björn was right on all 22 items with the same front-end output. Cesar failed most of them.
- **Cesar is largely unintelligible on unfamiliar text.** Of four nonsense sentences, two were mostly misheard. One was half right, and one was recognized only after the text was shown.
- **Vowel length is mostly right.** glas/glass and väg/vägg were heard correctly. tak/tack came through as tag/tagg: right length, wrong consonant.
- **Consonants fail most, then the u-like vowels, then clicks.**

So a better lexicon (for example Braxen) would not fix what the listener heard. The work is in Cesar's sound rules.

## Confusions

| Pattern | Examples (said → heard) |
| --- | --- |
| r heard as l | Herren → hälen, Herrens → hälens |
| Voiceless stops heard voiced or at another place | tak, tack → tag, tagg; bok → ord; kör → kyl |
| Other consonant place errors | hus → kis (h → k), simmar → sinna (m → n), sil → te (s → t) |
| Retroflexes come out plain or as r | barn → ban, bord → bod, fors → forr |
| ɧ heard as German/English sch | sjö, sju, skjorta → sche, schy, schuta |
| u-like vowels | sul → tju, hus → kis, hund → kund, bok/bock → ord/båt |
| Clusters lost | gröna → mina, stolen → solen |
| Clicks | noted on three of the four nonsense sentences |
| Rhythm | "stressed" on the length pairs, too long a pause at commas in lists |

Names and numbers failed for the same reasons, not because of their transcriptions: Getsemane → rese vande, "Kapitel 23" → "han heter kilo".

## Causes visible in the code

From [cesar_voice.cpp](../../src/speech/cesar_voice.cpp) and probe traces:

1. **r is an l.** The Swedish r is built from rsynth's L element with a 2 dB amplitude dip. In *kyrka* it lasts 85 ms. A Swedish tap needs a brief, real closure.
2. **Aspiration is too short.** In stressed onsets the aspiration lasts 12–25 ms (*tack* 12 ms, *kyrka* 18 ms). Swedish voiceless stops are strongly aspirated, much longer than that. That matches hearing k and t as g and d. A published Swedish VOT value is still to be sourced.
3. **Consonant transitions come from English rsynth.** Each vowel's loci are set to half its own formant values. Place errors such as k/t, m/n and h/k point at these transitions.
4. **Retroflexes are only a lowered F3 on the alveolars.** That was too weak to hear.
5. **ɧ uses rsynth's X element**, a velar/uvular fricative. Swedish ɧ is a different sound.

The vowel targets look like published Swedish measurements; /iː/ 255/2190/3150 Hz resembles Fant's figures, but this is unverified. So retuning vowel formants alone is unlikely to fix the vowel errors. Several of those are heard next to failing consonants, so fix the consonants first and then test the vowels again.

## Front-end notes (secondary)

Björn coped with all of these, but they affect Cesar's melody and stress rules:

- Every word carries primary stress, including function words (`ˈɛt`, `ˈiː`, `ˈɔk`). Cesar gives each one stressed duration and a pitch accent.
- The IPA has no word-accent marks. Cesar assumes accent 2 for any primary stress before the last syllable, so accent-1 words (*anden* the duck) cannot come out right.
- There are a few lexicon choices to check: *den* `dˈeːn`, *min* `mˈiːn`, *sjön* `ɧˈœn`.

## Next steps

1. Fix one consonant mechanism at a time, using published Swedish reference values: the r closure first, then aspiration length, then retroflexes and ɧ. Record each source.
2. **Use new blind items for every round.** The listener has now heard these texts, so they cannot be used blind again. Keep the minimal pairs and add fresh nonsense sentences.
3. Then re-test the vowels, clicks and rhythm. Function-word stress is a separate change in Cesar's prosody, not in the lexicon.

## Limits

One listener, one session, 22 items. Categories were mostly given as notes rather than ticks, so the patterns above come from comparing what was typed with the text. No code change was made in response yet.
