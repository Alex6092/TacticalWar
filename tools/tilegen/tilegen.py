"""Génère des variantes de tuiles à partir des peintures d'origine (assets/tiles/*.png).

Usage (depuis la racine du dépôt) :
    py tools/tilegen/tilegen.py              # génère les tuiles et met à jour assets/tiles/tileset.json
    py tools/tilegen/tilegen.py --preview    # idem + planche d'aperçu (tools/tilegen/preview.png)
    py tools/tilegen/tilegen.py --only sand,lava

Chaque tuile est décrite dans recipes.json : une base (peinture d'origine) et une liste d'opérations
(recoloration HSV d'une zone, bruit, miroir, ajout d'un élément d'une autre base). Les zones (masques)
sont calculées à partir de la teinte des pixels et de leur position par rapport à l'ancre de la case :
  top     surface du bloc (herbe)        sides   flancs de terre du bloc
  object  rocher posé sur le bloc         leaves  feuillage (au-dessus du bloc)
  trunk   tronc d'arbre                   water   eau                       all   toute la tuile
Les calculs se font à une échelle de travail (4x la taille finale), puis l'image est réduite
(Lanczos, alpha prémultiplié) à la taille du jeu. Les tirages aléatoires sont seedés : le résultat
est reproductible.
"""
import argparse
import json
import os
import re
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HERE = os.path.dirname(os.path.abspath(__file__))
TILESET = os.path.join(ROOT, 'assets', 'tiles', 'tileset.json')

# Taille de la face supérieure d'une case, en pixels du jeu (losange 120 x 60).
HALF_W = 60.0
HALF_H = 30.0


# ---------------------------------------------------------------------------
# Couleurs
# ---------------------------------------------------------------------------

