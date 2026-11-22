#!/usr/bin/env python3
"""Keeps the design docs: Confluence is the master, the repo holds a Markdown copy.

    python scripts/confluence_sync.py migrate [--dry-run]
        one-time baseline: push the DOCS below to Confluence. Refuses to touch
        anything if a page was edited by hand since its last sync.
    python scripts/confluence_sync.py draft FILE --path docs/x.md --parent README.md [--dry-run]
        publish a new page from a local draft (keep drafts in build/docs-drafts/).
        Refuses if the page already exists.
    python scripts/confluence_sync.py cleanup [--dry-run]
        remove what pages must not hold (the old sync banner, links to and URLs of
        the code repository) in place, keeping every other edit made on the page.
    python scripts/confluence_sync.py pull [PATH ...] [--dry-run]
        write the reviewed pages back to the repo as Markdown. Idempotent: a file
        whose content already matches its page is left untouched. Without PATH,
        pages still in "draft" state are skipped: name them once the code is done.

Workflow: write in Confluence -> review and edit there -> implement -> pull.
There is deliberately no command that overwrites an existing page from the repo.

Pages are for readers: no notes about tooling, and no references into the code
hosting (links to repo files become code-formatted paths, the clone URL becomes
a placeholder). Publishing refuses any page that mentions github.com.

docs/confluence_pages.json maps repo paths to page ids, so renaming a page in
Confluence does not break anything. It holds no credentials.

Credentials come from ~/.confluence/confluence.env (override the path
with RFS_CONFLUENCE_ENV), never from the repository:

    CONFLUENCE_BASE_URL=https://<site>.atlassian.net/wiki
    CONFLUENCE_EMAIL=<email of the Atlassian account that owns the token>
    CONFLUENCE_API_TOKEN=<API token>

Environment variables with the same names take precedence.

Requires the `markdown` package (installed by docker/setup_ubuntu.sh;
elsewhere: pip install markdown).
"""

import argparse
import base64
import difflib
import hashlib
import html
import html.entities
import json
import os
import pathlib
import re
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET

import markdown

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO_URL = 'https://github.com/ravii/ruhnix-flight-system'  # never written to a page
REPO_URL_PLACEHOLDER = '<仓库地址>'
FORBIDDEN_ON_PAGES = 'github.com'
MANIFEST = REPO_ROOT / 'docs' / 'confluence_pages.json'

# Parent page for everything below: https://<site>/wiki/x/<ROOT_TINY_LINK>
# (the space home page "My first space" of space MFS).
ROOT_TINY_LINK = 'dIAE'

# Baseline for `migrate`: (Markdown file, parent file or None for the root page). Parents first.
# Pages added later are recorded in MANIFEST only.
# README.md becomes the project page; all design docs hang below it.
DOCS = [
    ('README.md', None),
    ('docs/architecture.md', 'README.md'),
    ('common/README.md', 'README.md'),
    ('flight-controller/README.md', 'README.md'),
    ('flight-controller/bsp/README.md', 'flight-controller/README.md'),
    ('simulator/README.md', 'README.md'),
    ('perception/README.md', 'README.md'),
    ('localization/README.md', 'README.md'),
]

# Baseline pages renamed in Confluence, so `migrate` cannot find them by their Markdown title.
BASELINE_PAGE_IDS = {'README.md': '1048595'}  # the project page, titled "RFS Design"

# Markdown fence language -> Confluence code macro language.
CODE_LANGUAGES = {'bash': 'bash', 'sh': 'bash', 'powershell': 'powershell', 'cpp': 'cpp',
                  'c': 'cpp', 'python': 'python', 'yaml': 'yaml'}

SYNC_PREFIX = 'sync:'
CODE_MACRO = 'ac:name="code"'


# ---- conversion --------------------------------------------------------------

def tiny_link_to_page_id(tiny):
    raw = base64.urlsafe_b64decode(tiny + '=' * (-len(tiny) % 4))
    return str(int.from_bytes(raw, 'little'))


def split_title(md_text, path):
    first, _, rest = md_text.partition('\n')
    if not first.startswith('# '):
        sys.exit(f'{path}: first line must be the "# Title" heading')
    return first[2:].strip(), rest


def cdata(text):
    return '<![CDATA[' + text.replace(']]>', ']]]]><![CDATA[>') + ']]>'


def scrub_code(code):
    return re.sub(re.escape(REPO_URL) + r'(?:\.git)?', REPO_URL_PLACEHOLDER, code)


