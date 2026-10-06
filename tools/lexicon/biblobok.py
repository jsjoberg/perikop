"""Extract Nyström's 1896 dictionary prose from pinned Runeberg letter pages."""
import html
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import unicodedata
from urllib.parse import quote, unquote, urldefrag, urljoin

from fetch import fetch


class WithoutTables(HTMLParser):
    """Remove whole tables before identifying headings inside the remaining prose."""
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.parts = []
        self.hidden = 0

    def handle_starttag(self, tag, attrs):
        if tag in ('table', 'script', 'style'):
            self.hidden += 1
        elif not self.hidden:
            self.parts.append(self.get_starttag_text())

    def handle_endtag(self, tag):
        if tag in ('table', 'script', 'style'):
            self.hidden -= 1
        elif not self.hidden:
            self.parts.append(f'</{tag}>')

    def handle_data(self, data):
        if not self.hidden:
            self.parts.append(html.escape(data, quote=False))


class Prose(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.parts = []
        self.hidden = 0

    def handle_starttag(self, tag, attrs):
        if tag in ('table', 'script', 'style'):
            self.hidden += 1
        if not self.hidden and tag in ('br', 'p', 'li', 'h2', 'h3'):
            self.parts.append('\n')

    def handle_endtag(self, tag):
        if tag in ('table', 'script', 'style'):
            self.hidden -= 1
        if not self.hidden and tag in ('p', 'li', 'h2', 'h3'):
            self.parts.append('\n')

    def handle_data(self, data):
        if not self.hidden:
            self.parts.append(data)


def plain(fragment):
    parser = Prose()
    parser.feed(fragment)
    lines = [re.sub(r'\s+', ' ', line).strip() for line in ''.join(parser.parts).splitlines()]
    return '\n'.join(line for line in lines if line)


def articles():
    """Return (source id, headword, base form, definition, URL, cross-references)."""
    inputs = json.loads(Path(__file__).with_name('inputs.json').read_text(encoding='utf-8'))
    result = []
    for source, metadata in sorted(inputs.items()):
        if not source.startswith('biblobok_'):
            continue
        text = fetch(source).read_text(encoding='utf-8')
        text = text.partition('The above contents can be inspected in scanned images:')[0]
        prose = WithoutTables()
        prose.feed(text)
        text = ''.join(prose.parts)
        # Page-number anchors interrupt prose, not articles. Keep them in the body.
        anchors = [match for match in re.finditer(r'<a\s+name="([^"]+)"[^>]*>\s*</a>', text, re.I)
                   if not re.fullmatch(r'sid\d+', match[1])]
        boundaries = [(match.start(), match.end(), html.unescape(match[1]), True) for match in anchors]
        heading_positions = set()
        for index, match in enumerate(anchors):
            segment = text[match.end():anchors[index + 1].start() if index + 1 < len(anchors) else len(text)]
            heading = re.match(r'\s*(?:<br\s*/?>|<p>)*\s*<b>(.*?)</b>', segment, re.I | re.S)
            if heading:
                heading_positions.add(match.end() + heading.start(1))
        # Some printed entries (e.g. Rebecka) have no HTML anchor.
        # A bold headword at the start of a paragraph still marks their boundary.
        for match in re.finditer(r'<(?:br\s*/?|p)>\s*<b>([^<]+)</b>', text, re.I):
            headword = html.unescape(match[1]).strip(' .,')
            if match.start() < anchors[0].start():
                continue
            if match.start(1) in heading_positions or len(headword) < 2 or len(headword) > 80:
                continue
            if not headword[0].isalpha() or re.fullmatch(r'[IVXLCDM]+', headword):
                continue
            boundaries.append((match.start(), match.start(), headword, False))
        boundaries.sort()
        seen = {}
        for index, (_, start, anchor, anchored) in enumerate(boundaries):
            segment = text[start:boundaries[index + 1][0] if index + 1 < len(boundaries) else len(text)]
            heading = re.match(r'\s*(?:<br\s*/?>|<p>)*\s*<b>(.*?)</b>', segment, re.I | re.S)
            if not heading:
                continue
            base = re.sub(r'[.,\s]*\d+[.]?$', '', anchor).strip(' .,')
            number = re.search(r'(\d+)[.]?$', anchor)
            headword = base + (f' ({number[1]})' if number else '')
            definition = plain(segment[heading.end():]).lstrip(' .,\n')
            if not definition or not base:
                continue
            base = unicodedata.normalize('NFC', base)
            seen[anchor] = seen.get(anchor, 0) + 1
            article_id = source + '#' + ('unanchored:' if not anchored else '') + anchor + (f'~{seen[anchor]}' if seen[anchor] > 1 else '')
            url = metadata['url'] + ('#' + quote(anchor, safe='') if anchored else '')
            references = []
            if len(definition) < 150 and re.match(r'(?:se\b|dens\.\s*s\.)', definition, re.I):
                for href in re.findall(r'<a\s+href="([^"]+)"', segment, re.I):
                    page, fragment = urldefrag(urljoin(metadata['url'], html.unescape(href)))
                    if fragment and page.startswith('https://runeberg.org/biblobok/ordbok_'):
                        references.append(page + '#' + quote(unquote(fragment), safe=''))
            result.append((article_id, headword, base.lower(), definition, url, references))
    return result


def resolve_forms(entries, words, spellings):
    """Match whole headwords and genitives; never guess a person from a prefix."""
    direct = {}
    by_url = {}
    references = {}
    for article_id, _, base, _, url, links in entries:
        for form in [base, *[part.strip() for part in base.split(',')]]:
            direct.setdefault(form, set()).add(article_id)
        by_url.setdefault(url, []).append(article_id)
        references[article_id] = links
    aliases = {}
    for line in Path(__file__).parents[2].joinpath('resources/lexicon/sv1917-biblical-forms.tsv').read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'):
            continue
        form, headwords = line.split('\t')
        targets = []
        for headword in headwords.split('|'):
            if headword not in direct:
                raise ValueError(f'Unknown biblical headword: {headword}')
            targets.extend(sorted(direct[headword]))
        aliases[form] = list(dict.fromkeys(targets))
    resolved = {}
    # Keep curated aliases even if this edition does not happen to use the form.
    for word in sorted(set(words) | aliases.keys()):
        found = aliases.get(word, sorted(direct.get(word, set())))
        if not found:
            for candidate in spellings(word):
                if candidate in direct:
                    found = sorted(direct[candidate])
                    break
        if not found and word.endswith('s'):
            for candidate in spellings(word[:-1]):
                if candidate in aliases or candidate in direct:
                    found = aliases.get(candidate, sorted(direct.get(candidate, set())))
                    break
        # A bare "see" reference also opens its target, with that target's own URL.
        expanded = []
        visited = set()
        def append(article_id, depth=0):
            if article_id in visited:
                return
            visited.add(article_id)
            expanded.append(article_id)
            if depth < 8:
                for url in references[article_id]:
                    for target in by_url.get(url, []):
                        append(target, depth + 1)
        for article_id in found:
            append(article_id)
        resolved[word] = expanded
    return resolved
