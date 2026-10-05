PRAGMA foreign_keys=ON;
CREATE TABLE source (
 id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE, language TEXT NOT NULL,
 name TEXT NOT NULL, versification TEXT NOT NULL, license_id TEXT, attribution TEXT
);
CREATE TABLE book (
 id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE, canonical_order INTEGER NOT NULL,
 name_sv TEXT, name_el TEXT, name_en TEXT
);
CREATE TABLE verse (
 id INTEGER PRIMARY KEY, source_id INTEGER NOT NULL REFERENCES source(id),
 book_id INTEGER NOT NULL REFERENCES book(id), chapter INTEGER NOT NULL CHECK(chapter>0),
 verse INTEGER NOT NULL CHECK(verse>0), text TEXT NOT NULL,
 UNIQUE(source_id,book_id,chapter,verse)
);
CREATE INDEX verse_lookup ON verse(source_id,book_id,chapter,verse);
-- Explicit source ranges preserve chapter and verse coordinates.
CREATE TABLE alignment (
 from_source INTEGER NOT NULL REFERENCES source(id),
 to_source INTEGER NOT NULL REFERENCES source(id), book_id INTEGER NOT NULL REFERENCES book(id),
 from_first_chapter INTEGER NOT NULL, from_first_verse INTEGER NOT NULL,
 from_last_chapter INTEGER NOT NULL, from_last_verse INTEGER NOT NULL,
 to_first_chapter INTEGER NOT NULL, to_first_verse INTEGER NOT NULL,
 to_last_chapter INTEGER NOT NULL, to_last_verse INTEGER NOT NULL,
 kind INTEGER NOT NULL,
 PRIMARY KEY(from_source,to_source,book_id,from_first_chapter,from_first_verse)
);
CREATE TABLE pronunciation (
 language TEXT NOT NULL, source TEXT NOT NULL, spoken TEXT, phonemes TEXT,
 priority INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(language,source)
);