def code_macro(match):
    lang = CODE_LANGUAGES.get(match.group(1) or '', 'none')
    code = scrub_code(html.unescape(match.group(2)).rstrip('\n'))
    return ('<ac:structured-macro ac:name="code">'
            f'<ac:parameter ac:name="language">{lang}</ac:parameter>'
            f'<ac:plain-text-body>{cdata(code)}</ac:plain-text-body>'
            '</ac:structured-macro>')


def as_code(body):
    """Link text without the link; plain text becomes a code-formatted path or name."""
    return body if '<' in body else f'<code>{body}</code>'


def rewrite_link(match, doc_path, titles):
    href, body = html.unescape(match.group(1)), match.group(2)
    if FORBIDDEN_ON_PAGES in href:
        return as_code(body)
    if re.match(r'^[a-z]+:', href) or href.startswith('#'):
        return match.group(0)
    target, _, _anchor = href.partition('#')
    resolved = os.path.normpath(os.path.join(os.path.dirname(doc_path), target)).replace('\\', '/')
    if resolved in titles:
        title = html.escape(titles[resolved], quote=True)
        return (f'<ac:link><ri:page ri:content-title="{title}" />'
                f'<ac:link-body>{body}</ac:link-body></ac:link>')
    return as_code(body)  # a repo file: name it, do not link into the code hosting


def to_storage(md_text, doc_path, titles):
    title, body_md = split_title(md_text, doc_path)
    body = markdown.markdown(body_md, extensions=['tables', 'fenced_code'], output_format='xhtml')
    body = re.sub(r'<pre><code(?: class="language-([\w+-]+)")?>(.*?)</code></pre>', code_macro,
                  body, flags=re.S)
    body = re.sub(r'<a href="([^"]+)">(.*?)</a>', lambda m: rewrite_link(m, doc_path, titles),
                  body, flags=re.S)
    storage = body
    validate_xhtml(storage, doc_path)
    return title, storage


def parse_storage(storage):
    """Parses storage XHTML. Named HTML entities (&nbsp; ...) are not valid XML."""
    def numeric(match):
        name = match.group(1)
        if name in ('amp', 'lt', 'gt', 'quot', 'apos'):
            return match.group(0)
        code = html.entities.name2codepoint.get(name)
        return f'&#{code};' if code is not None else match.group(0)

    parts = re.split(r'(<!\[CDATA\[.*?\]\]>)', storage, flags=re.S)
    parts[::2] = [re.sub(r'&([A-Za-z][A-Za-z0-9]*);', numeric, part) for part in parts[::2]]
    return ET.fromstring('<root xmlns:ac="urn:ac" xmlns:ri="urn:ri" xmlns:at="urn:at">' +
                         ''.join(parts) + '</root>')


def validate_xhtml(storage, doc_path):
    try:
        parse_storage(storage)
    except ET.ParseError as e:
        sys.exit(f'{doc_path}: converted page is not well-formed XHTML: {e}')
    found = [m.group(0) for m in re.finditer(r'.{0,40}' + re.escape(FORBIDDEN_ON_PAGES) + r'.{0,40}',
                                             storage)]
    if found:
        sys.exit(f'{doc_path}: a page must not reference {FORBIDDEN_ON_PAGES}: ' + ' | '.join(found))


# ---- storage XHTML -> Markdown --------------------------------------------------

AC = '{urn:ac}'
RI = '{urn:ri}'
BLOCK_TAGS = {'p', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'ul', 'ol', 'table', 'pre', 'hr',
              'blockquote', AC + 'structured-macro'}
CODE_LANGUAGE_ALIASES = {'c++': 'cpp', 'shell': 'bash'}


class ConversionError(Exception):
    """The page holds content that Markdown cannot carry; nothing may be dropped silently."""

    def __init__(self, problems):
        super().__init__('; '.join(problems))
        self.problems = problems


def escape_text(text, in_table=False):
    text = text.replace('\\', '\\\\')
    text = re.sub(r'([`*])', r'\\\1', text)
    text = re.sub(r'(?<!\w)_|_(?!\w)', r'\\_', text)
    text = re.sub(r'\](?=[(\[])', r'\\]', text)
    text = re.sub(r'&(?=#?\w+;)', '&amp;', text)
    text = re.sub(r'<(?=[A-Za-z/!?])', '&lt;', text)
    return text.replace('|', '\\|') if in_table else text


