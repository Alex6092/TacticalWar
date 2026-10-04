"""Cartes du tournoi à cases spéciales (braises, sources, hautes herbes) : assets/map/9.json à 11.json.

Usage (depuis la racine du dépôt) : py tools/maps/make_special_maps.py

Chaque carte est symétrique par rotation d'un demi-tour : la case (x, y) a la même tuile que
(largeur - 1 - x, hauteur - 1 - y), et les départs de l'équipe 2 sont ceux de l'équipe 1 tournés. Les
deux équipes ont donc exactement les mêmes cases à portée. On décrit la moitié haute de la carte
(lignes complètes, puis la moitié gauche de la ligne du milieu et sa case centrale) ; le script
complète le reste, vérifie les départs et la connexité, puis écrit la carte au format des autres
(une ligne de texte par ligne de la carte).
"""
import json
import os
from collections import deque

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
MAP_DIR = os.path.join(ROOT, 'assets', 'map')
WIDTH, HEIGHT = 15, 13

# Règles utiles à la vérification (les vraies règles sont dans assets/tiles/tileset.json).
WALKABLE = {'grass', 'dirt', 'sand', 'flagstones', 'embers', 'spring', 'tall_grass'}
SPECIAL = {'embers', 'spring', 'tall_grass'}

MAPS = [
    {
        'id': 9, 'name': 'Cœur du volcan',
        'legend': {'d': 'dirt', 'v': 'stone_volcanic', 'l': 'lava', 'e': 'embers', 's': 'spring', 'h': 'tall_grass', 'f': 'flagstones'},
        'top': [
            'vdddddlllddddvv',
            'dddeddddlddddhd',
            'ddvddddddddeddd',
            'dddddsdvddddvdd',
            'ddhddddddeddddd',
            'dddvdeddddhdldd',
        ],
        'middle': ('ddddddv', 'f'),
        'starts': [(1, 2), (1, 4), (1, 6), (2, 8)],
    },
    {
        'id': 10, 'name': 'Prairie des hautes herbes',
        'legend': {'.': 'grass', 't': 'tree', 'r': 'stone', 'h': 'tall_grass', 's': 'spring', 'e': 'embers'},
        'top': [
            't.....hhh.....t',
            '..r...hh....s..',
            '.....r....hh...',
            '.hh.......hh.r.',
            '.hh...t...e....',
            '....s.....hhh..',
        ],
        'middle': ('.r.....', 'h'),
        'starts': [(1, 1), (0, 5), (1, 7), (2, 10)],
    },
    {
        'id': 11, 'name': 'Oasis brûlante',
        'legend': {'S': 'sand', 'R': 'stone_sand', 'W': 'water', 'o': 'spring', 'e': 'embers', 'b': 'bush', 'h': 'tall_grass', 'f': 'flagstones'},
        'top': [
            'RSSSSSSWWSSSSSR',
            'SSSeSSSSWSSSRSS',
            'SRSSSSoSSSSeSSS',
            'SSSSbSSSSSSSSSS',
            'SeSSSSSRSShhSSS',
            'SSShhSSSSSSSoSS',
        ],
        'middle': ('SSSRSSe', 'f'),
        'starts': [(1, 1), (2, 3), (1, 6), (2, 9)],
    },
]


def build(spec):
    top = spec['top']
    left, center = spec['middle']
    assert len(top) == HEIGHT // 2 and all(len(row) == WIDTH for row in top), spec['name']
    assert len(left) == WIDTH // 2 and len(center) == 1, spec['name']
    middle = left + center + left[::-1]
    rows = top + [middle] + [row[::-1] for row in reversed(top)]
    legend = spec['legend']
    return [[legend[c] for c in row] for row in rows]


def check(spec, grid):
    starts1 = spec['starts']
    starts2 = [(WIDTH - 1 - x, HEIGHT - 1 - y) for x, y in starts1]
    for x, y in starts1 + starts2:
        tile = grid[y][x]
        assert tile in WALKABLE and tile not in SPECIAL, '%s : départ (%d, %d) sur %s' % (spec['name'], x, y, tile)
    assert not set(starts1) & set(starts2), spec['name']

    # Toutes les cases praticables sont accessibles depuis les départs.
    seen = set(starts1)
    queue = deque(starts1)
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < WIDTH and 0 <= ny < HEIGHT and (nx, ny) not in seen and grid[ny][nx] in WALKABLE:
                seen.add((nx, ny))
                queue.append((nx, ny))
    walkable = {(x, y) for y in range(HEIGHT) for x in range(WIDTH) if grid[y][x] in WALKABLE}
    assert walkable <= seen, '%s : cases inaccessibles %s' % (spec['name'], sorted(walkable - seen))
    assert all(start in seen for start in starts2), spec['name']
    counts = {tile: sum(row.count(tile) for row in grid) for tile in sorted(SPECIAL)}
    return starts1, starts2, counts


def write(spec, grid, starts1, starts2):
    palette = []
    for row in grid:
        for tile in row:
            if tile not in palette:
                palette.append(tile)
    root = {
        'format': 'tw-map',
        'height': HEIGHT,
        'id': spec['id'],
        'name': spec['name'],
        'palette': palette,
        'start': {'1': [list(c) for c in starts1], '2': [list(c) for c in starts2]},
        'tournament': True,
        'version': 2,
        'width': WIDTH,
    }
    text = '{\n'
    for key, value in root.items():
        text += '  ' + json.dumps(key) + ': ' + json.dumps(value, ensure_ascii=False, separators=(',', ':')) + ',\n'
    text += '  "tiles": [\n'
    text += ',\n'.join('    ' + json.dumps([palette.index(t) for t in row], separators=(',', ':')) for row in grid)
    text += '\n  ]\n}\n'
    path = os.path.join(MAP_DIR, '%d.json' % spec['id'])
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    return path


if __name__ == '__main__':
    for spec in MAPS:
        grid = build(spec)
        starts1, starts2, counts = check(spec, grid)
        path = write(spec, grid, starts1, starts2)
        print('%-28s %s  %s' % (spec['name'], os.path.relpath(path, ROOT), counts))
