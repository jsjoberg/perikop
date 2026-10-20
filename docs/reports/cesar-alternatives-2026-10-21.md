# Cesar checkpoint and alternatives

2026-10-21. Pause further source-shape tuning while evaluating existing synthesis systems. No replacement engine has been selected or integrated.

Follow-up: [Small Swedish voice comparison](compact-swedish-comparison-2026-10-21.md) now measures Cesar, Swedish Piper NST and Björn with our phonetic input, including Swedish qualification probes. It also checks KBLab's source checkpoint terms. That comparison supersedes the preliminary recommendation to prioritize Flite before an available Swedish-trained model.

Native follow-up to that comparison: Piper's nicer melody did not produce a meaningful intelligibility win. The listener prefers pursuing a tiny, non-neural Swedish system, with eSpeak/Microsoft Sam as an intelligibility target. Piper is retained as a measured comparison; it is no longer the preferred next development direction. The linked report now includes an external Swedish eSpeak listening reference and specific classical-synthesizer hypotheses to test.

## Listening checkpoint

The listener preferred the original PR voice's clarity to the LF experiments, while preferring the later compact melody. Combining those is exactly B from the milestone comparison: the original polynomial source with compact prosody. The subsequent peak-controlled version was described as “maybe best so far” with the explicit qualification that the difference was hard to hear.

Preserve both, without replacing the earlier reference-melody benchmark:

- `tests/fixtures/cesar-prosody/psalm.compact-clear.wav`: original Cesar source plus compact melody, normalized to −21 LUFS.
- `tests/fixtures/cesar-prosody/psalm.compact-peak-controlled.wav`: the same audio through an offline lookahead limiter, then normalized to −21 LUFS.
- `tests/fixtures/cesar-prosody/psalm.compact.review.json`: hashes, controls, processing provenance and qualified listening feedback.

Both contain 347,160 samples at 24 kHz (14.465 seconds). The original has no full-scale clipping. Offline limiting reduces peak amplitude from 0.467 to 0.325 at the same integrated loudness; this does not establish an audible quality improvement. FFmpeg is used only to produce this comparison asset, not as an app dependency. Production synthesis and cache identity remain unchanged.

## What was reused

Cesar already reuses the public-domain rsynth/Klatt synthesizer lineage through speech.h/SoLoud, with a port and Swedish-specific sound, transition and melody work. It was not an entirely new synthesizer. Nevertheless, the listening results do not justify assuming further local source/filter tuning is the best route to a substantially better lightweight Swedish voice.

Earlier neural-engine research is recorded in [Portable speech selection](speech-selection.md) and [Kokoro voices](kokoro-voices.md). Alice and Björn already use Swedish Kokoro. Recommending Kokoro as if it were a newly discovered alternative would miss the reason for this separate fast voice experiment.

## Initial alternatives check

These are primary-source checks, not a complete licensing audit or an audio quality ranking. Engine terms alone do not settle the terms of voices, dictionaries, weights or recordings.

| Candidate | Verified basis | Missing evidence / consequence |
| --- | --- | --- |
| [CMU Flite](https://github.com/festvox/flite) | Small C runtime; diphone and statistical voice support. [COPYING](https://github.com/festvox/flite/blob/master/COPYING) describes a permissive BSD-like runtime with component notices. | Worth evaluating for the small, fast voice requirement. No ready-made Swedish voice with verified suitable terms was established in this check. Merely mapping Swedish IPA onto an English voice would not supply missing Swedish acoustic units or prosody. |
| [Festival / FestVox](https://github.com/festvox/festival) | Existing voice-building ecosystem, with a [permissive core licence](https://github.com/festvox/festival/blob/master/COPYING). Flite's [README](https://github.com/festvox/flite/blob/master/README.md) documents its relationship to FestVox voices. | A possible Swedish voice-building route, not an immediately verified drop-in. Festival explicitly warns that individual voices and extensions have different terms. Recording/training needs must be established before choosing this path. |
| [Swedish Kokoro](https://huggingface.co/Joakim/kokoro-sv-voices) | Swedish voice model card declares Apache 2.0. Existing Alice/Björn implementation and pronunciation front end provide a naturalness control. | Already integrated; address the measured speed/size problem rather than presenting it as a new tiny engine. New voices or optimizations still need listening and hardware measurements. |
| [Piper Swedish NST](https://huggingface.co/rhasspy/piper-voices/blob/main/sv/sv_SE/nst/medium/MODEL_CARD) | Existing Swedish acoustic model; voice card identifies its training dataset as CC0. | The maintained [Piper engine](https://github.com/OHF-Voice/piper1-gpl) is GPL. The older [MIT engine](https://github.com/rhasspy/piper/blob/master/LICENSE.md) also used an eSpeak phonemization dependency. The NST card's dataset statement alone does not establish all model redistribution terms. Independent ONNX inference with our front end is an investigation, not a verified permissive package. |

[MBROLA](https://github.com/numediart/MBROLA/blob/master/LICENSE) is AGPL-3.0, so it does not meet the requested MIT-like licensing criterion as-is. This says nothing about relative speech quality.

## Next decision

Investigate Flite/FestVox's Swedish voice/data path before implementing another synthesizer or a speculative English-to-Swedish mapping. First establish the phone inventory, recordings or model needed, their terms, and whether independent pitch/duration controls are available for that voice type. If those cannot be supplied at reasonable cost, that is a reason to reject the route early.

Any viable candidate should be compared against the preserved clear compact Cesar and Björn on the same unfamiliar Swedish passages, phone/quantity contrasts and Scripture names, at matched loudness. Measure CPU time and package size separately from listening preference. Do not pick a replacement on engine reputation, an English demo or ASR scores alone.
