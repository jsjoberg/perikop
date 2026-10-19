# Word-duration correction after rejected v3 rhythm

The listener judged both v3 variants worse than compact v2, with uneven rhythm
that felt Norwegian rather than Swedish. The A/B difference was almost
undetectable. Both variants fail the mandatory pleasantness requirement.

V3's timing map permitted extreme local stretching/compression, including
punctuation pauses. Its source-pitch mode also generated a new pulse clock from
smoothed F0 rather than preserving actual source epoch intervals.

V4 makes one uniform duration change per approximate word, bounded at ±10%
with nearest-sample rounding. Words under 120 ms and gaps between words/clauses
retain their durations. Its scheduler uses actual source epoch periods. There
is no teacher pitch transfer. Word boundaries are still approximate; this does
not repair all inherited pronunciation, timbre or unit-join defects.

Every rendered waveform sample comes from the reusable compact v2 bank through
its selected-unit waveform. The existing neural reference supplies only offline
predicted timing. The native grain renderer performs no inference at playback.

## Verification and limits

- Twenty regression checks pass. These include the actual duration scheduler
  under an identity map with irregular pitch epochs, bounded word duration and
  unchanged gap lengths.
- On all three development passages, identity reconstruction has at most
  1.12e-16 float error; new native PCM matches the Python reference bit for bit.
- New renders have zero uncovered samples. This does not establish naturalness.
- Independent recognition across these three passages gives 24 word errors in
  181 words (13.26% WER). Historical v2 has 25 errors (13.81%). Two passages
  worsen and Corinthians improves. The new run uses CPU; the historical run may
  use Metal. Treat this as a rough intelligibility guardrail, not a controlled
  improvement claim.
- No held-out passages were used. The listener reports improvement over v3, but still hears staccato phrasing and wants more melodic Swedish. Voice quality is not accepted.

The 12-second listening sample is the beginning of 1 Corinthians 13:4–7 from
the native compact render, without normalization or substituted neural audio.

Reproduction commands and format details are in
[the experiment notes](compact-prosody.md#bounded-word-duration-correction-after-v3-rejection).
[Full measurements and provenance](speech-evaluation/compact-word-rhythm.json).
