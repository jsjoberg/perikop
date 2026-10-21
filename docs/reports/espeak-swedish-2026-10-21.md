# eSpeak NG with Perikop's Swedish pronunciation

2026-10-21. After the [Cesar diagnostic](cesar-diagnostic-2026-10-21.md) stopped rule tuning, the listener compared upstream `espeak-ng -v sv` reading our pronunciation data with eSpeak's own Swedish rules. Their verdict: "our rules are way superior". This report records the choices since then.

## Decisions

- **Engine:** eSpeak NG's default synthesizer. In round 4 it was the "clearest and cleanest", with no clicks.
  - `klatt 6` is not the classic Klatt source. It is NVDA's SpeechPlayer, compiled in only when eSpeak is built with it, and the listener heard Cesar-like clicks in it.
  - `klatt 3` and a 5 kHz low-pass on klatt6 were also rejected.
- **Voices:** stock `sv` (male) and `sv+f2` (female), at 150 words/min. The listener found 175 too fast and 140 too slow.
- **Only documented eSpeak behaviour.** No custom voice files or linked data folders. Power users get an advanced field for documented settings.
- **Pronunciation:** the front end's IPA converted by [`tools/speech/espeak_swedish.py`](../../tools/speech/espeak_swedish.py), with function words unstressed. All 47 lexicon phones round-trip through eSpeak 1.52.0.
- **Distribution:** Perikop ships no eSpeak code or data. The user installs eSpeak NG; Perikop loads the installed library at run time and stays MIT. An iPhone port would leave eSpeak out.

[`tools/speech/espeak_variants.py`](../../tools/speech/espeak_variants.py) renders each listening round; its history holds rounds 1–5.

## Measurements (Apple Silicon, eSpeak NG 1.52.0)

| Path | Result |
| --- | --- |
| One `espeak-ng` process per call | 8 ms for one word; Psalm 23:1–3 (16 s of audio) in 23 ms |
| Library in-process (`espeak_Initialize`, `AUDIO_OUTPUT_SYNCHRONOUS`), one `espeak_Synth` per verse with `espeakPHONEMES` | 13.2 s of audio in 17 ms, about 780× real time; initialisation about 1 ms |

The listener rejected per-sentence processes as unclean, so the plan is to load the library in-process.

One call per verse gives verse boundaries without markers. SSML `<mark>` events lost text in a quick test, because `&` (eSpeak's æ) must be escaped as `&amp;`, and returned only the first mark. That path is not needed.

## Findings to keep

- eSpeak's `r` is English [ɹ]; Swedish r is `R`. eSpeak has no codes for retroflex t and d, so the converter writes `Rt`/`Rd`, as eSpeak's own Swedish rules do.
- An unknown phoneme code inside `[[...]]` silently drops the rest of the word, which is why the converter self-checks every phone.
- eSpeak does not load a variant file from a path. It also lists at most 349 voices and silently drops the rest.
- The default synthesizer honours breath and tone settings; klatt6 ignores breath, tone, roughness and flutter.
- Lexicon errors noticed along the way: *du* `dyː`, and *han*/*hon* with long vowels.

## Pronunciation data needed without the neural pack

All bundled Bible text resolves through `lexicon.tsv`, `custom_lexicon.tsv` and `g2p-corpus.tsv`, about 0.9 MB. The pack's neural G2P loads only for words outside the corpus. An eSpeak-only installation could fall back to eSpeak's own rules for those words.

## Not done

- No in-app integration yet: no library loader, no C++ converter, no settings UI, no packaging changes.
- Library names and paths are unverified on Windows and Linux, and so are older distribution versions (1.50, 1.51).
- The stock-voice round (round 5) still awaits listening.