def escape_line_start(line):
    """Keeps a paragraph line from being read as a heading, list, quote, fence ..."""
    if re.match(r'=+\s*$', line):
        return '&#61;' + line[1:]
    if line.startswith('~~~'):
        return '&#126;' + line[1:]
    if re.match(r'#|>|[-+](\s|$)|-+\s*$|\[[^\]]*\]:', line):
        return '\\' + line
    return re.sub(r'^(\d+)\.(?=\s)', r'\1\\.', line)


class Links:
    """Maps Confluence pages back to repo paths."""

    def __init__(self, titles, ids=None):
        self.by_title = {title: path for path, title in titles.items()}
        self.by_id = dict(ids or {})


class MarkdownWriter:
    def __init__(self, doc_path, links, known_languages=None):
        self.doc_path = doc_path
        self.links = links
        self.known_languages = known_languages or {}
        self.problems = []
        self.in_table = False

    def problem(self, what, el=None):
        text = ' '.join(''.join(el.itertext()).split())[:40] if el is not None else ''
        self.problems.append(f'{what}' + (f' near "{text}"' if text else ''))

    # ---- blocks ----------------------------------------------------------------

    def blocks(self, parent, skip_banner=False):
        out = []
        if (parent.text or '').strip():
            self.problem('text outside a block', parent)
        for index, el in enumerate(parent):
            if (el.tail or '').strip():
                self.problem('text outside a block', el)
            if skip_banner and index == 0 and self.is_banner(el):
                continue
            md = self.block(el)
            if md:
                out.append(md)
        return out

    @staticmethod
    def is_banner(el):
        """The note older versions of this script put at the top of every page."""
        return (el.tag == AC + 'structured-macro' and el.get(AC + 'name') == 'info' and
                'confluence_sync.py' in ''.join(el.itertext()))

    def block(self, el):
        tag = el.tag
        if tag in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6'):
            text = ' '.join(self.inline(el).split())
            return f'{"#" * int(tag[1])} {text}' if text else None
        if tag == 'p':
            text = self.inline(el).strip()
            return '\n'.join(escape_line_start(line) for line in text.split('\n')) if text else None
        if tag in ('ul', 'ol'):
            return self.list_block(el)
        if tag == 'table':
            return self.table(el)
        if tag == 'pre':
            return self.fence(''.join(el.itertext()).rstrip('\n'), '')
        if tag == 'hr':
            return '---'
        if tag == 'blockquote':
            body = '\n\n'.join(self.blocks(el))
            return '\n'.join(f'> {line}'.rstrip() for line in body.split('\n'))
        if tag == AC + 'structured-macro':
            return self.macro(el)
        self.problem(f'unsupported block <{tag.split("}")[-1]}>', el)
        return None

    def macro(self, el):
        name = el.get(AC + 'name')
        if name != 'code':
            self.problem(f'unsupported macro "{name}"', el)
            return None
        params = {p.get(AC + 'name'): (p.text or '') for p in el.findall(AC + 'parameter')}
        if params.get('title'):
            self.problem('code block with a title', el)
        body = el.find(AC + 'plain-text-body')
        code = (body.text or '') if body is not None else ''
        language = (params.get('language') or '').lower()
        language = CODE_LANGUAGE_ALIASES.get(language, language)
        known = set(CODE_LANGUAGES.values()) - {'none'}
        return self.fence(code.rstrip('\n'), language if language in known else '')

    def fence(self, code, language):
        language = language or self.known_languages.get(code, '')
        longest = max((len(run) for run in re.findall(r'`+', code)), default=0)
        ticks = '`' * max(3, longest + 1)
        return f'{ticks}{language}\n{code}\n{ticks}' if code else f'{ticks}{language}\n{ticks}'

    def flow(self, el):
        """Content of a list item: runs of inline content and nested blocks."""
        out = []
        run = ET.Element('p')
        run.text = el.text

        def flush():
            nonlocal run
            if (run.text or '').strip() or len(run):
                md = self.block(run)
                if md:
                    out.append((md, 'p'))
            run = ET.Element('p')

        for child in el:
            if child.tag in BLOCK_TAGS:
                flush()
                if child.tag in ('pre', 'table', AC + 'structured-macro'):
                    self.problem('code block, table or macro inside a list', child)
                else:
                    md = self.block(child)
                    if md:
                        out.append((md, child.tag))
                run.text = child.tail
            else:
                run.append(child)
        flush()
        return out

    def list_block(self, el):
        ordered = el.tag == 'ol'
        number = int(el.get('start', '1'))
        items = []
        for li in el:
            if li.tag != 'li':
                self.problem(f'unexpected <{li.tag}> in a list', li)
                continue
            body = ''
            for index, (md, tag) in enumerate(self.flow(li)):
                joiner = '' if index == 0 else ('\n' if tag in ('ul', 'ol') else '\n\n')
                body += joiner + md
            marker = f'{number}. ' if ordered else '- '
            number += 1
            lines = body.split('\n')
            rest = [f'    {line}' if line else '' for line in lines[1:]]
            items.append('\n'.join([marker + lines[0]] + rest))
        return '\n'.join(items) if items else None

    def table(self, el):
        rows = []
        for tr in el.iter('tr'):
            row = []
            for cell in tr:
                if cell.tag not in ('th', 'td'):
                    self.problem(f'unexpected <{cell.tag}> in a table row', cell)
                    continue
                if cell.get('colspan', '1') != '1' or cell.get('rowspan', '1') != '1':
                    self.problem('merged table cells', cell)
                row.append(self.cell(cell))
            rows.append(row)
        if not rows or any(len(row) != len(rows[0]) for row in rows):
            self.problem('table with uneven rows', el)
            return None
        lines = ['| ' + ' | '.join(row) + ' |' for row in rows]
        lines.insert(1, '|' + '|'.join('---' for _ in rows[0]) + '|')
        return '\n'.join(lines)

    def cell(self, cell):
        children = [c for c in cell if c.tag in BLOCK_TAGS]
        paragraphs = [c for c in children if c.tag == 'p' and
                      ((c.text or '').strip() or len(c))]
        if len(children) != sum(c.tag == 'p' for c in children) or len(paragraphs) > 1:
            self.problem('more than one paragraph, or a block, in a table cell', cell)
            return ''
        self.in_table = True
        source = paragraphs[0] if paragraphs else cell
        text = ' '.join(self.inline(source).split('\n')).strip() if not (
            children and not paragraphs) else ''
        self.in_table = False
        return text

    # ---- inline ----------------------------------------------------------------

    def text(self, value):
        return escape_text(value, self.in_table) if value else ''

    def inline(self, el):
        out = [self.text(el.text)]
        for child in el:
            out.append(self.inline_element(child))
            out.append(self.text(child.tail))
        return ''.join(out)

    def wrap(self, el, mark):
        inner = self.inline(el)
        stripped = inner.strip()
        if not stripped:
            return inner
        lead = inner[:len(inner) - len(inner.lstrip())]
        trail = inner[len(inner.rstrip()):]
        return f'{lead}{mark}{stripped}{mark}{trail}'

    def code_span(self, el):
        code = ''.join(el.itertext())
        if not code:
            return ''
        if self.in_table and '|' in code:  # a raw pipe would split the table cell
            return '<code>' + escape_text(code).replace('|', '&#124;') + '</code>'
        longest = max((len(run) for run in re.findall(r'`+', code)), default=0)
        ticks = '`' * (longest + 1)
        pad = ' ' if code.startswith('`') or code.endswith('`') else ''
        return f'{ticks}{pad}{code}{pad}{ticks}'

    def inline_element(self, el):
        tag = el.tag
        if tag in ('strong', 'b'):
            return self.wrap(el, '**')
        if tag in ('em', 'i'):
            return self.wrap(el, '*')
        if tag == 'code':
            return self.code_span(el)
        if tag == 'br':
            return '<br />' if self.in_table else '  \n'
        if tag == 'a':
            return self.anchor(el)
        if tag == AC + 'link':
            return self.page_link(el)
        if tag in ('span', AC + 'inline-comment-marker'):  # comments are not page content
            return self.inline(el)
        self.problem(f'unsupported inline <{tag.split("}")[-1]}>', el)
        return ''

    def anchor(self, el):
        href = el.get('href')
        if not href:
            return self.inline(el)
        target = self.map_href(href)
        text = self.inline(el) or escape_text(href, self.in_table)
        if ''.join(el.itertext()) == href and target != href:
            text = escape_text(target, self.in_table)
        return f'[{text}]({target})'

    def relative(self, repo_path):
        base = os.path.dirname(self.doc_path) or '.'
        return os.path.relpath(repo_path, base).replace(' ', '%20')

    def map_href(self, href):
        page = re.search(r'/pages/(\d+)(?:/|$|[?#])', href)
        if page and page.group(1) in self.links.by_id:
            return self.relative(self.links.by_id[page.group(1)])
        return href

    def page_link(self, el):
        page = el.find(RI + 'page')
        if page is None:
            self.problem('link to something other than a page (user mention, attachment ...)', el)
            return ''
        title = page.get(RI + 'content-title')
        path = self.links.by_title.get(title)
        if path is None:
            self.problem(f'link to page "{title}", which is not in {MANIFEST.name}', el)
            return ''
        body = el.find(AC + 'link-body')
        plain = el.find(AC + 'plain-text-link-body')
        if body is not None:
            text = self.inline(body)
        elif plain is not None:
            text = escape_text(plain.text or '', self.in_table)
        else:
            text = escape_text(title, self.in_table)
        anchor = el.get(AC + 'anchor')
        return f'[{text}]({self.relative(path)}{"#" + anchor if anchor else ""})'


