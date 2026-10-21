# eSpeak NG voices with Perikop's Swedish pronunciation

2026-10-21. Perikop gets a small, CPU-light Swedish voice option: the user's own eSpeak NG reading Perikop's curated pronunciations. Alice and Björn are unchanged. The research history (Cesar, Clustergen, listening rounds) is in draft [PR #33](https://github.com/jsjoberg/perikop/pull/33), which is not merged.

## Why eSpeak

- **Cesar was stopped.** A blind native listening test of Cesar, our own Klatt-based formant voice, made one thing clear. Björn, given the same pronunciation input, was right on all 22 items, so our pronunciation data is good. Cesar was largely unintelligible: r was heard as l, voiceless stops as voiced, retroflexes as plain consonants, and there were clicks. The first two rule fixes, a tapped r and Swedish aspiration, were rated worse.
- **Clustergen was dropped.** The CMU Wilderness Swedish voices had no downloadable model. Their training audio comes through Bible Brain, whose licence does not cover training or redistribution.
- **Our pronunciation beats eSpeak's own rules.** Upstream `espeak-ng -v sv` reading Perikop's pronunciations was judged "way superior" to eSpeak's own Swedish rules.

## Decisions

- eSpeak's **default synthesizer**, with the stock voices `sv` (male) and `sv+f2` (female) at **150 words/min**.
  - `klatt 6` (NVDA's SpeechPlayer, compiled in only optionally) produced Cesar-like clicks.
  - The listener found 175 too fast and 140 too slow.
- **Only documented eSpeak behaviour.** No custom voice files and no data-folder tricks.
- **Perikop ships no eSpeak code or data**, so it stays MIT. The user installs eSpeak NG, and Perikop loads its library at run time.
  - An iPhone build would leave eSpeak out. A Developer ID build with the hardened runtime would need the `com.apple.security.cs.disable-library-validation` entitlement.
- **Function words are unstressed in the converted input.** The front end stresses every word.

## Implementation

- [`src/speech/espeak_voice.cpp`](../../src/speech/espeak_voice.cpp) declares the documented API locally and loads the library from Homebrew, `/usr/local` or MacPorts on macOS, `libespeak-ng.so.1` on Linux, or the folder in `HKLM\SOFTWARE\eSpeak NG\Path` on Windows.
- It initialises with `espeakINITIALIZE_DONT_EXIT`. Without it, eSpeak ends the whole process when its data is missing.
- Synthesis is synchronous and in-process: one call per text, resampled from 22,050 Hz to 24 kHz.
- Codes come from eSpeak 1.52's Swedish tables:
  - Swedish r is `R`, because eSpeak's plain `r` is English.
  - Retroflex stops are written `Rt`/`Rd`, as eSpeak's own rules write them.
  - An unknown code inside `[[...]]` silently drops the rest of the word, so a test requires a code for every lexicon phone.
- Portable speech uses only the voice pack's pronunciation files for eSpeak, never the neural model. Audio is cached under `espeak/<version>/<voice>/<rate>/v1`.
- Settings list every installed, loadable Swedish eSpeak voice and variant ("eSpeak – <name>"); a saved voice that is no longer installed stays selected and is marked unavailable. MBROLA voices are left out, since they change eSpeak's sample rate. When no library loads, Settings show an installation link: Homebrew's formula on macOS, eSpeak NG's guide elsewhere.

## Measurements (Apple Silicon, Homebrew eSpeak NG 1.52.0)

| Check | Result |
| --- | --- |
| Psalm 23:1–3, in-process, one call per verse | 13.2 s of audio in 17 ms (about 780× real time) |
| Same text through `espeak-ng -z` | Identical duration; band energies within 0.03 dB; −56 dB above 11.1 kHz after resampling |
| Repeat synthesis | End moves by about 0.5 ms; eSpeak varies its voice source at random |

## Not done

- Windows and Linux loading (including the DLL name) and older eSpeak versions (1.50, 1.51) are unverified.
- The missing-eSpeak hint has not been seen on screen; the test Mac has eSpeak installed.
- There is no advanced field for documented eSpeak settings yet.
- The pronunciation files still live in the Alice/Björn pack. Moving them into the base app, and making Alice/Björn a download, is the next PR.
- Lexicon errors noticed along the way: *du* `dyː`, and *han*/*hon* with long vowels.
