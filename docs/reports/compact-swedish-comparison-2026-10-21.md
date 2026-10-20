# Small Swedish voice comparison

2026-10-21. Compare small bundle size, low CPU cost, usable Swedish speech and control through our pronunciation data. A permissive English engine is not considered a Swedish solution. No product voice has been replaced.

## Result

Native listening follow-up: Piper had a nicer melody, but the listener found it about as difficult to understand as Cesar and was not impressed by it. Its measured size/CPU advantages over Björn therefore do not justify selecting it. The product direction is a tiny, non-neural Swedish voice with intelligibility comparable to eSpeak or the clarity associated with Microsoft Sam. The earlier recommendation below records what the measurements suggested before that listening feedback; Piper is now parked as a comparison, not the preferred development path.

Piper NST is the strongest **available Swedish-trained candidate to qualify**, rather than a demonstrated listening winner. Its measured CPU cost is about one-eighth of Björn's and its acoustic model is substantially smaller. It remains much larger and more memory-hungry than Cesar. Flite/FestVox and HTS have appropriate engine architectures and permissive runtimes, but a verified, redistributable Swedish voice compatible with our input is still missing from this comparison. Their English voices do not establish suitability.

The next decision depends on native listening to the Swedish probes below. Symbol coverage and benchmark speed cannot establish correct Swedish realization, quantity, accent or flow.

## What was actually measured

All inference ran locally on the Apple Silicon development Mac, CPU only, using ONNX Runtime 1.23.2 for both neural models. Each shape had a warm-up and three measured generations; figures below are medians. Native Cesar was called through a small research C ABI. The front end had already produced the phonetic input, so normalization, dictionary lookup and unknown-word G2P cost are excluded for every engine. Björn includes its native-equivalent decoder notch cleanup.

The common Psalm text produces different natural durations; no engine's timing is forced onto another engine. CPU time includes all worker threads. It is distinct from elapsed time.

| Configuration | Acoustic component on disk | Psalm audio | Generation elapsed | CPU time | CPU seconds / audio second |
| --- | ---: | ---: | ---: | ---: | ---: |
| Cesar, clear original source + compact melody | 0.088 MB research native library | 14.47 s | 0.010 s | 0.010 s | 0.00066 |
| Piper NST, 1 thread | 63.10 MB model | 14.32 s | 0.945 s | 0.945 s | 0.06574 |
| Piper NST, 4 threads | same | 14.27 s | 0.354 s | 1.210 s | 0.08477 |
| Björn, 1 thread | 166.24 MB model + 0.52 MB style | 13.65 s | 7.526 s | 7.525 s | 0.55127 |
| Björn, current 4-thread setting | same | 13.65 s | 2.538 s | 9.650 s | 0.70697 |
| Piper NST, existing clauses separately, 1 thread | same 63.10 MB model | 17.37 s | 1.123 s | 1.123 s | 0.06473 |

MB means decimal megabytes; these are uncompressed acoustic components, **not complete app bundle sizes**. Cesar's figure includes its source and melody code but excludes the shared pronunciation front end. The current Alice/Björn pack totals 179.46 MB. Keeping the same dictionaries and optional neural G2P fallback would put an analogous Piper asset pack near 75.3 MB; that pack has not been built. The existing macOS ONNX Runtime library adds 35.14 MB to the application and is already shared. A standalone tiny Cesar pack would require separating the current shared pack/front-end dependencies; it is not an already implemented sub-megabyte app distribution.

One Piper thread is a better low-total-CPU starting point than four in this test. Four reduces waiting time but spends more CPU. Measurements on this Mac do not establish performance on older Intel or Windows/Linux machines.

### Memory is a separate constraint

Each configuration ran in a separate process to prevent previous models contaminating its peak. Every process imported the same Python/numpy/scipy/ORT environment first, accounting for roughly 111 MiB. Peak RSS is the maximum over the complete seven-case suite, not model file size or an app RAM measurement.