def fence_languages(md_text):
    """Fence languages Confluence cannot store (cmake ...), to keep them when a page is pulled."""
    found = {}
    for match in re.finditer(r'^(`{3,})([\w+-]+)\n(.*?)\n\1[ ]*$', md_text, flags=re.S | re.M):
        found[match.group(3)] = match.group(2)
    return found


def storage_to_markdown(title, storage, doc_path, links, known_languages=None):
    writer = MarkdownWriter(doc_path, links, known_languages)
    blocks = writer.blocks(parse_storage(storage), skip_banner=True)
    if writer.problems:
        raise ConversionError(writer.problems)
    return '\n\n'.join([f'# {title}'] + blocks) + '\n'


def canonical(md_text, doc_path, titles, links):
    """The Markdown a page would have if it held md_text: comparable with a pulled page."""
    title, storage = to_storage(md_text, doc_path, titles)
    return storage_to_markdown(title, storage, doc_path, links)


def content_hash(title, parent, storage):
    return hashlib.sha256(f'{title}\0{parent}\0{storage}'.encode('utf-8')).hexdigest()[:16]


# ---- Confluence REST API -------------------------------------------------------

def load_credentials():
    env_file = pathlib.Path(os.environ.get(
        'RFS_CONFLUENCE_ENV', pathlib.Path.home() / '.confluence' / 'confluence.env'))
    values = {}
    if env_file.is_file():
        for line in env_file.read_text(encoding='utf-8').splitlines():
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                key, _, value = line.partition('=')
                values[key.strip()] = value.strip()
    keys = ('CONFLUENCE_BASE_URL', 'CONFLUENCE_EMAIL', 'CONFLUENCE_API_TOKEN')
    creds = {k: os.environ.get(k) or values.get(k) for k in keys}
    missing = [k for k in keys if not creds[k]]
    if missing:
        sys.exit(f'missing {", ".join(missing)} (looked in the environment and {env_file})')
    return creds


