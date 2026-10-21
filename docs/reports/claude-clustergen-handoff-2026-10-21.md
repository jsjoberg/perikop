# Handoff prompt for Claude

Continue work in `/Users/johan/develop/lectionary`. Read this handoff and inspect the current working tree before changing anything. The user wants an actual trial of Swedish Random Forest Clustergen through Flite, after strongly preferring the upstream Swedish synthesized demos. Please progress autonomously through local, reversible work. Do not return to speculative Cesar source/filter tuning.

## Objective and native listening decisions

Build a small, low-CPU Swedish speech option that sounds reasonably clear and respects our curated phonetic data. The user prefers a classical/non-neural solution and earlier requested MIT-like/permissive licensing. They have not relaxed that licensing requirement.

- Alice and Björn (existing Swedish Kokoro voices) remain the sound-quality references, but are too large/compute-heavy for this separate lightweight goal.
- Cesar is our tiny local rsynth/Klatt-derived formant voice. Its Swedish clarity, clicking and rhythm remain disappointing despite many source/prosody experiments. Preserve checkpoints; stop treating tiny numerical changes as audible progress.
- Piper NST was actually measured with our IPA. The user found it roughly as hard to understand as Cesar, despite nicer melody. It is parked, not the preferred direction.
- Swedish eSpeak is clearer and lacks Cesar's clicks, but is GPL.
- MBROLA Swedish Lukas/Ofelia were built and auditioned locally. Latest user feedback: not impressed by their sound or licensing. Do not prioritize them.
- User heard the two CMU Wilderness **synthesized** RFS Swedish samples and said: “wow definitely best so far, probably best after Björn and Alice so far, so we should really try this if we can and I don't misunderstand something important.” This is the active direction. They sometimes say RFC; they mean RFS/Random Forest Clustergen.

## Repository state

- Remote: `git@github.com:jsjoberg/perikop.git`.
- Branch: `formant-voice`.
- Existing draft PR #33: https://github.com/jsjoberg/perikop/pull/33 . Already attached to the Codex chat. User previously authorized pushing milestones to this PR. Do not merge it.
- HEAD: `9a6a033 Reconstruct upstream speech and audit existing Swedish support`, already pushed.
- At handoff, the only untracked code was `tools/speech/clustergen_trial.py`, newly written and successfully exercised with an **English loader control**. This handoff document is also newly untracked. No Clustergen changes are committed/pushed yet.
- No production voice/default/cache/dependency changes in the current investigation. Existing source experiments are opt-in. Production Cesar still uses its original settings.
- No AGENTS.md was found previously in repo/ancestors. Check applicable instructions yourself. Do not spawn subagents unless authorized by the user or applicable instructions.

## The Swedish RFS evidence

Official pages work through plain HTTP even though the web reader repeatedly fails/rewrites them to HTTPS:

- http://www.festvox.org/cmu_wilderness/
- http://www.festvox.org/cmu_wilderness/SWESFB/index.html
- http://www.festvox.org/cmu_wilderness/SWESFV/index.html

Both have a **Text to speech examples** section separate from original **Alignment examples**. The user heard these exact synthesized files, both saying “Detta är berättelsen om Jesus Kristus, Davids son, Abrahams son.”:

- http://www.festvox.org/cmu_wilderness/SWESFB/tts/B01___01_Matthew_____SWESFBN2DA_00002.wav
- http://www.festvox.org/cmu_wilderness/SWESFV/tts/B01___01_Matthew_____SWESFVN2DA_00002.wav

They were played directly from upstream; do not confuse them with aligned recordings. No local Swedish RFS synthesis has happened yet. The native preference is for these demos, not proof of arbitrary-text performance or successful IPA control.

SWESFB lists 10,126 aligned utterances / 15:54:31; SWESFV 11,197 / 16:45:31. RFS MCD values are 5.48 / 4.97 respectively. These are objective diagnostics, not substitutes for native listening or reliable cross-speaker rankings. The user has not explicitly selected one voice over the other.

Official recipe repo: https://github.com/festvox/datasets-CMU_Wilderness , pinned revision `a2102dcd050bf09d4b8aeea5babc6470b5b9376f`.

Downloaded/read local evidence:

