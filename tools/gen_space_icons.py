#!/usr/bin/env python3
"""Generate browser/foundation/space_icon_data.cc for the Space icon picker.

  gen_space_icons.py EMOJI_TEST ANNOTATIONS ANNOTATIONS_DERIVED LUCIDE_DIR

EMOJI_TEST is Unicode's emoji-test.txt (15.1). ANNOTATIONS and
ANNOTATIONS_DERIVED are CLDR's English annotations.json files from
cldr-json's cldr-annotations-full and cldr-annotations-derived-full. LUCIDE_DIR
holds lucide-static 1.61.0 icons/*.svg. All are read as data only.

The emoji list keeps fully-qualified emoji without skin-tone variants, in
Unicode's order, sorted into Arc's eight picker tabs. Each entry carries its
CLDR name and keywords for search. Lucide icons become Chromium vector-icon
paths in absolute commands at their 24-unit canvas and stroke 2.
"""

import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'browser/foundation/space_icon_data.cc'

# Unicode group -> picker tab. Component (skin tones, hair) is left out.
CATEGORIES = {
    'Smileys & Emotion': 'kSmileys',
    'People & Body': 'kSmileys',
    'Animals & Nature': 'kNature',
    'Food & Drink': 'kFood',
    'Activities': 'kActivities',
    'Travel & Places': 'kTravel',
    'Objects': 'kObjects',
    'Symbols': 'kSymbols',
    'Flags': 'kFlags',
}
SKIN_TONES = set(range(0x1F3FB, 0x1F400))
MAX_KEYWORDS = 8

# Icons offered on the Icon tab, in grid order. Names are Lucide's and are
# what a Space stores.
SPACE_ICONS = [
    'star', 'heart', 'house', 'briefcase', 'book-open', 'graduation-cap',
    'code', 'terminal', 'music', 'headphones', 'camera', 'film', 'gamepad-2',
    'palette', 'pen-tool', 'coffee', 'utensils', 'shopping-cart', 'plane',
    'car', 'globe', 'map-pin', 'rocket', 'lightbulb', 'flame', 'zap', 'leaf',
    'sun', 'moon', 'cloud', 'umbrella', 'gift', 'trophy', 'dumbbell', 'wallet',
    'chart-column', 'calendar', 'mail', 'users', 'flask-conical', 'wrench',
    'shield', 'folder', 'sparkles', 'dog',
]
# The Emoji tab's category buttons, in tab order.
TAB_ICONS = ['smile', 'sun', 'coffee', 'award', 'truck', 'gift', 'heart', 'flag']


def c_string(text):
    out = []
    for byte in text.encode('utf-8'):
        ch = chr(byte)
        if byte >= 0x80:
            out.append('\\%03o' % byte)
        elif ch in '"\\':
            out.append('\\' + ch)
        elif ch == '?':
            out.append('\\?')  # No trigraphs.
        else:
            out.append(ch)
    return '"' + ''.join(out) + '"'


def load_emoji(test_path, annotations, derived):
    names = json.loads(Path(annotations).read_text())['annotations']['annotations']
    names.update(json.loads(Path(derived).read_text())['annotationsDerived']['annotations'])
    group = None
    entries = []
    for line in Path(test_path).read_text().splitlines():
        if line.startswith('# group:'):
            group = line.split(':', 1)[1].strip()
            continue
        if not line or line.startswith('#'):
            continue
        codes, rest = line.split(';', 1)
        if not rest.strip().startswith('fully-qualified'):
            continue
        category = CATEGORIES.get(group)
        points = [int(code, 16) for code in codes.split()]
        if category is None or SKIN_TONES & set(points):
            continue
        emoji = ''.join(chr(point) for point in points)
        name = rest.split('#', 1)[1].split(None, 2)[2].strip()
        annotation = names.get(emoji) or names.get(emoji.replace('️', ''))
        words = [name.lower()]
        if annotation:
            words += [w.lower() for w in annotation.get('default', [])
                      if w.lower() not in name.lower()][:MAX_KEYWORDS]
        keywords = '|'.join(dict.fromkeys(w.replace('|', ' ') for w in words))
        entries.append((emoji, keywords, category))
    return entries