class NetworkError(Exception):
    """The request may or may not have reached the server."""


class Confluence:
    def __init__(self, creds):
        self.base = creds['CONFLUENCE_BASE_URL'].rstrip('/')
        token = f'{creds["CONFLUENCE_EMAIL"]}:{creds["CONFLUENCE_API_TOKEN"]}'
        self.auth = 'Basic ' + base64.b64encode(token.encode()).decode()

    def request(self, method, path, payload=None, query=None, retries=3):
        """Network errors are retried; callers of non-idempotent requests pass retries=0."""
        url = self.base + path
        if query:
            url += '?' + urllib.parse.urlencode(query)
        data = json.dumps(payload).encode('utf-8') if payload is not None else None
        req = urllib.request.Request(url, data=data, method=method, headers={
            'Authorization': self.auth, 'Accept': 'application/json',
            'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                return json.loads(resp.read() or b'{}')
        except (urllib.error.URLError, TimeoutError, ConnectionError) as e:
            if isinstance(e, urllib.error.HTTPError):
                return self._http_error(method, path, e)
            if retries <= 0:
                raise NetworkError(f'{method} {path}: {e}') from e
            print(f'  network error ({e}), retrying ...')
            time.sleep(3)
            return self.request(method, path, payload, query, retries - 1)

    @staticmethod
    def _http_error(method, path, e):
        detail = e.read().decode('utf-8', 'replace')[:500]
        hint = ''
        if e.code in (401, 403):
            hint = ('\nhint: authentication failed. CONFLUENCE_EMAIL must be the email of '
                    'the Atlassian account that created the API token, and the token must '
                    'not be expired or revoked.')
        sys.exit(f'{method} {path} -> HTTP {e.code}: {detail}{hint}')

    def page(self, page_id):
        return self.request('GET', f'/api/v2/pages/{page_id}', query={'body-format': 'storage'})

    def page_at(self, page_id, number):
        return self.request('GET', f'/api/v2/pages/{page_id}',
                            query={'body-format': 'storage', 'version': number})

    def versions(self, page_id):
        """Newest first."""
        return self.request('GET', f'/api/v2/pages/{page_id}/versions',
                            query={'limit': 250}).get('results', [])

    def find(self, space_id, title):
        found = self.request('GET', '/api/v2/pages',
                             query={'space-id': space_id, 'title': title, 'status': 'current'})
        results = found.get('results', [])
        return self.page(results[0]['id']) if results else None

    def create(self, space_id, parent_id, title, storage):
        """Not blindly retried: a lost response may still have created the page."""
        for attempt in range(3):
            try:
                return self.request('POST', '/api/v2/pages', {
                    'spaceId': space_id, 'status': 'current', 'title': title,
                    'parentId': parent_id,
                    'body': {'representation': 'storage', 'value': storage}}, retries=0)
            except NetworkError as e:
                print(f'  {e}; checking whether the page was created ...')
                time.sleep(3)
                existing = self.find(space_id, title)
                if existing is not None:
                    return existing
        sys.exit(f'could not create "{title}": network keeps failing')

    def update(self, page, title, storage, message):
        return self.request('PUT', f'/api/v2/pages/{page["id"]}', {
            'id': page['id'], 'status': 'current', 'title': title,
            'body': {'representation': 'storage', 'value': storage},
            'version': {'number': page['version']['number'] + 1, 'message': message}})


# ---- commands ------------------------------------------------------------------

def page_storage(page):
    return ((page.get('body') or {}).get('storage') or {}).get('value') or ''


def load_manifest():
    if MANIFEST.is_file():
        return json.loads(MANIFEST.read_text(encoding='utf-8'))
    return {}


def save_manifest(manifest):
    tmp = MANIFEST.with_suffix('.tmp')
    tmp.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    tmp.replace(MANIFEST)


def connect():
    api = Confluence(load_credentials())
    root = api.page(tiny_link_to_page_id(ROOT_TINY_LINK))
    print(f'root page: "{root["title"]}" (id {root["id"]})')
    return api, root


def is_sync_version(version):
    """A page's first version comes from this script; later ones carry its version message."""
    return version.get('number') == 1 or str(version.get('message') or '').startswith(SYNC_PREFIX)


def edited_by_hand(api, page):
    """True if the body differs from the last synced version. A rename alone does not count."""
    if is_sync_version(page.get('version') or {}):
        return False
    for version in api.versions(page['id']):
        if is_sync_version(version):
            return page_storage(api.page_at(page['id'], version['number'])) != page_storage(page)
    return True


def show_diff(old, new, old_name, new_name, limit=40):
    diff = list(difflib.unified_diff(old.splitlines(), new.splitlines(), old_name, new_name,
                                     lineterm=''))
    for line in diff[:limit]:
        print('    ' + line)
    if len(diff) > limit:
        print(f'    ... {len(diff) - limit} more lines')


def cmd_migrate(args):
    manifest = load_manifest()
    todo = [(path, parent) for path, parent in DOCS if path not in manifest]
    if not todo:
        print('all pages are already migrated; Confluence is the master now')
        return
    texts = {path: (REPO_ROOT / path).read_text(encoding='utf-8') for path, _ in DOCS}
    repo_titles = {path: split_title(texts[path], path)[0] for path, _ in DOCS}
    titles = dict(repo_titles)  # becomes the actual page titles below
    api, root = connect()
    space_id = root['spaceId']

    # Check everything first: either the whole baseline goes up or nothing does.
    ids = {None: root['id']}
    ids.update({path: entry['id'] for path, entry in manifest.items()})
    existing, conflicts = {}, []
    for path, parent in todo:
        known_id = BASELINE_PAGE_IDS.get(path)
        page = api.page(known_id) if known_id else api.find(space_id, titles[path])
        existing[path] = page
        if page is None:
            continue
        ids[path] = page['id']
        titles[path] = page['title']  # a title changed in Confluence stays
    links = Links(titles, {str(v): k for k, v in ids.items() if k})
    for path, parent in todo:
        page = existing[path]
        if page is None:
            print(f'create   "{titles[path]}"')
            continue
        if ids.get(parent) is not None and str(page.get('parentId')) != str(ids[parent]):
            conflicts.append(path)
            print(f'CONFLICT "{titles[path]}": exists elsewhere in the space (id {page["id"]}, '
                  f'parent {page.get("parentId")})')
        elif edited_by_hand(api, page):
            conflicts.append(path)
            print(f'CONFLICT "{titles[path]}": edited by hand since the last sync '
                  f'(id {page["id"]}, version {page["version"]["number"]})')
            try:
                show_diff(canonical(texts[path], path, titles, links),
                          storage_to_markdown(repo_titles[path], page_storage(page), path, links),
                          f'repo/{path}', 'confluence')
            except ConversionError as e:
                print(f'    (cannot show a diff: {e})')
        else:
            renamed = f', repo title "{repo_titles[path]}"' if titles[path] != repo_titles[path] else ''
            print(f'update   "{titles[path]}" (id {page["id"]}{renamed})')
    if conflicts:
        sys.exit(f'nothing was changed: {len(conflicts)} page(s) need a decision')
    if args.dry_run:
        return

    for path, parent in todo:
        _, storage = to_storage(texts[path], path, titles)
        page = existing[path]
        if page is None:
            page = api.create(space_id, ids[parent], titles[path], storage)
            ids[path] = page['id']
            print(f'created  "{titles[path]}" (id {page["id"]})')
        else:
            digest = SYNC_PREFIX + content_hash(titles[path], ids[parent], storage)
            api.update(page, titles[path], storage, digest)
            print(f'updated  "{titles[path]}" (id {page["id"]})')
        manifest[path] = {'id': str(page['id']), 'parent': parent, 'state': 'published'}
        if titles[path] != repo_titles[path]:
            manifest[path]['repo_title'] = repo_titles[path]  # the file keeps its own heading
        save_manifest(manifest)  # after every page, so an interrupted run can resume


def manifest_pages(api, manifest):
    return {path: api.page(entry['id']) for path, entry in manifest.items()}


def cmd_draft(args):
    manifest = load_manifest()
    path = args.path
    if path in manifest:
        sys.exit(f'{path} already has a page (id {manifest[path]["id"]}); edit it in Confluence')
    if (REPO_ROOT / path).exists():
        sys.exit(f'{path} already exists in the repo; a draft must be a new document')
    if args.parent not in manifest:
        sys.exit(f'parent {args.parent} is not in {MANIFEST.name}; run migrate first')
    text = pathlib.Path(args.file).read_text(encoding='utf-8')
    title, _ = split_title(text, args.file)
    api, root = connect()
    pages = manifest_pages(api, manifest)
    titles = {p: page['title'] for p, page in pages.items()}
    titles[path] = title
    _, storage = to_storage(text, path, titles)
    if api.find(root['spaceId'], title) is not None:
        sys.exit(f'a page titled "{title}" already exists; refusing to overwrite it')
    parent_page = pages[args.parent]
    print(f'create   "{title}" under "{parent_page["title"]}" '
          f'({len(storage)} chars, {storage.count(CODE_MACRO)} code blocks)')
    if args.dry_run:
        return
    page = api.create(root['spaceId'], parent_page['id'], title, storage)
    manifest[path] = {'id': str(page['id']), 'parent': args.parent, 'state': 'draft'}
    save_manifest(manifest)
    print(f'created  "{title}" (id {page["id"]})')
    print(f'{api.base}{(page.get("_links") or {}).get("webui", "")}')


BANNER_RE = re.compile(r'^<ac:structured-macro ac:name="info"[^>]*>(?:(?!</ac:structured-macro>).)*'
                       r'confluence_sync\.py(?:(?!</ac:structured-macro>).)*</ac:structured-macro>',
                       flags=re.S)
FORBIDDEN_LINK_RE = re.compile(r'<a\b[^>]*\bhref="[^"]*' + re.escape(FORBIDDEN_ON_PAGES) +
                               r'[^"]*"[^>]*>(.*?)</a>', flags=re.S)


def scrub_storage(storage):
    """Edits the page source text in place; everything else on the page stays as it is."""
    storage = BANNER_RE.sub('', storage, count=1)
    storage = FORBIDDEN_LINK_RE.sub(lambda m: as_code(m.group(1)), storage)
    parts = re.split(r'(<!\[CDATA\[.*?\]\]>)', storage, flags=re.S)
    parts[1::2] = [scrub_code(part) for part in parts[1::2]]
    return ''.join(parts)


def cmd_cleanup(args):
    manifest = load_manifest()
    api, _ = connect()
    pages = manifest_pages(api, manifest)
    titles = {path: page['title'] for path, page in pages.items()}
    links = Links(titles, {str(e['id']): p for p, e in manifest.items()})
    for path, page in pages.items():
        old = page_storage(page)
        new = scrub_storage(old)
        if new == old:
            print(f'clean    "{page["title"]}"')
            continue
        validate_xhtml(new, path)  # exits if anything forbidden is left
        print(f'{"would fix" if args.dry_run else "fix"} "{page["title"]}" (id {page["id"]})')
        try:
            show_diff(storage_to_markdown(page['title'], old, path, links),
                      storage_to_markdown(page['title'], new, path, links), 'before', 'after')
        except ConversionError as e:
            print(f'    (no Markdown diff: {e})')
        if not args.dry_run:
            parent = manifest[path].get('parent')
            parent_id = manifest[parent]['id'] if parent else page.get('parentId')
            api.update(page, page['title'], new,
                       SYNC_PREFIX + content_hash(page['title'], parent_id, new))


def git_dirty(path):
    result = subprocess.run(['git', '-C', str(REPO_ROOT), 'status', '--porcelain', '--', path],
                            capture_output=True, text=True, check=True)
    return bool(result.stdout.strip())


def cmd_pull(args):
    manifest = load_manifest()
    if not manifest:
        sys.exit(f'{MANIFEST.name} is empty; run migrate first')
    unknown = [p for p in args.paths if p not in manifest]
    if unknown:
        sys.exit(f'not in {MANIFEST.name}: {", ".join(unknown)}')
    targets = args.paths or [p for p, e in manifest.items() if e.get('state') != 'draft']
    skipped = [p for p, e in manifest.items() if not args.paths and e.get('state') == 'draft']

    api, _ = connect()
    pages = manifest_pages(api, manifest)
    titles = {path: page['title'] for path, page in pages.items()}
    links = Links(titles, {str(e['id']): p for p, e in manifest.items()})
    headings = {p: manifest[p].get('repo_title', titles[p]) for p in manifest}

    # Convert and check everything first: either all files are written or none.
    writes, problems = {}, []
    for path in targets:
        local_file = REPO_ROOT / path
        local = local_file.read_text(encoding='utf-8') if local_file.is_file() else None
        known = fence_languages(local) if local else {}
        try:
            new = storage_to_markdown(headings[path], page_storage(pages[path]), path, links, known)
            plain = storage_to_markdown(headings[path], page_storage(pages[path]), path, links)
            if local is not None and canonical(local, path, titles, links) == plain:
                print(f'unchanged {path}')
                continue
        except ConversionError as e:
            problems.append(f'{path}: page cannot be converted:')
            problems.extend(f'    {p}' for p in e.problems)
            continue
        if local is not None and git_dirty(path):
            problems.append(f'{path}: has uncommitted local changes; commit or revert them first')
            continue
        writes[path] = (local, new)
    if problems:
        print('\n'.join(problems), file=sys.stderr)
        sys.exit('nothing was written')

    for path, (local, new) in writes.items():
        print(f'{"would write" if args.dry_run else "wrote"} {path}')
        if args.dry_run:
            show_diff(local or '', new, f'repo/{path}', 'confluence')
            continue
        target = REPO_ROOT / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(new, encoding='utf-8')
    if not args.dry_run:
        for path in targets:
            manifest[path]['state'] = 'published'
        save_manifest(manifest)
    for path in skipped:
        print(f'skipped  {path} (draft: name it explicitly once the code is implemented)')


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='command', required=True)

    p = sub.add_parser('migrate', help='one-time baseline of the existing docs')
    p.add_argument('--dry-run', action='store_true', help='check only, change nothing')
    p.set_defaults(run=cmd_migrate)

    p = sub.add_parser('draft', help='publish a new page from a local draft')
    p.add_argument('file', help='draft Markdown file (first line: "# Title")')
    p.add_argument('--path', required=True, help='repo path the page will be pulled to')
    p.add_argument('--parent', required=True, help='repo path of the parent page')
    p.add_argument('--dry-run', action='store_true', help='check only, change nothing')
    p.set_defaults(run=cmd_draft)

    p = sub.add_parser('cleanup', help='remove the old banner and code-hosting references')
    p.add_argument('--dry-run', action='store_true', help='show the changes, change nothing')
    p.set_defaults(run=cmd_cleanup)

    p = sub.add_parser('pull', help='write reviewed pages to the repo as Markdown')
    p.add_argument('paths', nargs='*', help='repo paths (default: all non-draft pages)')
    p.add_argument('--dry-run', action='store_true', help='show the changes, write nothing')
    p.set_defaults(run=cmd_pull)

    args = ap.parse_args()
    args.run(args)


if __name__ == '__main__':
    main()
