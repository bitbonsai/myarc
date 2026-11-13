"""Build and publish only website artifacts, preserving unrelated repository work."""
from pathlib import Path
import base64
import hashlib
import json
import subprocess
import sys
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parent
REPO = 'bitbonsai/myarc'
BRANCH = 'gh-pages'
# Generated subtrees replaced whole on each publish, so removed pages disappear.
OWNED = ('blog', 'docs')
IDENTITY = {'name': 'bitbonsai', 'email': '44185+bitbonsai@users.noreply.github.com'}


def artifacts(root):
    names = ('index.html', 'og-image.png', 'og-image.webp', 'product-window-light.webp', 'product-window-dark.webp', 'favicon.png', 'favicon.ico', 'robots.txt', 'sitemap.xml', 'index.md', 'download/index.html', 'download/index.md', 'llms.txt', 'llms-full.txt')
    files = {'.nojekyll': b'', 'CNAME': b'myarc.dev\n'}
    paths = [root / name for name in names]
    for folder in OWNED:
        if not (root / folder / 'index.html').is_file():
            raise ValueError(f'Build the {folder} pages before publication')
        paths.extend(path for path in (root / folder).rglob('*') if path.is_file() or path.is_symlink())
    if len(paths) > 5000:
        raise ValueError('Website exceeds the 5,000-file deployment limit')
    for path in paths:
        if path.is_symlink():
            raise ValueError(f'Artifact cannot be a symlink: {path}')
        path.resolve().relative_to(root.resolve())
        files[path.relative_to(root).as_posix()] = path.read_bytes()
    return files


def blob_sha(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def main():
    subprocess.run([sys.executable, str(ROOT / 'build.py')], check=True)
    subprocess.run([sys.executable, str(ROOT / 'check.py')], check=True)
    subprocess.run([sys.executable, str(ROOT / 'check_seo.py')], check=True)
    files = artifacts(ROOT)
    token = subprocess.check_output(['env', '-u', 'GH_TOKEN', '-u', 'GITHUB_TOKEN', 'gh', 'auth', 'token', '--hostname', 'github.com', '--user', 'bitbonsai'], text=True).strip()

    def api(path, method='GET', body=None):
        request = urllib.request.Request('https://api.github.com/' + path,
            data=None if body is None else json.dumps(body).encode(), method=method,
            headers={'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
                     'Content-Type': 'application/json', 'X-GitHub-Api-Version': '2022-11-28',
                     'User-Agent': 'myArc-site-publisher'})
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                raw = response.read()
                return json.loads(raw) if raw else None
        except urllib.error.HTTPError as error:
            try:
                message = json.load(error).get('message', error.reason)
            except (ValueError, AttributeError):
                message = error.reason
            raise RuntimeError(f'{method} {path}: HTTP {error.code}: {message}') from None

    if api('user')['login'] != 'bitbonsai':
        raise ValueError('Publication requires the bitbonsai GitHub identity')
    repo = api('repos/' + REPO)
    if repo['visibility'] != 'public' or not repo['permissions'].get('push'):
        raise ValueError('Expected the public myArc repository with push access')
    pages = api(f'repos/{REPO}/pages')
    if pages.get('cname') != 'myarc.dev' or not pages.get('https_enforced') or pages.get('source') != {'branch': BRANCH, 'path': '/'}:
        raise ValueError('Pages domain, HTTPS or source changed; review before publishing')
    parent = api(f'repos/{REPO}/git/ref/heads/{BRANCH}')['object']['sha']
    base = api(f'repos/{REPO}/git/commits/{parent}')['tree']['sha']
    previous = api(f'repos/{REPO}/git/trees/{base}?recursive=1')
    if previous.get('truncated'):
        raise ValueError('Remote artifact tree is truncated; publication stopped')
    known = {entry['path']: entry['sha'] for entry in previous['tree'] if entry['type'] == 'blob'}
    entries = []
    owned = {folder: [] for folder in OWNED}
    for name, data in sorted(files.items()):
        sha = blob_sha(data)
        if known.get(name) != sha:
            sha = api(f'repos/{REPO}/git/blobs', 'POST', {'content': base64.b64encode(data).decode('ascii'), 'encoding': 'base64'})['sha']
        entry = {'path': name, 'mode': '100644', 'type': 'blob', 'sha': sha}
        folder, _, rest = name.partition('/')
        if folder in owned and rest:
            owned[folder].append(dict(entry, path=rest))
        else:
            entries.append(entry)
    # Replace the owned subtrees so unpublished posts, removed docs and removed screenshots disappear.
    for folder, tree in owned.items():
        sha = api(f'repos/{REPO}/git/trees', 'POST', {'tree': tree})['sha']
        entries.append({'path': folder, 'mode': '040000', 'type': 'tree', 'sha': sha})
    tree = api(f'repos/{REPO}/git/trees', 'POST', {'base_tree': base, 'tree': entries})['sha']
    if tree == base:
        print('Website artifacts already match the deployment branch')
        return
    commit = api(f'repos/{REPO}/git/commits', 'POST', {'message': 'Publish myArc website and progress journal', 'tree': tree, 'parents': [parent], 'author': IDENTITY, 'committer': IDENTITY})
    api(f'repos/{REPO}/git/refs/heads/{BRANCH}', 'PATCH', {'sha': commit['sha'], 'force': False})
    receipt = {'commit': commit['sha'], 'branch': BRANCH, 'url': 'https://myarc.dev/', 'artifacts': sorted(files)}
    (ROOT / '.publish-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, RuntimeError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'Publication failed: {error}') from None
