> Updated user priorities: pleasant voice quality is non-negotiable. The 30 MB figure was an estimate, not a limit. Performance and startup delay of the existing neural voice on machines such as the ThinkPad X220i and Raspberry Pi 5 are the current problem. Replacement engines/models require license review. The Bas investigations below are historical diagnostic evidence, not a commitment to that architecture.

# Is Björn Bas a dead end? Diagnostic control, 2026-10-19

Observed result: the unchanged native Bas renderer recognizes much better with units extracted from the exact sentence being rendered. This is an oracle reconstruction control, not a general-purpose or reusable voice improvement. No production synthesizer or resource was changed.

## Controlled comparison

Three short development sentences use identical teacher phoneme instructions for both native renders. This bypasses text-processing differences. Source predicted durations and the existing preparation/extraction rules build a temporary sentence-specific unit bank; the renderer executable is identical to the failed baseline. Shipped units are the other condition. ASR receives audio alone, with no expected-text hint. The third condition applies only the existing mu-law companding to neural source PCM.

| Sentence | Shipped units WER / CER | Same-sentence units WER / CER | Companding only WER / CER |
|---|---:|---:|---:|
| I begynnelsen skapade Gud himmel och jord. | 100.0% / 100.0% | 0.0% / 0.0% | 0.0% / 0.0% |
| Han låter mig vila på gröna ängar. | 142.9% / 111.1% | 0.0% / 0.0% | 0.0% / 0.0% |
| Han vederkvicker min själ. | 200.0% / 86.4% | 50.0% / 4.5% | 50.0% / 4.5% |

The short “själ” sample is recognized as “Han vederkvick är min själ” after either companding alone or same-sentence assembly, versus “Han vederkvicker min själ” in the source. One letter changes and one word splits: WER is 50%, CER only 4.55%. This demonstrates why short-sentence WER alone is a poor improvement criterion.

## What the result supports

- The unchanged renderer can preserve recognizable word sequences with compatible source units. Its joins and companding do not inevitably destroy all speech.
- The shipped bank fails even when supplied with the teacher’s exact phoneme sequence. A text frontend error alone cannot explain these three failures.
- Extracted-unit content/labeling, unit context and source timing are now stronger investigation targets than further general crossfade tuning. These factors are not yet causally separated.

## What it does not establish

- Same-sentence reconstruction uses the answer acoustically. It does not show that units generalize to other sentences.
- Contiguous reassembly can work even with wrongly labeled boundaries. The success does not validate phoneme boundaries; those still need an independent audit.
- Replaced units also change duration, coarticulation, pitch and spectral context. This experiment does not isolate which property matters.
- Three short passages provide diagnostic evidence only, not a general quality estimate or a listener assessment.
- Fit within approximately 30 MB for two useful voices remains unproven.

## Recommended next controlled experiment

Create a development-only bank from connected Bible speech with traceable source spans and multiple candidates per unit. Independently inspect a small subset of boundaries. Test sentences excluded from that bank’s source recordings, with the same fixed phoneme instructions and unchanged renderer, against the shipped bank. Hold the production pack unchanged. If same-sentence controls remain good but these separate-source targets remain unusable, revisit the representation or use longer context-bearing units. Do not keep tuning joins without evidence.

## Audio labeling

`diagnostic-bas-same-sentence-*.wav` was rendered by the native Bas engine from units cut from that exact sentence. It is not neural inference at playback, but its units originate in neural speech at preparation time. It is not the corrected reusable voice. `diagnostic-bas-shipped-units-*.wav` is the same renderer/phone plan with the existing reusable bank. No need to listen to more failed examples.

The machine report retains source spans, instructions, raw ASR, renderer statistics, signal diagnostics and hashes.