| Configuration | Whole research process peak RSS | Above import baseline, approximately |
| --- | ---: | ---: |
| Cesar native library | 120 MiB | 9 MiB |
| Piper, 1 thread, whole input | 678 MiB | 567 MiB |
| Piper, 4 threads, whole input | 701 MiB | 591 MiB |
| Björn, 1 or 4 threads | 889 MiB | 779 MiB |
| Piper, existing clauses separately | 376 MiB | 266 MiB |

Shorter model requests reduce Piper's workspace substantially. This variant splits only at existing commas, semicolons and sentence punctuation, preserves all IPA characters in order, adds no fixed silence, and concatenates the model outputs. It introduces additional model boundaries. The Psalm becomes roughly three seconds longer, so this is **not a free memory optimization**: pauses, pitch resets and Swedish flow may change. Its audio is kept separately, with whole-input Piper as the main candidate. No quantization or hypothetical smaller Swedish model is credited with unmeasured benefits.

## Our phonetic data is the input

The suite was exported from the existing native `KokoroText::ipa()` using pack `kokoro-sv-alice-bjorn-2c7968d-v3`. This is the NST IPA **before** Kokoro-specific remaps. Cesar and Piper receive those IPA strings; Björn receives the existing native front end's corresponding model tokens.

Piper inference here uses our small phoneme-ID adapter and MIT ONNX Runtime. It does not invoke the Piper engine, an eSpeak phonemizer, an English G2P, or a text-regeneration fallback. Unknown symbols fail rather than disappear. The only explicit representation expansion is the affricate ligature `ʧ` to `tʃ`. The model's ID map then covers every symbol in the pack's lexicon, custom lexicon and precomputed G2P data. In particular, this path does not apply Kokoro's `ɧ→ʂ`, `ʉ→ɨ`, `ɵ→ɜ` or `ɭ→l` remaps to Piper.

This establishes **input transport**, not correct acoustic realization. The Piper configuration was trained for eSpeak Swedish conventions. An ID can exist without being well represented in Swedish training, and identical-looking symbols do not guarantee identical allophones or durations. The installed eSpeak NG 1.52.0 diagnostic differs from our input in, for example:

| Word / context | Our IPA | Installed eSpeak diagnostic |
| --- | --- | --- |
| själ | `ɧˈɛːl` | `sxˈɛːl` |
| barn | `bˈɑːɳ` | `bˈɑːrn` |
| bord | `bˈuːɖ` | `bˈuːrd` |
| pärla | `pˈɛːɭa` | `pˈɛrla` |
| ful | `fˈʉːl` | `fˈʉl` |

That installed phonemizer is not established as the 2023 training version. These differences identify an adapter/realization audit, not a validated automatic rewrite. The actual diagnostic does not support a blanket rule of adding long consonants to every short-vowel word; the earlier conversational suggestion of that rule was too specific. The main listening renders deliberately retain our input rather than inventing another broad Swedish repair rule.

KBLab's own [release article](https://kb-labb.github.io/posts/2023-05-24-swedish-text-to-speech/) identifies incorrect Swedish eSpeak conversion as the cause of some model mispronunciations. This makes our curated phonetic data valuable, while leaving the task of matching the acoustic model's conventions. It does not mean the language gap has disappeared.

### Swedish qualification probes

The comparison includes seven cases, not just the repeatedly tuned Psalm:

| Case | Purpose / outstanding question |
| --- | --- |
| Psalm | Familiar control; retain the original clear Cesar source with compact prosody. |
| Unfamiliar prose and a question | Phrase flow and question contour outside the tuning sample. |
| tak/tack, vila/villa, glas/glass, väg/vägg | Vowel and consonant quantity. A symbol-level pass is insufficient. |
| sil/syl/sul, föl/ful/full, själ/kär, barn/bord/hörn/pärla | Rounded-vowel distinctions, sj/tj and retroflex realization. |
| anden and tomten in contrasting contexts | Accent-1/accent-2 ambiguity. Our present input gives the same word transcription in both senses; none of these paths offers reliable explicit sense-dependent accent control from that data alone. |
| Scripture names | Pronunciation overrides and rare forms. The exported front-end predictions are not declared approved by a native listener. |
| Explicit `⟦kˈat⟧` and `⟦hˈɵnd⟧` | Verify that the pipeline consumes forced phones. Actual realization still needs listening. |

