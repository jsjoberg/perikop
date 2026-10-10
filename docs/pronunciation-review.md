# Swedish pronunciation review

The native review tool uses `resources/lexicon/sv1917-words.tsv`.
The prepared list contains 28,487 forms from the Swedish 1917 corpus, including its apocrypha.
NST supplies reference pronunciations for 20,183 forms.
The remaining 8,304 forms include 3,354 possible names.
Capitalization identifies possible names, so this classification can include other words.

Open **Uppläsning → Granska svenskt uttal…**.
You can also start the application with `--pronunciation-review`.
The default queue shows possible names without NST entries, in order of decreasing frequency.
The filters can show all missing forms, all words, or the words with several NST pronunciations.
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
The bundled voice pack is necessary for audio previews.

NST SAMPA is reference data.
The review tool does not convert SAMPA to model input.
Speech spellings guide the voice model through text replacements.
Text in `⟦…⟧` gives the exact phonemes of one word, for example `⟦manˈasə⟧`.
Phonemes use the voice pack's NST symbols, with the stress mark directly before the stressed vowel.

## Words with several pronunciations

Some spellings have more than one pronunciation, such as "kors" (a cross, or the genitive of "ko").
The voice pack's lexicon keeps only one pronunciation for each spelling.
For frequent Bible words it often has the pronunciation of a name or an abbreviation, such as "Han" for "han" and "Du" for "du".

`tools/lexicon/build_homographs.py` finds these words.
It compares the corpus words with every NST pronunciation and with the prepared voice pack.
It writes `resources/lexicon/sv1917-homographs.tsv`, which contains 303 words.
Run it after the voice pack preparation:

```sh
uv run --locked tools/lexicon/build_homographs.py
```

The filter **Flera uttal i NST** shows these words.
For each word there is one button for each NST pronunciation, with its parts of speech.
"böjd form" marks a pronunciation that NST lists only as an inflection of another word.
"röstens val" marks the pronunciation of the voice pack.
A button puts the phonemes in **Uttalsstavning** and plays the verse with them.

`resources/corpus/pronunciation-sv.tsv` corrects 40 of these words for the sense they have in this Bible.

Some words change their sense between verses:

| Word | Senses | Phrases mark |
|------|--------|--------------|
| förlåten | the temple veil, or "forgiven" | the veil, such as "innanför förlåten" and "förlåten i templet" |
| hov | a royal court, or the past tense of "häva" | the court: "ditt hov" and "konungens hov" |
| bete | pasture, or the verb "bete sig" | the verb: "bete sig", "bete mig", "bete dig", and "bete oss" |
| dans | a dance, or "Dans" (of the tribe of Dan) | the tribe, such as "Dans stam" and "Dans barn" |

A phrase row puts the word in brackets, for example `innanför [förlåten]	fˈøːɭˌoːtən`.
The phonemes apply to the bracketed word wherever the complete phrase occurs.
The words of a phrase must be separated only by spaces, so punctuation stops a match.
Capitalization does not matter, so "Dans" and "dans" need different neighboring words.
Elsewhere, the word gets its own row, such as `hov	hˈuːv`, or the voice's pronunciation.
The phrases cover each occurrence of the other sense in the Swedish 1917 corpus.
`tests/core_test.cpp` checks every occurrence of the four words.
Check new phrases against every verse that contains the word. A short phrase can match a verse with the other sense.

A phrase has priority over a saved correction and over a preview of the word.
Thus a correction of "förlåten" does not change "innanför förlåten".
If an example verse contains a phrase, **Vers · förslag** plays the phrase pronunciation in that verse.

## Bundled phonemes

`resources/corpus/pronunciation-sv.tsv` contains first-pass phonemes for 122 frequent names and book titles, the 40 homograph corrections, and the phrases above.
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
