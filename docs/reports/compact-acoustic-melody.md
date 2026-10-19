# Acoustic melody trial and primary-voice size estimate

New v6 samples use reusable compact-bank waveform material. **They are not neural-master audio.** The original FP32 master supplies offline pitch controls, mapped by the frozen acoustic alignment. All v5 word and pause timing is unchanged. The source bank itself still comes from the earlier compressed teacher; rebuilding and improving its units remains separate work.

Three full development samples and clearly labeled master/v5/v6 sequences are exported locally by this experiment. Audio is not bundled with the application.

## What changed numerically

All entries compare the same three passages against the full FP32 master. Lower errors are closer; these numbers do not rank pleasantness.

| Signal | V5 | V6 |
|---|---:|---:|
| Mel spectral convergence | 0.4645 | 0.4611 |
| Log-mel error, dB | 6.3547 | 6.3368 |
| Pitch RMSE, semitones | 3.3718 | 3.0815 |
| Voicing disagreement | 0.0976 | 0.1000 |
| Independent ASR, total errors / words | 18 / 181 | 18 / 181 |

Corinthians pitch RMSE improves from 2.46 to 1.68 semitones. Mean spectral error barely changes. ASR errors redistribute: Corinthians 1→2, Kings 7→5, Samuel 10→11. The micro-averaged WER stays 9.94%; mean passage WER worsens slightly. One local spectral window regresses beyond the provisional repeat allowance (Corinthians, master 7–8 seconds). The numerical decision is **hold**: no demonstrated overall improvement, only three paired passages and a local regression. These examples are for inspection, not a new accepted voice.

## Voice asset size

All units below are decimal MB/GB. This excludes the app and corpus database. Corpus: 35,515 verses / 839,666 whitespace words, approximately 81 hours per voice from the 40-master duration/word ratio. No full-corpus package or Alice bank has been built.

| Assets | One voice | Both voices, assuming Alice has equal density |
|---|---:|---:|
| Reusable waveform bank, measured for Björn | 90.7 MB installed / 52.9 MB compressed | 181.4 MB / 105.7 MB |
| Current literal BUS1 + BGP1 formats, whole-corpus projection including bank | 661 MB installed / 340 MB download | 1.32 GB installed / 679 MB download |

The BGP1 debug format stores 16 bytes for every overlap-add grain, roughly 120 grains/second. Whole-corpus grain instructions alone project to 551 MB per voice. A ±30% planning allowance gives roughly **0.9–1.7 GB installed and 0.48–0.88 GB download for both voices**. This is an extrapolation from three grain-plan passages and 40 span-plan passages, not a measured release size. Short per-verse streams may compress worse.

A practical redesign would store pitch/voicing curves and word timing, then generate grains during playback. Budgeting one pitch byte per 20 ms, two timing bytes per word and 2 MB of source epochs gives approximately **129 MB per voice / 258 MB for both** with the present bank. **About 200–350 MB installed for both** is an engineering planning range, not an implemented or quality-validated result. Download size is not yet established. More units could raise the bank size. The current 166 MB neural acoustic model already shares its weights between Alice and Björn, so two voices do not mean two neural models.

## Can this become the primary strategy?

It remains a plausible fixed-corpus strategy, but the present measurements do not establish that it can replace either voice. Runtime assembly is inexpensive; the unresolved requirement is pleasant, clear speech. The compact distance to the master remains much larger than the existing neural export, and some earlier metric rankings disagree with the listener.

The next useful synthesis experiment is better and longer reusable source units with acoustically checked boundaries, generated from the original full-precision voices. Use the master to plan target and join costs offline, measure reconstruction errors and inspect the worst windows. Melody alone cannot restore missing or incorrectly joined speech sounds. Alice needs its own bank and quality evaluation. Confirm the chosen design on unused passages and the X220i/Pi 5 before switching production defaults. No production voice replacement is made here.

## CI and reproduction

The Windows compact-voice failure was caused by text-mode fixture streams translating LF to CRLF. The fixture now writes the exact binary pack bytes. The CodeQL multiplication alert is fixed by converting samples to double before multiplication. The metrics workflow now compiles both native renderers before tests, rather than silently skipping native conformance checks. Local verification: compact CTest passes, all 34 Python/native tests pass with zero skips, C++ formatting and targeted clang-tidy pass. GitHub results are checked after pushing.

For the experiment run `tools/speech/evaluation/acoustic_prosody_experiment.py --help`; inputs are the frozen v2 bank outputs, v4 epoch analysis, v5 timing controls, full-reference comparison and original FP32 masters. `reference_compare.py` and `reference_gate.py` consume its output exactly as earlier candidates. `size_estimate.py --help` reproduces the measured/projection size split. Master/teacher data retains the upstream Apache-2.0 notices; these evaluation scripts and native renderers are original MIT code. No ASR or master model is shipped.

Detailed results and size assumptions: [machine-readable summary](speech-evaluation/acoustic-melody.json).