- `/tmp/lectionary-wilderness-index.html`
- `/tmp/lectionary-wilderness-SWESFB.html`, `/tmp/lectionary-wilderness-SWESFV.html`
- `/tmp/lectionary-SWESFB-tts-index.html`, `/tmp/lectionary-SWESFV-tts-index.html`
- `/tmp/lectionary-wilderness-do_found` (official `bin/do_found`)
- `/tmp/lectionary-wilderness-languages.txt`
- `/tmp/lectionary-wilderness-issues.txt`
- `/tmp/lectionary-SWESFV-indices.tar.gz`

The Swedish `.tar.gz` linked from the page was inspected. It contains `info`, language ID/name, start URL, `.segments`, `.scores`, `.rfs` indices. **It is not a voice model or audio corpus.** Public `tts/` directory listings contain ten WAV examples each, no runnable model.

The recipe's `make_tts` (~line 918) calls `setup_cg`, selects up to 2,000 good utterances, calls `make_cg_grapheme`, then `build_cg_rfs_voice`. `get_voices` (~1090) copies `v_rfs/flite/*.flitevox` and Festival archives to `voices/`. Demo generation (~1120+) invokes Flite from text files, including first Matthew verses. This supports ordinary text-driven synthesis; these familiar examples are not established held-out evaluation. The README estimates at least 48 hours training on an old 12-core machine; do not assume this Mac's duration. Full realignment is a separate expensive process and should not be done unnecessarily.

## Missing model and distribution issues

No runnable Swedish `.flitevox` or Festival archive has been found yet. Checked the official packed Flite 2.1/2.3 voice directory inventories (English/Indic), official Bard voice selection source, GitHub code/repo searches, demo directories, and a small set of recipe-derived Swedish model URL paths. Searches found language-list references, not model files. Do not turn this bounded search into a global claim that models do not exist. Stop endless filename guessing; request upstream assistance if necessary.

- https://github.com/festvox/datasets-CMU_Wilderness/issues/11 documents the broken old Bible.is scraper.
- https://github.com/festvox/datasets-CMU_Wilderness/pull/13 adds a 2025 note about current Bible Brain API access requiring an API key.
- https://www.faithcomesbyhearing.com/audio-bible-resources/bible-brain is the official current API entry point.
- No API key was requested, obtained or configured by this agent. Do not scrape/extract an unrelated embedded key or bypass access requirements.
- Flite runtime is permissively licensed: https://github.com/festvox/flite/blob/master/COPYING . This does **not** establish Swedish voice/data rights.
- Wilderness README says it cannot redistribute original Bible.is audio. Issue #4 has a participant confirming language-data restrictions, but no clear permission for distributing trained Swedish models was established: https://github.com/festvox/datasets-CMU_Wilderness/issues/4 . Do not conflate recordings, model weights, generated outputs and engine terms. Do not claim that a free app automatically resolves this.
- User has authorized a local trial, not contacting maintainers/rights holders or posting public issues. If external contact becomes necessary, prepare a concrete request for model files + training phone maps + redistribution terms, show it for approval, then ask before sending. No message/request has been drafted or sent yet.

## Critical phonetic-model issue

These are **grapheme-trained** voices. Upstream Unitran maps characters to labels that need not have their IPA/English acoustic meaning after Swedish training. For example `j → dZ`, `ä → OY`; `o`, `u` are labels derived from spelling. A label `dZ` alone does not prove it sounds like an English affricate. The user preferred the actual Swedish sound despite these labels.

Our precise IPA is therefore **not** safely interchangeable with this inventory. One-to-one remapping can lose quantity, rounded-vowel distinctions, retroflexes, stress and word accent, and phonetic labels may not exist in training. We may need new phonetic alignments/retraining with our transcriptions for reliable control. First obtain/reproduce the unmodified Swedish baseline. Do not silently replace our curated pronunciation pipeline with spelling-to-sound and call the requirement met.

Read:

- `/tmp/lectionary-make-cg-grapheme` (official voice-building helper).
- `/tmp/lectionary-unitran-tables.c`.
- `build/clustergen-swedish-research/flite/lang/cmu_grapheme_lex/cmu_grapheme_lex.c`.
- `build/clustergen-swedish-research/flite/lang/cmu_grapheme_lang/cmu_grapheme_lang.c`.

Flite exposes `flite_synth_phones` / CLI `-p`, but that takes the **model's native phone inventory**, not arbitrary IPA. Its existence alone does not solve this mapping.

## Completed native runtime preparation

Pinned Flite revision `6c9f20dc915b17f5619340069889db0aa007fcdc`, source and binaries under:

`/Users/johan/develop/lectionary/build/clustergen-swedish-research/flite`

