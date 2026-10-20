# Swedish Random Forest Clustergen trial

2026-10-21. Goal: try the CMU Wilderness Swedish RFS (Random Forest Clustergen) voices through Flite. The listener judged their upstream synthesized demos the best Swedish sound after Alice and Björn. No product voice, default, dependency or cache has been changed.

## Result

**No Swedish synthesis has run locally.** The runtime is ready; a Swedish model is not.

- The listener's preference rests on two upstream TTS demos ([SWESFB](http://www.festvox.org/cmu_wilderness/SWESFB/tts/B01___01_Matthew_____SWESFBN2DA_00002.wav), [SWESFV](http://www.festvox.org/cmu_wilderness/SWESFV/tts/B01___01_Matthew_____SWESFVN2DA_00002.wav)). Both are familiar Matthew text from the training domain. They do not prove arbitrary-text quality or control through our IPA.
- No runnable Swedish `.flitevox` or Festival voice was found. Checked: Flite 2.1/2.3 packed voice inventories, Bard voice selection source, the Wilderness `tts/` directories (WAV only), GitHub, Hugging Face model/dataset search and general web search. This is a bounded search, not proof that no copy exists.
- Rebuilding the voice is technically well defined but blocked by data access and terms (below).
- The voices the listener liked cannot be assumed shippable. Flite's runtime is permissive. The training audio and text are not, and no permission for trained models was found.

## Runtime readiness (verified)

Flite at `6c9f20dc915b17f5619340069889db0aa007fcdc` is built under `build/clustergen-swedish-research/flite` with the grapheme language and lexicon. [`tools/speech/clustergen_trial.py`](../../tools/speech/clustergen_trial.py) loaded the official English `cmu_us_slt` Clustergen model (SHA-256 `c54b0d75…94bc0`) and rendered one sentence without PCM clipping. That checks the loader only. Its 0.08 s per process covers startup and an English frontend; it is not a Swedish performance figure.

Swedish runs (`--voice-id SWESFB|SWESFV`) require a model whose language is `cmu_grapheme_lang`. They synthesize six cases from `fixtures/compact-swedish.json` through the voice's own text frontend and record `project_ipa_used: false`. The forced-phone case is excluded until a phone adapter exists. These Swedish paths are untested.

## How the voice would be rebuilt

