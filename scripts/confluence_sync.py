#!/usr/bin/env python3
"""Publishes the design docs to Confluence, one page per Markdown file.

    python scripts/confluence_sync.py              create / update the pages
    python scripts/confluence_sync.py --dry-run    convert and validate only, no network
    python scripts/confluence_sync.py --dump DIR   also write the converted pages to DIR

Credentials come from ~/.confluence/confluence.env (override the path
with RFS_CONFLUENCE_ENV), never from the repository:

    CONFLUENCE_BASE_URL=https://<site>.atlassian.net/wiki
    CONFLUENCE_EMAIL=<email of the Atlassian account that owns the token>
    CONFLUENCE_API_TOKEN=<API token>

Environment variables with the same names take precedence.

The repository is the source of truth. Every page starts with a note saying
so, and a page is only rewritten when its converted content changed (the
content hash is kept in the page's version message). A page with the same
title elsewhere in the space is never overwritten.

Requires the `markdown` package (installed by docker/setup_ubuntu.sh;
elsewhere: pip install markdown).
"""

import argparse
import base64
import hashlib
import html
import json
import os
import pathlib
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET

import markdown

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO_URL = 'https://github.com/ravii/ruhnix-flight-system'

# Parent page for everything below: https://<site>/wiki/x/<ROOT_TINY_LINK>
# (the space home page "My first space" of space MFS).
ROOT_TINY_LINK = 'dIAE'

# (Markdown file, parent file or None for the root page). Parents first.
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


def code_macro(match):
    lang = CODE_LANGUAGES.get(match.group(1) or '', 'none')
    code = html.unescape(match.group(2)).rstrip('\n')
    return ('<ac:structured-macro ac:name="code">'
            f'<ac:parameter ac:name="language">{lang}</ac:parameter>'
            f'<ac:plain-text-body>{cdata(code)}</ac:plain-text-body>'
            '</ac:structured-macro>')


def github_url(repo_path):
    kind = 'tree' if (REPO_ROOT / repo_path).is_dir() else 'blob'
    return f'{REPO_URL}/{kind}/main/{urllib.parse.quote(repo_path)}'


def rewrite_link(match, doc_path, titles):
    href, body = html.unescape(match.group(1)), match.group(2)
    if re.match(r'^[a-z]+:', href) or href.startswith('#'):
        return match.group(0)
    target, _, _anchor = href.partition('#')
    resolved = os.path.normpath(os.path.join(os.path.dirname(doc_path), target)).replace('\\', '/')
    if resolved in titles:
        title = html.escape(titles[resolved], quote=True)
        return (f'<ac:link><ri:page ri:content-title="{title}" />'
                f'<ac:link-body>{body}</ac:link-body></ac:link>')
    return f'<a href="{html.escape(github_url(resolved), quote=True)}">{body}</a>'


def to_storage(doc_path, titles):
    title, body_md = split_title((REPO_ROOT / doc_path).read_text(encoding='utf-8'), doc_path)
    body = markdown.markdown(body_md, extensions=['tables', 'fenced_code'], output_format='xhtml')
    body = re.sub(r'<pre><code(?: class="language-([\w+-]+)")?>(.*?)</code></pre>', code_macro,
                  body, flags=re.S)
    body = re.sub(r'<a href="([^"]+)">(.*?)</a>', lambda m: rewrite_link(m, doc_path, titles),
                  body, flags=re.S)
    note = ('<ac:structured-macro ac:name="info"><ac:rich-text-body><p>'
            f'本页由仓库文件 <a href="{github_url(doc_path)}"><code>{doc_path}</code></a> '
            '通过 <code>scripts/confluence_sync.py</code> 自动同步。请在仓库中修改，'
            '直接在这里编辑的内容会在下次同步时被覆盖。'
            '</p></ac:rich-text-body></ac:structured-macro>')
    storage = note + body
    validate_xhtml(storage, doc_path)
    return title, storage


def validate_xhtml(storage, doc_path):
    try:
        ET.fromstring('<root xmlns:ac="urn:ac" xmlns:ri="urn:ri">' + storage + '</root>')
    except ET.ParseError as e:
        sys.exit(f'{doc_path}: converted page is not well-formed XHTML: {e}')


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
        return self.request('GET', f'/api/v2/pages/{page_id}')

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


# ---- main ----------------------------------------------------------------------

def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--dry-run', action='store_true', help='convert and validate only')
    ap.add_argument('--dump', metavar='DIR', help='write converted storage XHTML to DIR')
    args = ap.parse_args()

    titles = {}
    for path, _ in DOCS:
        titles[path], _ = split_title((REPO_ROOT / path).read_text(encoding='utf-8'), path)
    pages = [(path, parent) + to_storage(path, titles) for path, parent in DOCS]

    if args.dump:
        out = pathlib.Path(args.dump)
        out.mkdir(parents=True, exist_ok=True)
        for path, _, _, storage in pages:
            (out / (path.replace('/', '__') + '.xhtml')).write_text(storage, encoding='utf-8')
    if args.dry_run:
        for path, parent, title, storage in pages:
            print(f'{path:34} -> "{title}"  (parent: {titles.get(parent, "root page")}, '
                  f'{len(storage)} chars, {storage.count(CODE_MACRO)} code blocks)')
        return

    api = Confluence(load_credentials())
    root = api.page(tiny_link_to_page_id(ROOT_TINY_LINK))
    space_id = root['spaceId']
    page_ids = {None: root['id']}
    print(f'root page: "{root["title"]}" (id {root["id"]})')

    for path, parent, title, storage in pages:
        parent_id = page_ids[parent]
        digest = SYNC_PREFIX + content_hash(title, parent_id, storage)
        existing = api.find(space_id, title)
        if existing is None:
            created = api.create(space_id, parent_id, title, storage)
            page_ids[path] = created['id']
            print(f'created  "{title}" (id {created["id"]})')
            continue
        if str(existing.get('parentId')) != str(parent_id):
            sys.exit(f'"{title}" already exists elsewhere in the space (id {existing["id"]}, '
                     f'parent {existing.get("parentId")}); refusing to overwrite it')
        page_ids[path] = existing['id']
        if (existing.get('version') or {}).get('message') == digest:
            print(f'unchanged "{title}"')
            continue
        api.update(existing, title, storage, digest)
        print(f'updated  "{title}" (id {existing["id"]})')


if __name__ == '__main__':
    main()
