# Swedish pronunciation audit

The prepared voice lexicon gives one pronunciation per spelling.
It cannot select a word meaning from its verse context.
This audit compares its 303 ambiguous forms with the bundled Swedish 1917 text, including the apocrypha.

The audit adds 35 word corrections and two phrase corrections to `resources/corpus/pronunciation-sv.tsv`.
The audit checked all occurrences of these words in context.
NST supplies the replacement phonemes; these changes do not alter the displayed Scripture text.
The new word rows use `word-only` to prevent automatic genitives.
For example, a correction for `ko` must not affect `Kos` or the phrase `sin kos`.
Audio listening remains necessary to assess the voices' rendering of these phonemes.

## Word corrections

| Word | Occurrences | Meaning in this corpus | Voice selected | Example verse |
|------|------------:|------------------------|----------------|---------------|
| hos | 760 | preposition, at or with | noun | 1Chr 4:23 |
| vikt | 61 | weight | folded or yielded | 1Chr 21:25 |
| vänt | 44 | turned | beautiful | 1Kgs 13:22 |
| rest | 17 | raised or traveled | remainder | 1Sam 15:12 |
| tomt | 14 | empty | plot of land or name | 2Cor 9:3 |
| predikat | 13 | preached | grammatical predicate | 1Peter 1:12 |
| förleder | 5 | leads astray | prefixes, plural of förled | Acts 18:13 |
| förena | 4 | unite | bows of ships, plural of för | 2Cor 6:16 |
| kista | 4 | chest or coffin | place name Kista | 2Chr 24:8 |
| ko | 4 | cow | name or abbreviation | Heb 9:13 |
| svans | 4 | tail | genitive of swan | Deut 28:13 |
| duken | 3 | cloth; also the old verb form duken | genitive of duke | Acts 10:16 |
| kosta | 3 | cost | name | Josh 6:26 |
| logen | 3 | threshing floor | theater box | 2Kgs 6:27 |
| rene | 3 | pure | name René | 2Sam 22:27 |
| saks | 3 | genitive of sak, matter | name | 1Cor 4:17 |
| synts | 3 | been visible | genitive of synthesizer | 1Chr 17:17 |
| ton | 3 | tone | tonne | Exod 19:13 |
| eggen | 2 | edge of a blade | name Eggen | 1Sam 13:21 |
| klor | 2 | claws | chlorine | Dan 7:19 |
| menat | 2 | meant | adjective menat | 2Cor 12:19 |
| demon | 1 | demon | the demo | Tob 6:8 |
| korsar | 1 | crosses | corsair | Jer 51:31 |
| makes | 1 | genitive of make, husband | English make | Sir 4:10 |
| mats | 1 | genitive of mat, food | name Mats | Rom 14:20 |
| murat | 1 | built with masonry | name Murat | Lam 3:9 |
| ron | 1 | the peace or rest | name Ron | Sir 30:17 |
| tima | 1 | happen | time an event | Jer 31:40 |
| timat | 1 | happened | timed an event | Luke 1:1 |
| trumpet | 1 | trumpet | sulky | Hos 5:8 |
| van | 1 | practiced or accustomed | name Van | Wis 13:11 |
| åse | 1 | watch | name Åse | Ezra 4:14 |
| öst | 1 | drawn water | east | John 2:9 |
| skylt | 3 | covered | sign | 2Sam 19:4 |
| fördraget | 4 | the treaty | adjective fördraget | 2Macc 13:25 |

## Context corrections

| Word | Occurrences | Correction | Other occurrences |
|------|------------:|------------|-------------------|
| ana | 9 | `att [ana]` selects the verb in 2 Maccabees 8:11. | Eight occurrences name Ana; they retain the voice pronunciation. |
| förband | 5 | `den [förband]` selects the bandage in Ezekiel 30:21. | Four occurrences use the verb “joined” or “bound”; they retain the voice pronunciation. |

The existing phrase rules for `förlåten`, `hov`, `bete`, and `dans` remain in place.
`tests/core_test.cpp` checks every occurrence of all six context words and all 35 additional word corrections.
The tests check the selected pronunciation and the occurrence count.

## Joses and its possessive

The voice treats an unknown capitalized word that ends in s as a possible genitive.
For `Joses`, it finds the Spanish name `Jose` and adds s to that pronunciation.
This takes priority over the prepared pronunciation of the complete name.

The bundled correction gives `Joses` the Swedish pronunciation `jˈuːsəs`.
It uses `word-only`, because the possessive is `Joses'`, with no additional s.
The correction preserves both straight and curved apostrophes in the displayed text.
The corpus test checks Mark 6:3, Mark 15:40, Mark 15:47, and Matthew 27:56.

## Krubba and krubban

The NST entries and the prepared voice lexicon give both forms the long vowel `ʉː`.
The bundled corrections use the short vowel `ɵ`: `krˈɵba` and `krˈɵban`.
Both rows use `word-only`.

The corpus test checks all seven occurrences.
`Krubba` occurs in Isaiah 1:3, Job 39:12, Luke 2:7, and Luke 2:12.
`Krubban` occurs in Luke 2:16, Luke 13:15, and Proverbs 14:4.

## Limits of the audit

An NST difference does not always identify a wrong meaning.
The voice column in `sv1917-homographs.tsv` excludes bundled corrections.
For example, the existing `drog` correction already gives `drogs` the correct pronunciation.
Pronouns such as `min`, `din`, and `vad` can change vowel length with emphasis.
The old verb form `helgen` needs a hard g; the weekend noun has a different pronunciation.
The voice already gives that verb form a hard g, so this audit leaves it unchanged.

Other differences need a separate pronunciation review, including `huru`, `inför`, and forms of `förbinda`.
Proper names such as `Lo-Debar`, `So`, `Makron`, and `Matris` also need review beyond NST's ordinary-word alternatives.
The audit does not claim that all 303 forms or all corpus names now have correct audio.
A saved personal word correction still takes priority over a bundled word correction.
A bundled phrase correction takes priority over a personal word correction.
