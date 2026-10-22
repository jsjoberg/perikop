"""Exercise source matching without downloading any lexical inputs."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/lexicon'))
from modern import Entry, associations, morphology, resolve, saldo_entries


class ModernLexiconTest(unittest.TestCase):
    def xml(self, text):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        path = Path(directory.name) / 'lexicon.xml'
        path.write_text('<LexicalResource><Lexicon>' + text + '</Lexicon></LexicalResource>', encoding='utf-8')
        return path

    def test_homographs_unicode_and_whole_words(self):
        entries = {
            'hjärta..1': Entry('hjärta..1', 'hjärta', 'nn', frozenset({'hjärta..nn.1'})),
            'hjärta..2': Entry('hjärta..2', 'hjärta', 'nn', frozenset({'hjärta..nn.1'})),
        }
        matched = resolve(entries, ['hjärta', 'hjärtats', 'xhjärta', 'hjärt'],
                          {'hjärtats': {'hjärta..nn.1'}}, {})
        self.assertEqual(matched['hjärta'], {'hjärta..1': 'exact', 'hjärta..2': 'exact'})
        self.assertEqual(matched['hjärtats'], {'hjärta..1': 'inflected', 'hjärta..2': 'inflected'})
        self.assertFalse(matched['xhjärta'])
        self.assertFalse(matched['hjärt'])
        decomposed = Entry('id', 'HJÄRTA'.replace('Ä', 'A\u0308'), 'nn', frozenset())
        self.assertEqual(resolve({'id': decomposed}, ['hjärta'], {}, {})['hjärta'], {'id': 'exact'})

    def test_morphology_excludes_compound_fragments_and_phrase_components(self):
        path = self.xml('''
          <LexicalEntry><Lemma><FormRepresentation>
            <feat att="writtenForm" val="lampa"/><feat att="lemgram" val="lampa..nn.1"/>
            <feat att="partOfSpeech" val="nn"/>
          </FormRepresentation></Lemma>
            <WordForm><feat att="writtenForm" val="lampor"/><feat att="msd" val="pl indef nom"/></WordForm>
            <WordForm><feat att="writtenForm" val="lamp"/><feat att="msd" val="ci"/></WordForm>
            <WordForm><feat att="writtenForm" val="lamp"/><feat att="msd" val="cm"/></WordForm>
            <WordForm><feat att="writtenForm" val="lamp"/><feat att="msd" val="c"/></WordForm>
            <WordForm><feat att="writtenForm" val="lamp"/><feat att="msd" val="sms"/></WordForm>
          </LexicalEntry>
          <LexicalEntry><Lemma><FormRepresentation>
            <feat att="writtenForm" val="av allt hjärta"/><feat att="lemgram" val="av_allt_hjärta..abm.1"/>
            <feat att="partOfSpeech" val="abm"/>
          </FormRepresentation></Lemma>
            <WordForm><feat att="writtenForm" val="hjärta"/><feat att="msd" val="invar"/></WordForm>
          </LexicalEntry>''')
        forms = morphology(path, {'lampa', 'lampor', 'lamp', 'hjärta'})
        self.assertEqual(forms, {'lampa': {'lampa..nn.1'}, 'lampor': {'lampa..nn.1'}})

    def test_saldo_selects_actual_word_and_retains_sense_relations(self):
        path = self.xml('''
          <LexicalEntry><Lemma/><Sense id="PRIM..1"/></LexicalEntry>
          <LexicalEntry><Lemma>
            <FormRepresentation><feat att="writtenForm" val="multi"/>
              <feat att="partOfSpeech" val="sxc"/><feat att="lemgram" val="multi..sxc.1"/></FormRepresentation>
            <FormRepresentation><feat att="writtenForm" val="mången"/>
              <feat att="partOfSpeech" val="pn"/><feat att="lemgram" val="mången..pn.1"/></FormRepresentation>
          </Lemma><Sense id="mången..1">
            <SenseRelation targets="mycken..1"><feat att="label" val="primary"/></SenseRelation>
            <SenseRelation targets="PRIM..1"><feat att="label" val="secondary"/></SenseRelation>
          </Sense></LexicalEntry>''')
        entries = saldo_entries(path)
        self.assertEqual(set(entries), {'mången..1'})
        self.assertEqual(entries['mången..1'].headword, 'mången')
        self.assertEqual(entries['mången..1'].lemgrams, {'mången..pn.1'})
        entries['mycken..1'] = Entry('mycken..1', 'mycken', 'pn', frozenset())
        self.assertEqual(associations(entries['mången..1'], entries), 'mycken')

    def test_curated_verbs_do_not_include_adjective_namesakes(self):
        entries = {'verb': Entry('verb', 'få', 'vb', frozenset()),
                   'adjective': Entry('adjective', 'få', 'av', frozenset())}
        self.assertEqual(resolve(entries, ['fingo'], {}, {'fingo': ('få', True)}),
                         {'fingo': {'verb': 'curated'}})


if __name__ == '__main__':
    unittest.main()
