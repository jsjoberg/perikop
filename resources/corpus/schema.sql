PRAGMA foreign_keys=ON;
CREATE TABLE source (
 id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE, language TEXT NOT NULL,
 name TEXT NOT NULL, versification TEXT NOT NULL, license_id TEXT, attribution TEXT
) STRICT;
-- Book data from books.tsv. Stanzas start a paragraph at every verse, as in the Psalms.
CREATE TABLE book (
 id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE, canonical_order INTEGER NOT NULL,
 name_sv TEXT, name_el TEXT, name_en TEXT, abbreviation_sv TEXT NOT NULL,
 new_testament INTEGER NOT NULL CHECK(new_testament IN (0,1)),
 deuterocanonical INTEGER NOT NULL CHECK(deuterocanonical IN (0,1)),
 stanzas INTEGER NOT NULL CHECK(stanzas IN (0,1))
) STRICT;
-- The reader's book order from canon.tsv. A book can be a chapter range of its
-- framing Septuagint book, as Nehemiah is of 2 Esdras; a NULL last chapter means the end.
CREATE TABLE canon (
 position INTEGER PRIMARY KEY, book TEXT NOT NULL UNIQUE REFERENCES book(code),
 frame_book TEXT NOT NULL REFERENCES book(code),
 first_chapter INTEGER NOT NULL CHECK(first_chapter>0),
 last_chapter INTEGER CHECK(last_chapter>=first_chapter)
) STRICT;
CREATE TABLE verse (
 id INTEGER PRIMARY KEY, source_id INTEGER NOT NULL REFERENCES source(id),
 book_id INTEGER NOT NULL REFERENCES book(id), chapter INTEGER NOT NULL CHECK(chapter>0),
 verse INTEGER NOT NULL CHECK(verse>0),
 last_verse INTEGER NOT NULL CHECK(last_verse>=verse), verse_suffix TEXT NOT NULL DEFAULT '',
 text TEXT NOT NULL CHECK(length(text)>0), UNIQUE(source_id,book_id,chapter,verse,verse_suffix)
) STRICT;
-- The UNIQUE constraint already provides the verse lookup index.
-- Display boundaries from USFM. JSON editions use explicitly labelled WEB
-- editorial boundaries where chapter/verse coordinates agree.
CREATE TABLE paragraph (
 source_id INTEGER NOT NULL REFERENCES source(id), book_id INTEGER NOT NULL REFERENCES book(id),
 chapter INTEGER NOT NULL, verse INTEGER NOT NULL, verse_suffix TEXT NOT NULL DEFAULT '',
 provenance TEXT NOT NULL,
 PRIMARY KEY(source_id,book_id,chapter,verse,verse_suffix)
) STRICT;
-- Explicit source ranges preserve chapter, verse and letter coordinates.
-- A target book may differ, as for Nehemiah in Greek 2 Esdras. A verse without
-- counterpart has no target.
CREATE TABLE alignment (
 from_source INTEGER NOT NULL REFERENCES source(id),
 to_source INTEGER NOT NULL REFERENCES source(id), book_id INTEGER NOT NULL REFERENCES book(id),
 from_first_chapter INTEGER NOT NULL, from_first_verse INTEGER NOT NULL, from_first_suffix TEXT NOT NULL,
 from_last_chapter INTEGER NOT NULL, from_last_verse INTEGER NOT NULL, from_last_suffix TEXT NOT NULL,
 to_book_id INTEGER REFERENCES book(id),
 to_first_chapter INTEGER, to_first_verse INTEGER, to_first_suffix TEXT,
 to_last_chapter INTEGER, to_last_verse INTEGER, to_last_suffix TEXT,
 kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 6), CHECK((to_book_id IS NULL)=(kind IN (5,6))),
 PRIMARY KEY(from_source,to_source,book_id,from_first_chapter,from_first_verse,from_first_suffix)
) STRICT;
CREATE TABLE pronunciation (
 language TEXT NOT NULL, source TEXT NOT NULL, spoken TEXT, phonemes TEXT,
 priority INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(language,source)
) STRICT;
-- Segment coordinates follow the reference edition's numbering.
CREATE TABLE reading_rule(id INTEGER PRIMARY KEY,pdist INTEGER,month INTEGER,day INTEGER,service TEXT,description TEXT,ordering INTEGER,tradition TEXT,label TEXT,reference INTEGER NOT NULL REFERENCES source(id)) STRICT;
CREATE TABLE reading_segment(
 rule_id INTEGER NOT NULL REFERENCES reading_rule(id),ordering INTEGER NOT NULL CHECK(ordering>=0),
 book TEXT NOT NULL REFERENCES book(code),
 first_chapter INTEGER NOT NULL CHECK(first_chapter>0),first_verse INTEGER NOT NULL CHECK(first_verse>0),
 last_chapter INTEGER NOT NULL,last_verse INTEGER NOT NULL CHECK(last_verse>0),
 CHECK(last_chapter>first_chapter OR (last_chapter=first_chapter AND last_verse>=first_verse)),
 PRIMARY KEY(rule_id,ordering)
) STRICT;
CREATE TABLE feast_rule(id INTEGER PRIMARY KEY,pdist INTEGER,month INTEGER,day INTEGER,rank INTEGER,title TEXT,feast TEXT,tradition TEXT) STRICT;
CREATE TABLE ordo_rule(year INTEGER,month INTEGER,day INTEGER,service TEXT,pdist INTEGER,PRIMARY KEY(year,month,day,service)) STRICT;