def rgb_to_hsv(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx = np.max(rgb, axis=-1)
    mn = np.min(rgb, axis=-1)
    d = mx - mn
    h = np.zeros_like(mx)
    nz = d > 1e-6
    rr = nz & (mx == r)
    gg = nz & (mx == g) & ~rr
    bb = nz & ~rr & ~gg
    h[rr] = ((g - b)[rr] / d[rr]) % 6
    h[gg] = (b - r)[gg] / d[gg] + 2
    h[bb] = (r - g)[bb] / d[bb] + 4
    h = h * 60.0
    s = np.where(mx > 1e-6, d / np.maximum(mx, 1e-6), 0)
    return np.stack([h, s, mx], axis=-1)


def hsv_to_rgb(hsv):
    h = (hsv[..., 0] % 360.0) / 60.0
    s = np.clip(hsv[..., 1], 0, 1)
    v = np.clip(hsv[..., 2], 0, 1)
    i = np.floor(h).astype(int) % 6
    f = h - np.floor(h)
    p = v * (1 - s)
    q = v * (1 - s * f)
    t = v * (1 - s * (1 - f))
    choices = [
        np.stack([v, t, p], -1), np.stack([q, v, p], -1), np.stack([p, v, t], -1),
        np.stack([p, q, v], -1), np.stack([t, p, v], -1), np.stack([v, p, q], -1),
    ]
    out = np.zeros(hsv.shape, dtype=np.float32)
    for k in range(6):
        out[i == k] = choices[k][i == k]
    return out


def hue_band(h, lo, hi, soft=8.0):
    """1 dans [lo, hi] (degrés, gère le passage par 0), décroissance douce aux bords."""
    if lo > hi:
        return np.maximum(hue_band(h, lo, 360.0 + soft, soft), hue_band(h, -soft, hi, soft))
    return np.clip(np.minimum(h - (lo - soft), (hi + soft) - h) / soft, 0, 1)


def circular_mean(h, weight):
    total = weight.sum()
    if total <= 1e-6:
        return 0.0
    rad = np.radians(h)
    return float(np.degrees(np.arctan2((np.sin(rad) * weight).sum(), (np.cos(rad) * weight).sum())) % 360)


# ---------------------------------------------------------------------------
# Images
# ---------------------------------------------------------------------------

def load_rgba(path, size):
    image = Image.open(path).convert('RGBA')
    image = image.convert('RGBa').resize(size, Image.LANCZOS).convert('RGBA')
    return np.asarray(image).astype(np.float32) / 255.0


def to_image(array):
    return Image.fromarray((np.clip(array, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA')


def resize_rgba(array, size):
    return np.asarray(to_image(array).convert('RGBa').resize(size, Image.LANCZOS).convert('RGBA')).astype(np.float32) / 255.0


def blur_mask(mask, radius):
    if radius <= 0:
        return mask
    image = Image.fromarray((np.clip(mask, 0, 1) * 255).astype(np.uint8), 'L')
    return np.asarray(image.filter(ImageFilter.GaussianBlur(radius))).astype(np.float32) / 255.0


def value_noise(shape, cell, rng):
    """Bruit lisse dans [0, 1] : grille aléatoire agrandie en bicubique."""
    h, w = shape
    gh, gw = max(2, int(h / cell) + 2), max(2, int(w / cell) + 2)
    grid = (rng.random((gh, gw)) * 255).astype(np.uint8)
    image = Image.fromarray(grid, 'L').resize((int(gw * cell), int(gh * cell)), Image.BICUBIC)
    return np.asarray(image).astype(np.float32)[:h, :w] / 255.0


# ---------------------------------------------------------------------------
# Masques
# ---------------------------------------------------------------------------

class Layer:
    """Une tuile en cours de fabrication : pixels (échelle de travail) et ancre."""

    def __init__(self, rgba, anchor, factor):
        self.rgba = rgba
        self.anchor = anchor          # en pixels de travail
        self.factor = factor          # pixels de travail par pixel du jeu
        self.cached_masks = None

    def copy(self):
        layer = Layer(self.rgba.copy(), tuple(self.anchor), self.factor)
        layer.cached_masks = self.cached_masks
        return layer

    def hsv(self):
        return rgb_to_hsv(self.rgba[..., :3])

    def masks(self):
        """Zones de la tuile, calculées sur ses couleurs d'origine : une recoloration ne les change
        pas (sinon une herbe décolorée ne serait plus reconnue comme surface). Elles sont recalculées
        après une opération qui déplace les pixels (miroir, ajout d'un élément)."""
        if self.cached_masks is None:
            self.cached_masks = self.compute_masks()
        return self.cached_masks

    def invalidate(self):
        self.cached_masks = None

    def compute_masks(self):
        rgba = self.rgba
        alpha = rgba[..., 3]
        hsv = rgb_to_hsv(rgba[..., :3])
        h, s, v = hsv[..., 0], hsv[..., 1], hsv[..., 2]
        height, width = alpha.shape
        ys, xs = np.mgrid[0:height, 0:width].astype(np.float32)
        ax, ay = self.anchor
        k = self.factor
        dx = np.abs(xs - ax) / (HALF_W * k)
        dy = (ys - ay) / (HALF_H * k)
        inside_top = (dx + np.abs(dy)) <= 1.08
        # Bord inférieur de la face supérieure : en dessous, ce sont les flancs du bloc.
        lower_edge = ay + HALF_H * k * (1 - np.minimum(dx, 1)) - 4 * k

        opaque = np.clip((alpha - 0.05) / 0.5, 0, 1)
        vegetation = hue_band(h, 40, 165) * np.clip((s - 0.12) / 0.1, 0, 1)
        brown = hue_band(h, 340, 35) * np.clip((s - 0.2) / 0.1, 0, 1)
        purple = np.maximum(hue_band(h, 200, 340), np.clip((0.2 - s) / 0.08, 0, 1) * (1 - vegetation))

        below_edge = (ys >= lower_edge) & (dx <= 1.05)
        above_block = ys < (ay - HALF_H * k * 0.93)

        masks = {
            'all': opaque,
            'top': vegetation * inside_top * ~above_block * opaque,
            'sides': brown * below_edge * opaque,
            'object': purple * ~below_edge * opaque,
            'leaves': above_block * opaque,
            'water': hue_band(h, 160, 215) * opaque,
        }
        masks['trunk'] = np.clip(opaque * ~below_edge * (1 - vegetation) * (1 - masks['object']) * (1 - masks['top']), 0, 1)
        return {name: blur_mask(mask.astype(np.float32), 0.6 * k) for name, mask in masks.items()}


# ---------------------------------------------------------------------------
# Opérations
# ---------------------------------------------------------------------------

def recolor(rgba, mask, op):
    hsv = rgb_to_hsv(rgba[..., :3])
    new = hsv.copy()
    if 'hue' in op:
        mean = circular_mean(hsv[..., 0], mask * hsv[..., 1])
        new[..., 0] = hsv[..., 0] + (op['hue'] - mean)
    new[..., 0] += op.get('hueShift', 0)
    new[..., 1] = hsv[..., 1] * op.get('sat', 1.0)
    new[..., 2] = hsv[..., 2] * op.get('val', 1.0) + op.get('valAdd', 0.0)
    lift = op.get('lift', 0.0)  # Éclaircit vers le blanc (neige)
    if lift:
        new[..., 2] = new[..., 2] * (1 - lift) + lift
    rgb = hsv_to_rgb(new)
    m = mask[..., None]
    rgba[..., :3] = rgba[..., :3] * (1 - m) + rgb * m


def op_recolor(layer, op, rng):
    recolor(layer.rgba, layer.masks()[op.get('mask', 'all')], op)


def op_noise(layer, op, rng):
    masks = layer.masks()
    mask = masks[op.get('mask', 'all')]
    noise = value_noise(mask.shape, op.get('scale', 12) * layer.factor, rng)
    factor = 1 + op.get('amount', 0.1) * (noise - 0.5) * 2
    layer.rgba[..., :3] = np.clip(layer.rgba[..., :3] * (1 - mask[..., None]) + layer.rgba[..., :3] * factor[..., None] * mask[..., None], 0, 1)


def op_highlight(layer, op, rng):
    """Couvre de neige les parties claires (faces éclairées) d'une zone."""
    mask = layer.masks()[op.get('mask', 'object')]
    hsv = layer.hsv()
    threshold = op.get('threshold', 0.55)
    bright = np.clip((hsv[..., 2] - threshold) / 0.12, 0, 1) * mask
    color = np.array(op.get('color', [0.93, 0.95, 1.0]), dtype=np.float32)
    amount = bright[..., None] * op.get('amount', 0.9)
    layer.rgba[..., :3] = layer.rgba[..., :3] * (1 - amount) + color * amount


def op_mirror(layer, op, rng):
    layer.rgba = layer.rgba[:, ::-1].copy()
    layer.invalidate()
    width = layer.rgba.shape[1]
    layer.anchor = (width - 1 - layer.anchor[0], layer.anchor[1])


def op_flagstones(layer, op, rng):
    """Dalles : joints sombres sur la face supérieure, parallèles aux bords du losange."""
    top = layer.masks()['top']
    height, width = top.shape
    ys, xs = np.mgrid[0:height, 0:width].astype(np.float32)
    ax, ay = layer.anchor
    k = layer.factor
    # Coordonnées de la case (u, v dans [-1, 1]) le long des deux axes du losange.
    u = (xs - ax) / (HALF_W * k) + (ys - ay) / (HALF_H * k)
    v = (ys - ay) / (HALF_H * k) - (xs - ax) / (HALF_W * k)
    count = op.get('count', 3)
    width_joint = op.get('joint', 0.035)
    joints = np.zeros_like(top)
    for i in range(1, count):
        t = -1 + 2 * i / count
        joints = np.maximum(joints, np.clip(1 - np.abs(u - t) / width_joint, 0, 1))
        joints = np.maximum(joints, np.clip(1 - np.abs(v - t) / width_joint, 0, 1))
    darkness = joints * top * op.get('darkness', 0.45)
    # Légère variation de teinte d'une dalle à l'autre.
    cells = (np.floor((u + 1) * count / 2) * 7 + np.floor((v + 1) * count / 2) * 13).astype(int)
    shades = 1 + (rng.random(64) - 0.5) * op.get('variation', 0.12)
    shade = shades[np.clip(cells, 0, 63)]
    rgb = layer.rgba[..., :3] * (1 - top[..., None] + top[..., None] * shade[..., None])
    layer.rgba[..., :3] = np.clip(rgb * (1 - darkness[..., None]), 0, 1)


def op_overlay(layer, op, rng, bases):
    """Ajoute un élément d'une autre base (ex : feuillage d'arbre réduit = buisson)."""
    source = bases[op['from']].copy()
    mask = source.masks()[op.get('mask', 'all')]
    piece = source.rgba.copy()
    piece[..., 3] *= mask
    if 'tint' in op:
        recolor(piece, np.ones(mask.shape, dtype=np.float32), op['tint'])
    ys, xs = np.nonzero(piece[..., 3] > 0.02)
    if len(xs) == 0:
        return
    piece = piece[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    scale = op.get('scale', 0.5)
    size = (max(1, round(piece.shape[1] * scale)), max(1, round(piece.shape[0] * scale)))
    piece = resize_rgba(piece, size)

    for offset in op.get('at', [[0, 0]]):
        # Position : bas de l'élément centré sur l'ancre + décalage (en pixels du jeu).
        cx = layer.anchor[0] + offset[0] * layer.factor
        bottom = layer.anchor[1] + offset[1] * layer.factor
        x0 = int(round(cx - size[0] / 2))
        y0 = int(round(bottom - size[1]))
        paste(layer, piece, x0, y0)
    layer.invalidate()


def paste(layer, piece, x0, y0):
    height, width = layer.rgba.shape[:2]
    # Agrandit le calque si l'élément dépasse (ex : buisson plus haut que le bloc).
    pad_top = max(0, -y0)
    pad_left = max(0, -x0)
    pad_right = max(0, x0 + piece.shape[1] - width)
    pad_bottom = max(0, y0 + piece.shape[0] - height)
    if pad_top or pad_left or pad_right or pad_bottom:
        layer.rgba = np.pad(layer.rgba, ((pad_top, pad_bottom), (pad_left, pad_right), (0, 0)))
        layer.anchor = (layer.anchor[0] + pad_left, layer.anchor[1] + pad_top)
        x0 += pad_left
        y0 += pad_top
    region = layer.rgba[y0:y0 + piece.shape[0], x0:x0 + piece.shape[1]]
    a = piece[..., 3:4]
    region[..., :3] = piece[..., :3] * a + region[..., :3] * (1 - a)
    region[..., 3:4] = a + region[..., 3:4] * (1 - a)


OPERATIONS = {
    'recolor': op_recolor,
    'noise': op_noise,
    'highlight': op_highlight,
    'mirror': op_mirror,
    'flagstones': op_flagstones,
}


# ---------------------------------------------------------------------------
# Jeu de tuiles
# ---------------------------------------------------------------------------

def load_tileset():
    with open(TILESET, encoding='utf-8-sig') as f:
        return json.load(f)


def dump_tileset(tileset):
    text = json.dumps(tileset, ensure_ascii=False, indent=2)
    # Tableaux de nombres sur une ligne ("anchor": [66, 45]).
    text = re.sub(r'\[\s+(-?[\d.]+),\s+(-?[\d.]+)\s+\]', r'[\1, \2]', text)
    with open(TILESET, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text + '\n')


def merge_tile(tileset, recipe, texture, anchor):
    """Champs gérés par le générateur : texture, ancre. Les autres ne sont écrits que s'ils
    sont absents, pour conserver les retouches faites à la main dans tileset.json."""
    tiles = tileset.setdefault('tiles', [])
    entry = next((t for t in tiles if t.get('id') == recipe['id']), None)
    if entry is None:
        entry = {'id': recipe['id']}
        tiles.append(entry)
    for key in ('name', 'category', 'group', 'shader', 'walkable', 'blocksLineOfSight'):
        if key in recipe and key not in entry:
            entry[key] = recipe[key]
    entry['texture'] = texture
    entry['anchor'] = [round(anchor[0], 1), round(anchor[1], 1)]
    entry['generated'] = True


# ---------------------------------------------------------------------------
# Aperçu
# ---------------------------------------------------------------------------

def render_preview(tileset, ids, path):
    """Planche : chaque tuile au centre d'un carré 3 x 3 d'herbe, dessiné de l'arrière vers l'avant."""
    tiles = {t['id']: t for t in tileset['tiles']}
    images = {}

    def image_of(tile_id):
        if tile_id not in images:
            tile = tiles[tile_id]
            images[tile_id] = Image.open(os.path.join(ROOT, tile['texture'])).convert('RGBA') if tile.get('texture') else None
        return images[tile_id]

    columns = 5
    cell_w, cell_h = 380, 300
    rows = (len(ids) + columns - 1) // columns
    sheet = Image.new('RGBA', (columns * cell_w, rows * cell_h), (30, 32, 45, 255))
    for index, tile_id in enumerate(ids):
        ox = (index % columns) * cell_w + cell_w // 2 - 60
        oy = (index // columns) * cell_h + 110
        for d in range(5):
            for x in range(max(0, d - 2), min(2, d) + 1):
                y = d - x
                current = tile_id if (x, y) == (1, 1) else 'grass'
                image = image_of(current)
                if image is None:
                    continue
                ax, ay = tiles[current]['anchor']
                cx = (x - y) * 60 + 60 + ox
                cy = (x + y) * 30 + 30 + oy
                sheet.alpha_composite(image, (int(cx - ax), int(cy - ay)))
    sheet.save(path)


# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--recipes', default=os.path.join(HERE, 'recipes.json'))
    parser.add_argument('--only', help='identifiants des tuiles à générer, séparés par des virgules')
    parser.add_argument('--preview', action='store_true', help='écrit tools/tilegen/preview.png')
    args = parser.parse_args()

    with open(args.recipes, encoding='utf-8') as f:
        recipes = json.load(f)
    tileset = load_tileset()
    known = {t['id']: t for t in tileset['tiles']}

    factor = recipes.get('supersampling', 4)
    output_dir = os.path.join(ROOT, recipes['outputDir'])
    os.makedirs(output_dir, exist_ok=True)

    # Bases : peintures d'origine à l'échelle de travail (taille de la tuile du jeu x supersampling),
    # ancre reprise du jeu de tuiles : les variantes se superposent exactement à la tuile d'origine.
    bases = {}
    final_sizes = {}
    for name, base in recipes['bases'].items():
        path = os.path.join(ROOT, base['source'])
        tile = known[base['tile']]
        with Image.open(os.path.join(ROOT, tile['texture'])) as game_texture:
            final_sizes[name] = game_texture.size
        anchor = tile['anchor']
        rgba = load_rgba(path, (final_sizes[name][0] * factor, final_sizes[name][1] * factor))
        bases[name] = Layer(rgba, (anchor[0] * factor, anchor[1] * factor), factor)

    only = set(args.only.split(',')) if args.only else None
    generated = []
    for recipe in recipes['tiles']:
        if only and recipe['id'] not in only:
            continue
        rng = np.random.default_rng(recipes.get('seed', 1) * 1000003 + sum(map(ord, recipe['id'])))
        layer = bases[recipe['base']].copy()
        for op in recipe.get('ops', []):
            if op['op'] == 'overlay':
                op_overlay(layer, op, rng, bases)
            else:
                OPERATIONS[op['op']](layer, op, rng)

        # Taille finale : celle de la base (ou proportionnelle si le calque a été agrandi).
        base_h, base_w = bases[recipe['base']].rgba.shape[:2]
        fw, fh = final_sizes[recipe['base']]
        size = (max(1, round(layer.rgba.shape[1] * fw / base_w)), max(1, round(layer.rgba.shape[0] * fh / base_h)))
        final = resize_rgba(layer.rgba, size)
        texture = '/'.join([recipes['outputDir'], recipe['id'] + '.png'])
        to_image(final).save(os.path.join(ROOT, texture), optimize=True)

        anchor = (layer.anchor[0] * size[0] / layer.rgba.shape[1], layer.anchor[1] * size[1] / layer.rgba.shape[0])
        merge_tile(tileset, recipe, texture, anchor)
        generated.append(recipe['id'])
        print('%-16s %-24s %dx%d  ancre (%.1f, %.1f)' % (recipe['id'], recipe.get('name', ''), size[0], size[1], anchor[0], anchor[1]))

    dump_tileset(tileset)
    print('%d tuile(s) générée(s), %s mis à jour.' % (len(generated), os.path.relpath(TILESET, ROOT)))

    if args.preview:
        path = os.path.join(HERE, 'preview.png')
        render_preview(tileset, [r['id'] for r in recipes['tiles'] if r['id'] in generated or not only], path)
        print('Aperçu :', os.path.relpath(path, ROOT))


if __name__ == '__main__':
    sys.exit(main())
