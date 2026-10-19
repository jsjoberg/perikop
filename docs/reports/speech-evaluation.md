# Björn evaluation findings — 2026-10-19

The evaluation distinguishes neural Björn, the earlier harmonic reconstructions,
and the failed reusable-unit Bas voice. It also detects major controlled damage.
It does **not** validate every acoustic metric as an intelligibility predictor,
or establish which implementation defect caused the failure. No synthesizer,
voice pack or product integration was changed.

[Run instructions](../../tools/speech/evaluation/README.md) ·
[Full machine-readable baseline](speech-evaluation/baseline.json) ·
[Control validation](speech-evaluation/validation.json) ·
[Pack audit](speech-evaluation/compact-pack-audit.json) ·
[Baseline compared with itself](speech-evaluation/self-comparison.json)

## Measured intelligibility

The run contains 144 recordings: neural/Bas for 50 frozen Swedish 1917 passages,
9 legacy segments, and 35 corruption controls. All completed without technical
failures. There are 40 development and 10 chapter-disjoint heldout passages.
Independent KB-Whisper Small transcribed audio without expected-text hints,
lexicon hints or prior text context. Model/executable/audio hashes, decoding
options, raw outputs, alignments and per-passage measurements are in the baseline.
The local neural probe sources match the reviewed PR's sources.

| Condition | Passages | WER | CER |
|---|---:|---:|---:|
| Neural Björn, development | 40 | 5.27% | 1.77% |
| Björn Bas, development | 40 | 118.20% | 102.83% |
| Neural Björn, heldout | 10 | 5.79% | 1.39% |
| Björn Bas, heldout | 10 | 113.29% | 89.20% |
| Legacy neural clips | 3 | 0% | 0% |
| Legacy 64-harmonic reconstructions | 3 | 0% | 0% |
| Legacy reusable Bas clips | 3 | 107.69% | 91.38% |

Values are rounded; JSON is authoritative. WER/CER can exceed 100% because
inserted/hallucinated output is retained. Raw decoder control markers such as
`<|nospeech|>` are excluded from scoring. Repeated hallucinated phrases remain
errors; they are not words present in the Bible or verified words in the audio.

Across the 50 passages, neural WER is 5.42%, Bas WER 116.79%. The paired difference
is +111.37 percentage points; its 95% chapter-cluster bootstrap interval is
approximately [+99.7, +125.9] points. All 50 Bas passages exceed 50% WER; none of
the neural passages does. Eight Bas passages have empty normalized recognition.
Both reference calibration gates pass. Curated names/titles have 2/87 expected
word errors in neural speech and 87/87 in Bas; the archaic category has 28/252
versus 249/252. These category rates omit insertions and overlap other categories.

The three harmonic reconstructions preserve recognized text in these familiar
samples. That is a useful upper control for complete-utterance resynthesis, not
evidence that a reusable synthesizer generalizes. The human report that names
were relatively best is **not reproduced as a useful ASR ranking**: the names
Bas clip hallucinates unrelated text while the Psalm Bas clip yields no speech.
Near the unintelligibility floor, ASR hallucination can dominate rankings. Small
Bas-to-Bas score changes therefore still require caution and eventual listening.

## What the controls validate, and what they miss

The first five development passages have 3.39% reference WER. All intervals use
chapter clusters; those five passages represent only three independent chapters.

| Controlled change | WER | Measured response and limitation |
|---|---:|---|
| Silence | 100.00% | All five normalize to empty ASR; acoustic silence is 100%. |
| Extensive overlaps | 83.05% | WER rises by 79.66 points; interval [66.67, 93.75]. Median duration falls by 7.52 s. This also removes/compresses speech content. |
| Alternating resampling rates | 46.89% | WER rises by 43.50 points; interval [28.13, 58.21]. Pitch-jump count does **not** reliably increase. Resampling changes pitch and timing together. |
| Clipping | 10.17% | Median near-rail occupancy rises by 25.67 percentage points. WER effect is uncertain under the conservative joint WER/CER rule. |
| 100 ms noise replacements | 9.04% | Spectral jump rate rises by a median 0.79/s. WER evidence remains uncertain. These windows are not independently aligned phonemes. |
| Three-second internal pause | 3.95% | Longest internal silence rises by a median 2.85 s; WER barely changes, appropriately for preserved words. |
| Phase/DC hard joins | 3.39% | WER barely changes and the conservative click detector misses them. Spectral jump rate changes substantially. |

This supports a multidimensional profile, not one universal quality score.
The signal-only tests also demonstrate that isolated impulses are detectable,
while moderate phase/DC joins can evade the waveform-step threshold.

On the actual 50-passage speech, Bas has roughly twice the median spectral jump
rate (2.23/s versus 1.19/s), more pitch-jump candidates (0.73/s versus 0.40/s),
less silence (10.5% versus 21.6%), and a median paired duration ratio of 1.208.
Bas has no near-rail samples. Both voices have roughly 53–54% voiced frames and
zero median detected waveform clicks. Those latter statistics fail to separate
usable and unusable speech. The understandable harmonic controls have only
13.1% median voiced-frame occupancy and essentially zero spectral jump rate:
matching neural voicing/flux is **not** a validated intelligibility objective.
Two neural passages have small pre-PCM overloads; reference encoding mirrors the
reader's clamping rather than introducing benchmark gain normalization.