From the [official recipe](https://github.com/festvox/datasets-CMU_Wilderness) at `a2102dcd`, `bin/do_found`:

1. `fast_make_align SWESFV.tar.gz` downloads chapter audio and text, cuts utterances with `sox trim` using the published `.segments` times, and selects the RFS subset from `.rfs`. **This skips the expensive realignment.**
2. `make_tts` builds the RFS voice from those utterances: `setup_cg`, `make_cg_grapheme`, `build_cg_rfs_voice`.
3. `get_voices` copies `v_rfs/flite/*.flitevox`.

The SWESFV index archive (inspected locally, 2018 timestamps) has 9,519 segments from fileset `SWESFVN2DA`, and a 2,000-utterance RFS list. Its `info` gives 2.86 h of RFS training audio, base MCD 5.15 and RF MCD 4.97. Training needs only that 2.86 h subset, not the 16.8 h aligned set. The README's 48-hour estimate covers the full pipeline on an old 12-core machine. Training time on this Mac is not measured.

Obstacles, in order:

- **Download.** The old Bible.is scraper is broken ([issue #11](https://github.com/festvox/datasets-CMU_Wilderness/issues/11)). The current [Bible Brain](https://www.faithcomesbyhearing.com/audio-bible-resources/bible-brain) API needs a key ([PR #13](https://github.com/festvox/datasets-CMU_Wilderness/pull/13)). No key was requested or used.
- **Segment drift.** The segment times match the 2018 recordings and text. If today's fileset was re-encoded or re-edited, cuts and text will not line up. This cannot be checked without the audio.
- **Toolchain.** Not installed: Festival, Festvox and SPTK. Homebrew bottles Edinburgh Speech Tools 2.5.0; the others need source builds.

## Terms

These are separate questions. The answers so far are:

| Item | Status |
| --- | --- |
| Flite runtime | Permissive ([COPYING](https://github.com/festvox/flite/blob/master/COPYING)). |
| Festvox tools | X11-style; CMU claims no rights in voices built with them. That does not cover the training data. |
| Bible audio and text | The Wilderness README says it cannot redistribute the audio. The [Bible Brain license](https://www.faithcomesbyhearing.com/bible-brain/license) grants API use for an application's development. It forbids offline download unless a fileset is exposed through its `/download` endpoint. It reserves all rights not expressly granted and says nothing about model training. Licensors of individual filesets may add stricter terms. |
| Trained Swedish models | No grant found. [Issue #4](https://github.com/festvox/datasets-CMU_Wilderness/issues/4) has a participant confirming per-language data restrictions, not permission. |
| Generated audio | Not established. |

A free app does not resolve this. Shipping either voice would need written permission from FCBH and the translation's rights holder, and possibly CMU's view on the models. That is unlikely to meet the permissive-licensing requirement.

## Phonetic control

These are grapheme voices. Unitran maps each letter to a label, for example `j → dZ` and `ä → OY`. The upstream demo page shows `och` as `o ch h` and `bröder` as `b r 2: d e r`. After Swedish training these labels mean whatever the letter sounded like. They are not IPA. Flite's `-p` input takes this native label set, not arbitrary phones.

Consequences:

- A table from our IPA to these labels cannot express quantity, stress, word accent or retroflex assimilation, because the model never saw them as distinct units. Names and loanwords would be read by Swedish spelling.
- Real control through our transcriptions needs retraining with phone-level labels derived from our IPA.

## Options

1. **Ask CMU for the existing built models**, for a local listening trial only. This is the only quick way to hear the liked voice on new text. It does not settle shipping terms. A draft request is below. **It has not been sent.**
2. **Rebuild SWESFV locally** with a Bible Brain key that the user applies for personally, if the fileset permits download. This is for research only and cannot be shipped. It needs the full toolchain and an unmeasured training run.
3. **Train the same RFS recipe on NST Swedish speech-synthesis recordings**, one male speaker with about 5,300 utterances. Secondary sources list CC0; confirm this on the Språkbanken record before relying on it. Our IPA is already NST-based, so training could use our own phone labels instead of graphemes. That solves both the licensing and the phonetic-control problem. It is a new voice project and will not reproduce the Wilderness voice's sound. Piper NST, also trained on NST data, was hard to understand. Part of that problem may come from the recordings, not the model.

Option 3 is the only one that could become a shippable, permissive voice with our pronunciation. Options 1 and 2 only tell us whether RFS itself sounds good on unfamiliar Swedish text.

## Draft upstream request (not sent; needs approval)

> Subject: CMU Wilderness Swedish RFS voices (SWESFB / SWESFV): model files for evaluation
>
> Hello,
>
> I am developing Perikop, a free offline Swedish lectionary app. Your Wilderness TTS examples for SWESFB and SWESFV are the clearest compact Swedish synthesis we have heard. We would like to evaluate them on new text with Flite.
>
> Could you share, for local evaluation only:
> 1. the `v_rfs` Flite voices (`.flitevox`) and the Festival voice directories for SWESFB and SWESFV;
> 2. the festvox, Flite and Unitran revisions used to build them, and the phone set or grapheme map;
> 3. the RFS `txt.done.data` prompts, if you are permitted to share them.
>
> We also want to understand the terms. Do you know of any permission, from Bible.is/Faith Comes By Hearing or the translation holders, for distributing the trained models or their output? We will not redistribute anything without clear written terms.
>
> Thank you for Flite and the Wilderness work.
>
> Johan Sjöberg

## Not done

- No Swedish synthesis, listening comparison, package-size or CPU measurement.
- No contact with CMU, FCBH or rights holders, and no API key request.
- No Festival, Festvox or SPTK build, and no training run.