Built successfully on Apple Silicon with `CFLAGS='-O2 -fPIC'`, shared libraries, sockets and audio-device output disabled. A local ignored `config/research.lv` selects:

```
LEXES = cmulex cmu_grapheme_lex
LANGS = usenglish cmu_grapheme_lang
VOXES = cmu_us_kal
```

Configuration: `./configure --disable-shared --disable-sockets --with-audio=none --with-langvox=research CFLAGS='-O2 -fPIC'`, then `make -j4`. Logs are `build/clustergen-swedish-research/{configure,build}.log`. Do not run `make install`.

An official English Clustergen model was downloaded **solely to exercise the native CG loader**, not as Swedish evidence:

- `build/clustergen-swedish-research/english-loader-control.flitevox`
- Source http://www.festvox.org/flite/packed/flite-2.3/voices/cmu_us_slt.flitevox
- SHA256 `c54b0d757b943bfbffc626e95a251f1659cc2d840465daa8b858457139b94bc0`
- 11,156,032 bytes, language `eng`, name `cmu_us_slt`.

The new `tools/speech/clustergen_trial.py` successfully loaded it and rendered an English smoke sentence through the actual CG path. Report: `build/clustergen-swedish-research/english-loader-control/validation.json`. 4.565 seconds output at 16 kHz, zero PCM rails, median process elapsed 0.083 s / CPU 0.080 s. Timing includes startup, loading, text frontend and WAV writing; **do not quote it as Swedish performance or compare directly with earlier inference-only numbers**.

The script accepts a local model and provenance URL, records hashes/metadata, warms up and measures 3 separate process runs, checks PCM output and writes segment durations. Swedish `--voice-id SWESFB|SWESFV` requires the model's language to be `cmu_grapheme_lang`, then runs six existing Swedish text cases. It explicitly records `project_ipa_used: false`, excludes forced-IPA overrides until an adapter exists, and adds no runtime dependency to the app. No Swedish run or normalization comparison has been performed.

Example once a real model is obtained:

```
.venv/bin/python tools/speech/clustergen_trial.py --voice-id SWESFV --model /absolute/path/to/voice.flitevox --model-source URL
```

Only English-loader-control execution has tested this new script so far. Do not pretend Swedish paths are validated. No extra tests, reports, commits or PR updates for this investigation have yet been completed.

## Existing project resources to preserve/reuse

- `tools/speech/fixtures/compact-swedish.json`: frozen native frontend text, IPA and Kokoro tokens for seven cases (Psalm, unfamiliar prose, quantity, Swedish sounds, word accent, names, forced-phone overrides).
- `tests/fixtures/cesar-prosody/`: exact original, accepted reference-melody and clear compact checkpoints; do not overwrite.
- Native probe `build/cmake/bin/ortho-cesar-audio-probe`; `tools/speech/cesar_benchmark.py` verifies preserved fixtures.
- Reports: `docs/reports/{compact-swedish-comparison-2026-10-21,cesar-upstream-2026-10-21,cesar-alternatives-2026-10-21,cesar-edges-2026-10-21}.md`.
- Original/default production code previously passed app build, 14 non-GUI CTests and relevant formatting/tidy. No production changes since. No need unrelated GUI tests for research-only work.
- `.venv/bin/python` works for stdlib scripts. Rich audio/ORT Python: `build/kokoro-export-env/bin/python`. ffmpeg, sox, espeak-ng, C++ compiler installed via Homebrew/Xcode.

## Continue from here

1. Obtain a runnable Swedish model or establish the precise practical way to reconstruct one. Avoid repeating already-failed searches indefinitely.
2. Establish source/model redistribution terms separately from Flite's permissive runtime. Prepare an approval-ready upstream inquiry if needed; do not send without explicit authorization.
3. Reproduce Swedish synthesis locally, then compare new text and phone/quantity/name probes with the accepted external sound and Alice/Björn/Cesar at matched loudness.
4. Measure actual Swedish package size and CPU/memory, then determine whether a safe pronunciation adapter is feasible or phonetic retraining is necessary.
5. Preserve the strong native preference as a qualified milestone, report what is verified and missing, and save useful work to the existing PR. Do not represent an English loader test, remote demo playback, a guessed phone map or unverified license as a completed Swedish integration.

If original models/data cannot be obtained on suitable terms, training Clustergen on appropriately licensed Swedish recordings (potentially NST) is an alternative requiring its own dataset verification and native qualification. It will not automatically reproduce the voice the user liked. Do not silently switch to that project or a neural system.
