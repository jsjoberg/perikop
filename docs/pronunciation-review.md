# Swedish pronunciation review

The native review tool uses `resources/lexicon/sv1917-words.tsv`.
The prepared list contains 28,487 forms from the Swedish 1917 corpus, including its apocrypha.
NST supplies reference pronunciations for 20,183 forms.
The remaining 8,304 forms include 3,354 possible names.
Capitalization identifies possible names, so this classification can include other words.

Open **Uppläsning → Granska svenskt uttal…**.
You can also start the application with `--pronunciation-review`.
The default queue shows possible names without NST entries, in order of decreasing frequency.
The filters can show all missing forms or all words.
Each word has up to three examples from the bundled Swedish corpus.
The examples use complete words and ignore capitalization.

1. Select a word.
2. Use **Ord · nu** to hear its current pronunciation.
3. Use **Vers · nu** to hear the pronunciation in context.
4. If the pronunciation needs a correction, enter a speech spelling or phonemes in **Uttalsstavning**.
5. Use **Ord · förslag** or **Vers · förslag** to hear the proposed spelling.
6. Use **Godkänn nuvarande**, **Spara korrigering**, or **Granska senare** to record your decision.

The preview uses the Swedish voice selected in the reader, at its normal speed.
The model can take time to load and generate new audio.
The status shows model preparation, audio preparation, playback, completion, or an error.
The installed voice pack is necessary for audio previews.

NST SAMPA is reference data.
The review tool does not convert SAMPA to model input.
Speech spellings guide the voice model through text replacements.
Text in `⟦…⟧` gives the exact phonemes of one word, for example `⟦manˈasə⟧`.
Phonemes use the voice pack's NST symbols, with the stress mark directly before the stressed vowel.

`resources/corpus/pronunciation-sv.tsv` contains first-pass phonemes for 122 frequent names and book titles.
They mainly correct the stress that the neural fallback guesses for unknown names, such as `Johannesevangeliet`.
They are drafts and nobody has reviewed them by ear yet.
The field shows a draft as the current `⟦…⟧` spelling. **Godkänn nuvarande** keeps it, and a saved correction replaces it.
A name's genitive -s uses the pronunciation of the name.
Listen to both the word and its verse before you approve a correction.

The application stores decisions in `pronunciation-review.db`, beside `user.db` in the application data directory.
Capitalized forms share one decision with their lowercase forms.
Approvals preserve the current reading.
Saved corrections apply to subsequent Swedish playback.
Deferred decisions retain the proposed spelling and note without changing playback.
Clear **Endast ej granskade** to revisit a saved decision.
The application preserves the Scripture text.

**Exportera TSV…** exports every saved decision, including its form, status, speech spelling, and note.
The exporter replaces tabs and line breaks in notes with spaces.
The export supports later curation of the bundled pronunciation lexicon.
Personal corrections do not automatically change the bundled data.

NST source details and the pinned download hash are in `tools/lexicon/inputs.json`.
The prepared word list uses the NST Swedish pronunciation lexicon under CC0.
Strong's numbers and Dalin definitions belong to [the word-study panel](word-study.md).
This pronunciation tool does not include those datasets.
