# Upstream engine reset and Swedish support

2026-10-21. The native listener found the latest pulse/control smoothing difference very small and proposed restoring upstream synthesis plus an existing upstream Swedish implementation. Stop treating local source tweaks as demonstrated progress. No production voice has been replaced by this investigation.

## What upstream actually means here

Our lineage is rsynth → SoLoud → Mattias Gustavsson's [speech.h](https://github.com/mattiasgustavsson/libs/blob/main/speech.h) → the original Enkel wrapper → Cesar's local port. Klatt is the synthesis method, not a complete language implementation or a Swedish voice pack. The original wrapper already supplied our own approximate Swedish acoustic targets, donor selection, consonant adaptations and duration rules.

The archived header at project commit `eda1b01` is byte-identical to upstream speech.h at `bc03ac2268db88d1fadfbe4cee5ec1d55f715edc`, SHA-256 `9368a13b44b50bbcfd5ec10e82bf0472fafecc74f6c07740abfa702d7bd3f726`. Its license footer describes public-domain lineage and makes Komppa's changes public domain/CC0. This is a provenance statement from that file, not a claim that every modern rsynth fork has the same terms: [rhdunn/rsynth](https://github.com/rhdunn/rsynth) has GPL/LGPL notices.

The original header initializes synthesis at 11,025 Hz, with 110 samples per nominal 10 ms frame. The original project wrapper changed it to 24 kHz. Cesar later changed source open time, filter coefficients, formant gains and upper resonances, amplitude handling, control resolution, articulation tables, durations and pitch. Reverting only the latest opt-in smoothing would not restore upstream; production was never using those experimental switches anyway.

[SoLoud's own documentation](https://solhsa.com/soloud/speech.html) describes its speech engine as “barely legible” and explicitly prioritizes a simple available synthesizer. Restoring this implementation is a useful rollback reference, not evidence of an eSpeak-quality foundation.

## Existing Swedish implementation search

| System | Verified result | Consequence |
| --- | --- | --- |
| speech.h / SoLoud speech | English text/phoneme tables; no ready Swedish acoustic/prosody pack found in the checked upstream sources. | A reset still needs a Swedish layer. Our old adapter must not be described as upstream Swedish. |
| rhdunn/rsynth | Recursive upstream inventory contains English and German language sources, English Festival voice wrappers, and no Swedish implementation. | No Swedish drop-in established. The modern fork's terms also differ from speech.h's stated provenance. |
| [KlattSyn](https://github.com/chdh/klatt-syn) | MIT TypeScript implementation of a cascade/parallel parameter synthesizer. Its documented interface concerns acoustic parameters, not an included Swedish text/phone/prosody stack. | A candidate engine, not a verified Swedish solution. Changing to it alone would repeat the language-layer work. |
| KTH RULSYS / OVE / GLOVE | Swedish formant synthesis is well documented in [KTH material](https://www.kth.se/social/upload/52f352c8f276541e730edc77/DT2112_speech_synthesis_2014.pdf). No complete redistributable Swedish rule/voice package with verified MIT-like terms was established. | Research and historical commercial systems do not supply a ready permissive package. This is a bounded search result, not proof that no such implementation exists. |
| [eSpeak NG](https://github.com/espeak-ng/espeak-ng/tree/1.52.0) | Existing upstream Swedish language, phone data, language rules, timing and synthesis. Both its normal formant backend and `sv+klatt` work in installed version 1.52.0. | This is a concrete tiny, non-neural Swedish stack. Its [GPL-3 terms](https://github.com/espeak-ng/espeak-ng/blob/1.52.0/COPYING) do not satisfy the earlier MIT-like selection criterion. It is an external listening reference here, not a new bundled dependency. |

The earlier Flite/Festival/HTS searches and native rejection of Piper remain recorded in [the compact Swedish comparison](compact-swedish-comparison-2026-10-21.md). No newly verified permissive Swedish voice was discovered in this follow-up.

eSpeak supports explicit [phoneme input](https://espeak.sourceforge.net/commands.html), so using it need not inherently discard the project's pronunciation data. A Swedish IPA-to-eSpeak phone adapter would still need to preserve stress, vowel/consonant quantity, retroflexes, sj/tj and intentional overrides, and reject unsupported phones. **The present eSpeak samples use its own text frontend**, so they do not establish that adapter's correctness. Its Swedish data cannot simply be copied into an MIT implementation to avoid the licensing issue.

## Reconstructed listening comparison

```sh
.venv/bin/python tools/speech/cesar_upstream_comparison.py
```

This builds an isolated probe under `build/cesar-upstream-comparison/source/` from archived project sources. The header is unmodified and hash checked. The original project Swedish wrapper changes only its output-rate constant from 24,000 to 11,025; its existing rate-dependent initialization then reproduces upstream's native settings. The probe refuses unsupported IPA rather than silently dropping it. It bypasses speech.h's English frontend and public API's output interpolation, invoking its original internal synthesizer as the historical project wrapper did.

Four cases (Psalm, unfamiliar prose, quantity contrasts and Swedish sounds) have four listening copies each:

- `cesar.wav`: clear original Cesar source with the compact melody and frozen project IPA.
- `upstream.wav`: original speech.h core at its native rate, same frozen IPA, historical project Swedish adapter and timing/melody.
- `espeak.wav`: upstream Swedish eSpeak with its normal backend, text frontend.
- `espeak-klatt.wav`: same Swedish eSpeak text frontend with its existing Klatt variant.

The pairs are matched by integrated loudness using constant gain. Shared headroom adjustment avoids adding a limiter: actual comparison levels are −23.4 LUFS for Psalm, −26.1 for prose, −23.0 for quantity and −24.5 for Swedish sounds, within 0.1 LU among variants in each case. There is no resampling, time stretching, EQ or compression. All raw renders have zero PCM rail samples. This does not establish the absence of audible artifacts.

| Passage | Cesar | Restored speech.h + old adapter | eSpeak Swedish | eSpeak Swedish + Klatt |
| --- | ---: | ---: | ---: | ---: |
| Psalm | 14.465 s | 17.101 s | 14.272 s | 14.430 s |
| Prose | 8.678 s | 9.668 s | 7.624 s | 7.627 s |
| Quantity | 5.202 s | 5.308 s | 4.730 s | 4.666 s |
| Swedish sounds | 6.732 s | 6.924 s | 7.132 s | 7.044 s |

The restored wrapper changes timing and melody as well as the source/filter path. This comparison answers what a historical upstream reset actually supplies; it does not isolate which individual local change caused a sound-quality difference. Native listening is pending. No unsupported claim of improved quality is made.

The script compiled the archived probe and rendered all 16 WAVs successfully. It checks header identity, phone coverage, native sample rates, signal rails and loudness matching, and records input/audio hashes in [the diagnostic record](data/cesar-upstream-2026-10-21.json). Since no application or production synthesis source changed, this research does not require rerunning the app's previously passing native suite.

## Decision

Preserve the accepted checkpoints and audition the actual reset before deleting working code. There is no verified “restore Klatt, add upstream Swedish” package under the current permissive-license requirement. eSpeak Swedish, including its Klatt option, is the verified existing classical stack; choosing it for the product requires revisiting that requirement and validating a phone adapter. If the permissive requirement stays, the missing piece remains a Swedish acoustic/articulation/prosody implementation, not another small oscillator correction.