NUMBER = re.compile(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?')
ARITY = {'M': 2, 'L': 2, 'H': 1, 'V': 1, 'C': 6, 'S': 4, 'Q': 4, 'T': 2,
         'A': 7, 'Z': 0}


def fmt(value):
    value = round(value, 3)
    if value == int(value):
        return str(int(value))
    return ('%.3f' % value).rstrip('0').rstrip('.') + 'f'


def arc_tokens(path_d):
    # Arc flags may be packed without separators ("a1 1 0 011 1").
    tokens = []
    command = None
    index = 0
    raw = re.findall(r'[MmLlHhVvCcSsQqTtAaZz]|[^MmLlHhVvCcSsQqTtAaZz]+', path_d)
    for part in raw:
        if part.isalpha():
            command = part
            tokens.append(part)
            index = 0
            continue
        if command in 'Aa':
            pos = 0
            while pos < len(part):
                if part[pos] in ' ,\t\n':
                    pos += 1
                    continue
                slot = index % 7
                if slot in (3, 4):
                    tokens.append(part[pos])
                    pos += 1
                else:
                    match = NUMBER.match(part, pos)
                    tokens.append(match.group())
                    pos = match.end()
                index += 1
        else:
            tokens += NUMBER.findall(part)
    return tokens


def convert_path(path_d):
    """SVG path data -> absolute Chromium vector commands."""
    tokens = arc_tokens(path_d)
    out = []
    x = y = 0.0
    start = (0.0, 0.0)
    last_cubic = None  # Second control point of the previous C/S.
    last_quad = None   # Control point of the previous Q/T.
    pos = 0
    command = None
    while pos < len(tokens):
        if tokens[pos].isalpha():
            command = tokens[pos]
            pos += 1
            if command in 'Zz':
                out.append(['CLOSE'])
                x, y = start
                last_cubic = last_quad = None
                continue
        upper = command.upper()
        rel = command.islower()
        args = [float(t) for t in tokens[pos:pos + ARITY[upper]]]
        pos += ARITY[upper]
        if upper == 'M':
            nx, ny = args
            if rel:
                nx, ny = x + nx, y + ny
            out.append(['MOVE_TO', nx, ny])
            x, y = start = (nx, ny)
            # Further pairs are implicit line-tos.
            command = 'l' if rel else 'L'
            last_cubic = last_quad = None
        elif upper in 'LHV':
            if upper == 'L':
                nx, ny = args
                if rel:
                    nx, ny = x + nx, y + ny
            elif upper == 'H':
                nx, ny = (x + args[0] if rel else args[0]), y
            else:
                nx, ny = x, (y + args[0] if rel else args[0])
            out.append(['LINE_TO', nx, ny])
            x, y = nx, ny
            last_cubic = last_quad = None
        elif upper == 'C':
            pts = [(args[i] + (x if rel else 0), args[i + 1] + (y if rel else 0))
                   for i in (0, 2, 4)]
            out.append(['CUBIC_TO', *pts[0], *pts[1], *pts[2]])
            last_cubic = pts[1]
            x, y = pts[2]
            last_quad = None
        elif upper == 'S':
            c1 = (2 * x - last_cubic[0], 2 * y - last_cubic[1]) if last_cubic else (x, y)
            pts = [(args[i] + (x if rel else 0), args[i + 1] + (y if rel else 0))
                   for i in (0, 2)]
            out.append(['CUBIC_TO', *c1, *pts[0], *pts[1]])
            last_cubic = pts[0]
            x, y = pts[1]
            last_quad = None
        elif upper == 'Q':
            pts = [(args[i] + (x if rel else 0), args[i + 1] + (y if rel else 0))
                   for i in (0, 2)]
            out.append(['QUADRATIC_TO', *pts[0], *pts[1]])
            last_quad = pts[0]
            x, y = pts[1]
            last_cubic = None
        elif upper == 'T':
            c = (2 * x - last_quad[0], 2 * y - last_quad[1]) if last_quad else (x, y)
            nx, ny = args
            if rel:
                nx, ny = x + nx, y + ny
            out.append(['QUADRATIC_TO', *c, nx, ny])
            last_quad = c
            x, y = nx, ny
            last_cubic = None
        elif upper == 'A':
            rx, ry, angle, large, sweep, nx, ny = args
            if rel:
                nx, ny = x + nx, y + ny
            out.append(['ARC_TO', rx, ry, angle, large, sweep, nx, ny])
            x, y = nx, ny
            last_cubic = last_quad = None
    return out


def convert_svg(path):
    root = ET.parse(path).getroot()
    commands = []
    for element in root:
        tag = element.tag.split('}')[-1]
        a = {k: float(v) for k, v in element.attrib.items()
             if k not in ('d', 'fill', 'points')}
        if tag == 'path':
            commands += convert_path(element.attrib['d'])
        elif tag == 'circle':
            commands.append(['CIRCLE', a['cx'], a['cy'], a['r']])
        elif tag == 'ellipse':
            commands.append(['OVAL', a['cx'], a['cy'], a['rx'], a['ry']])
        elif tag == 'rect':
            commands.append(['ROUND_RECT', a.get('x', 0), a.get('y', 0),
                             a['width'], a['height'], a.get('rx', a.get('ry', 0))])
        elif tag == 'line':
            commands += [['MOVE_TO', a['x1'], a['y1']], ['LINE_TO', a['x2'], a['y2']]]
        elif tag in ('polyline', 'polygon'):
            values = [float(v) for v in NUMBER.findall(element.attrib['points'])]
            pairs = list(zip(values[::2], values[1::2]))
            commands.append(['MOVE_TO', *pairs[0]])
            commands += [['LINE_TO', *pair] for pair in pairs[1:]]
            if tag == 'polygon':
                commands.append(['CLOSE'])
        else:
            raise SystemExit(f'{path}: unsupported <{tag}>')
    return commands


def identifier(name):
    return 'k' + ''.join(part.capitalize() for part in name.split('-'))


def icon_source(name, svg_dir):
    commands = convert_svg(Path(svg_dir) / f'{name}.svg')
    body = ['gfx::CANVAS_DIMENSIONS, 24', 'gfx::STROKE, 2']
    for command in commands:
        body.append(', '.join(['gfx::' + command[0]] + [fmt(v) for v in command[1:]]))
    ident = identifier(name)
    lines = [f'constexpr gfx::PathElement {ident}Path[] = {{']
    lines += [f'    {item},' for item in body]
    lines.append('};')
    lines.append(f'constexpr gfx::VectorIconRep {ident}Rep[] = {{{{{ident}Path}}}};')
    lines.append(f'constexpr gfx::VectorIcon {ident}Icon({ident}Rep, '
                 f'std::size({ident}Rep), "myarc_space_{name.replace("-", "_")}");')
    return '\n'.join(lines)


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    test, annotations, derived, svg_dir = sys.argv[1:]
    emoji = load_emoji(test, annotations, derived)
    icons = list(dict.fromkeys(SPACE_ICONS + TAB_ICONS))
    parts = [
        '// Generated by tools/gen_space_icons.py. Do not edit.',
        '//',
        '// Emoji, names and keywords: Unicode emoji-test.txt 15.1 and CLDR 49',
        '// English annotations, under the Unicode License v3. Icons:',
        '// lucide-static 1.61.0, under ISC. See ATTRIBUTION.md.',
        '',
        '#include <array>',
        '#include <iterator>',
        '',
        '#include "myarc/space_icon_catalog.h"',
        '#include "ui/gfx/vector_icon_types.h"',
        '',
        'namespace myarc {',
        'namespace {',
        '',
        '// clang-format off',
    ]
    parts += [icon_source(name, svg_dir) for name in icons]
    parts.append('')
    parts.append('constexpr EmojiEntry kEmoji[] = {')
    for text, keywords, category in emoji:
        parts.append(f'    {{{c_string(text)}, {c_string(keywords)}, '
                     f'EmojiCategory::{category}}},')
    parts.append('};')
    parts.append('')
    parts.append('constexpr NamedSpaceIcon kNamedIcons[] = {')
    for name in SPACE_ICONS:
        parts.append(f'    {{"{name}", &{identifier(name)}Icon}},')
    parts.append('};')
    parts.append('')
    parts.append(f'constexpr std::array<const gfx::VectorIcon*, {len(TAB_ICONS)}> kTabIcons = {{')
    parts += [f'    &{identifier(name)}Icon,' for name in TAB_ICONS]
    parts.append('};')
    parts += [
        '// clang-format on',
        '',
        '}  // namespace',
        '',
        'base::span<const EmojiEntry> BundledEmoji() {',
        '  return kEmoji;',
        '}',
        '',
        'base::span<const NamedSpaceIcon> BundledSpaceIcons() {',
        '  return kNamedIcons;',
        '}',
        '',
        'const gfx::VectorIcon& EmojiCategoryIcon(EmojiCategory category) {',
        '  return *kTabIcons[static_cast<size_t>(category)];',
        '}',
        '',
        '}  // namespace myarc',
        '',
    ]
    OUT.write_text('\n'.join(parts))
    print(f'{OUT.relative_to(ROOT)}: {len(emoji)} emoji, {len(SPACE_ICONS)} icons')


if __name__ == '__main__':
    main()