The [2025 KTH/MTM study](https://www.speech.kth.se/~jocke/publications/tannander25_ssw.pdf) specifically studies Swedish stress, accent and boundaries and finds the input distinctions matter especially for difficult and low-frequency text. It supports this targeted evaluation method, not a quality claim for our Piper adapter. Lexical accents, compounds and reduction/prominence remain separate problems from mapping the basic vowel inventory.

## Existing-engine comparison with Swedish as a requirement

| Candidate | Engine / voice evidence | Decision for this target |
| --- | --- | --- |
| Cesar | Existing public-domain rsynth/Klatt lineage; our Swedish inventory and melody. Tiny acoustic implementation, directly controlled by our IPA. | Keep as the tiny baseline. Listening still exposes synthetic timbre and prosody limitations. |
| Piper NST via independent ONNX inference | Actual Swedish-trained model, measured above; all phone input supplied by our front end. KBLab marks its [source checkpoint CC0](https://huggingface.co/KBLab/piper-tts-nst-swedish); the [converted voice repository](https://huggingface.co/rhasspy/piper-voices/tree/main/sv/sv_SE/nst/medium) declares MIT and records the NST provenance. | Concrete smaller/lower-CPU alternative to Björn. Qualify the Swedish adapter and listening before product integration. The maintained GPL Piper engine is not used in this experiment. |
| Flite / FestVox / Festival | Permissive [Flite runtime](https://github.com/festvox/flite/blob/master/COPYING) and [Festival core](https://github.com/festvox/festival/blob/master/COPYING). A [maintainer discussion of a Swedish Festival voice](https://github.com/festvox/flite/issues/17) explicitly describes the missing Flite language/front-end conversion work. | A voice-building route, not a verified ready-to-use Swedish package. Our front end helps, but the acoustic inventory, voice conversion, duration/prosody models and data terms still have to match. Do not rate it from US English voices. |
| CMU Wilderness Swedish | Swedish entries are referenced by the Flite maintainer, so Swedish work does exist. The [dataset project](https://github.com/festvox/datasets-CMU_Wilderness) says it cannot redistribute the original Bible audio. | No verified permissive distributable voice package established. An index/demo or a permissive engine licence is not a licence for the required recordings or voice. |
| HTS / hts_engine | [Modified BSD runtime](https://hts-engine.sourceforge.net/); [Swedish HTS research exists](https://fileadmin.cs.lth.se/nlp/sltc2012/proceedings/sltc2012proceedings.pdf). | Plausible compact statistical architecture, but this check did not establish a downloadable Swedish voice with suitable redistribution terms and matching context-label front end. Training/importing a Swedish voice is real work, not substituting IPA into an English model. |
| eSpeak NG / MBROLA | Existing multilingual synthesis; [GPL eSpeak](https://github.com/espeak-ng/espeak-ng/blob/master/COPYING), [AGPL MBROLA](https://github.com/numediart/MBROLA/blob/master/LICENSE). | Not candidates under the requested MIT-like runtime constraint. eSpeak is used only as an installed research diagnostic here. |
| Existing Alice/Björn | Swedish Kokoro, Apache-2.0 voice pack; already integrated. | Quality controls and measured compute/size baseline, not a proposed lightweight replacement. The acoustic weights are shared by Alice and Björn. |

If the target is a voice package around 65–80 MB, Piper NST now has concrete evidence of a useful size/CPU tradeoff. If the requirement is a package of only a few megabytes with negligible CPU, this comparison has not found a qualified replacement for Cesar. Building a Swedish diphone or compact statistical voice on Flite/HTS with suitable Swedish recordings is an option, but must be scoped as a Swedish voice project. It is not a shortcut around the language problem.

## Reproduction and listening assets

Inputs and model URLs/hashes are under `tools/speech/fixtures/compact-swedish*.json` and `piper-nst-provenance.json`. The model was downloaded at revision `5041a7a49ae5b3a0f30e6837769eb03fdbb231bd`, with its LFS SHA-256 verified. No model weights are added to this repository.

Place the pinned model, config, model card and provenance manifest in `build/compact-voice-comparison/models/nst`. With the existing Kokoro pack, C++23 compiler, ffmpeg and a Python environment containing numpy/scipy/soundfile/onnxruntime:

```sh
build/kokoro-export-env/bin/python tools/speech/compact_voice_comparison.py
```

The runner builds its research-only native bridge, runs each profile in isolation, and produces seven cases for each of six configurations. Main audio directories are `build/compact-voice-comparison/cesar-1`, `piper-1` and `bjorn-1`. `piper-clauses-1` retains the additional boundary experiment. An offline `index.html` listening matrix is included in the development output directory.

All 42 listening renders have checked finite output, headroom, hashes and matched EBU R128 integrated loudness within each case. Constant gain only: no EQ, peak limiting, resampling or time stretch. Most cases measure −21 LUFS; prose uses a common lower −21.7 LUFS to accommodate Björn's transient without clipping. Neural output varies; the listening sample is the final measured run, not a rendition chosen by listening. The JSON stores individual timing/duration runs. These artifacts are **unreviewed Swedish listening candidates**.

The committed [measurement record](data/compact-voices-2026-10-21.json) includes all cases, medians, raw runs, memory observations, model provenance and listening-file hashes. No app, default voice, cache version or dependency set changes in this comparison.

## Classical intelligibility target

The listener's target is clearly understandable synthetic Swedish, rather than human-like timbre. Preserve the original clear Cesar source with compact melody as the working baseline. Park the LF source variants and the Piper replacement direction. Keep the pronunciation data authoritative; validate both its intended transcription and the acoustic realization. Native listening decides intelligibility; ASR, pitch correlation, a smooth waveform or extra high-frequency energy cannot substitute for it.

An installed eSpeak NG 1.52.0 Swedish voice now supplies an external, deterministic reference for the unfamiliar prose, quantity pairs and Swedish sound contrasts. Each pair is matched in integrated loudness with constant gain only, retaining its native rate and timing. These recordings use the **same text but different front ends**: eSpeak supplies its Swedish transcription, while Cesar uses our existing IPA. They compare complete outputs and do not isolate the acoustic engine. This distinction matters given the pronunciation differences already documented above.

The listening files are under `build/classical-swedish-reference/{prose,quantity,swedish-sounds}/{espeak,cesar}.wav`. The [reference record](data/classical-swedish-reference-2026-10-21.json) preserves commands, transcriptions, levels, lengths and hashes. eSpeak remains an external research tool; no GPL implementation or phoneme/formant tables are copied into the app.

Before another broad timbre change, inspect recognizable Swedish phones and their joins. Specific current modelling choices worth testing independently include:

- Vowels inherit donor-element gains, bandwidths and transition behaviour while only their formant targets are changed; the resulting Swedish identities and distinctions need listening verification.
- The current /r/ is a short, shallow /l/-derived element. It has not been established that this produces a recognizable Swedish tap in context.
- Stop closure, release and aspiration and consonant-to-vowel transitions need to preserve word identity. Their timing currently combines rsynth element proportions with approximate neural-derived duration averages.
- Quantity, word accent and prominence must remain separate. A nicer whole-sentence melody cannot repair an unrecognizable vowel or consonant, and our current dictionary stress does not fully specify Swedish lexical accent.

These are inspectable hypotheses from the current implementation, not established diagnoses of the listener's difficulty. Use the short quantity/Swedish-sound probes and unfamiliar words to identify actual confusions, then change one acoustic or transition mechanism at a time. Do not infer a new Swedish voice from an English demo or promise eSpeak-level intelligibility before that comparison succeeds.

Further native feedback confirms that Swedish eSpeak is clearer and lacks the clicks heard in Cesar; Alice/Björn remain the stronger sound-quality references. The [pulse/control-edge experiment](cesar-edges-2026-10-21.md) now tests localized changes to the original clear source, with prosody held fixed. It does not claim that those mechanisms have been established as the cause.