## Observed implementation changes

The relevant history is
[`8901758`](https://github.com/jsjoberg/perikop/commit/89017585c4f5e84e807a41a737e2df7b666d1b35)
(whole-utterance harmonic fitting),
[`847cc2e`](https://github.com/jsjoberg/perikop/commit/847cc2e42bbdc35c1eef004df21d0a39c166e18b)
(reusable waveform units), and
[`35e1b80`](https://github.com/jsjoberg/perikop/commit/35e1b807235c16b1e013b395b246c1fd9f9695e5)
(failed listening review and explicit experimental selection).

The original method fitted time-varying pitch, harmonic amplitudes/phases and
noise bands directly to each complete waveform. The later renderer instead
assembles independently extracted diphone/monophone waveforms. These are
fundamentally different tasks and representations; adding arbitrary-text input
alone is not an established causal explanation.

Read-only inspection of `prepare_voice.py` and `compact_voice.cpp` establishes:

- One selected waveform per key: 2,139 entries, including 85 monophones. Selection
  primarily favors a target span duration, with no contextual target/join search.
  Audited stored core spans have median 87.5 ms, 5th–95th percentiles 50–125 ms.
  These are diphone spans, **not independently verified phone durations**.
- Cuts come from teacher token-duration predictions, with stress/length handling,
  whitespace extension and a global mapping from duration sums to waveform length.
  None establishes that the intended acoustic phone boundaries were found.
- Runtime concatenates word phone IDs until punctuation; it does not preserve
  explicit word/syllable boundaries or consume target sentence prosody/durations.
  Source timing and pitch come from unrelated training contexts.
- The new 50-passage Bas run has zero spelled/unknown words and 141 transition
  backoffs across 2,639 words. Lexical coverage alone is an insufficient account
  of such widespread failure. This does not verify the phone identities inside
  selected waveform fragments.
- Pack checksums and bank layout are valid. The bank omits the winning source
  batch/span for each unit, preventing a direct audit of selected cut locations
  from the committed bank alone.

## Competing hypotheses and prioritized experiments

1. **First: same-utterance extraction/reassembly control.** On a small frozen
   development subset, retain exact source-span/phone/word/join provenance. Compare
   intact teacher PCM; exact contiguous cut/rejoin as a null control; the same
   utterance reassembled through the extraction/companding/join path; and units
   drawn from other utterances. Hold phone sequence, intended timing and other
   processing fixed as far as each contrast permits. Measure WER/CER and perform
   a small blind listening/transcription check. Failure within the same utterance
   implicates cuts, mapping, companding or joins before contextual selection is
   blamed; success there with cross-utterance failure shifts attention toward
   context, timing and prosody. Regenerating audited provenance is preparation,
   not a runtime architectural redesign.
2. **Independently audit boundaries.** Manually check or independently align a
   small source subset, including vowels, stop closures/releases, sje/tje sounds
   and cross-word transitions. Compare predicted cuts with audited cuts while
   keeping selection, timing and joins unchanged. A small boundary-shift sweep
   can test sensitivity, but lower reconstruction error alone is insufficient.
3. **Separate lexical and boundary retention effects.** Check existing native
   frontend tokens against the compact lexicon/phone plan on the dev subset;
   then preserve precomputed word/syllable boundaries without changing units.
   Pronunciation/corpus overrides differ between adapters and should be controlled.
   No ASR target text or G2P-derived phones should become recognizer hints.
4. **Test contextual selection and longer units.** If audited within-utterance
   assembly is understandable, retain multiple candidates with source-context,
   energy, pitch and boundary spectra. Compare a fixed target/join cost against
   the one-candidate bank. Freeze pronunciation and prosody for this comparison.
5. **Test precomputed target duration/prosody.** Independently change durations,
   then sentence pitch/energy instructions on held-out-from-tuning connected
   speech. The known corpus permits build-time preparation; a general Swedish
   runtime frontend is unnecessary. These controls should distinguish timing
   defects from waveform/unit-content defects.
6. **Isolate join changes only after those controls.** Use traced join locations
   and stable voiced regions, including phase/pitch mismatch and overlap amount.
   Legitimate stop transitions need separate interpretation. Existing smooth-join
   tests and zero detected clicks already fail to predict intelligibility.

These are proposed experiments, not implemented synthesis improvements or proven
root causes. Boundary errors, unit selection, missing boundaries and prosody can
interact; the current evidence cannot uniquely assign their contributions.

## Limits and stopping point

The code provides a reproducible command, immutable baselines, complete per-passage
reports, independent ASR, actual corruption validation and conservative comparison
rules. Identical reports produce **uncertain** in both splits, not a claimed
improvement. Changed corpus/scoring/ASR or incomplete coverage prevents a favorable
comparison. Probable improvements require a chapter-bootstrap WER interval below
-2 points with CER agreement; this practical threshold is provisional.

The 25% neural reference gate is deliberately loose and provisional; actual
reference WER is far lower. ASR errors, language-model hallucination, familiar
Bible text, a single recognizer and only ten heldout passages limit conclusions.
No numeric blinded human transcriptions were available. The controls validate
large degradations more strongly than subtle near-floor Bas improvements.
A second independent recognizer and occasional blind listening remain valuable.
No synthesis improvement, acceptable voice identity or release readiness is claimed.

**Stop here. The first recommended synthesis investigation is experiment 1.**
