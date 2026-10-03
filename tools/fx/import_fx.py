"""Importe des animations de sorts (atlas libGDX : image + .txt) dans assets/spellsprites.

Usage (depuis la racine du dépôt) :
    py tools/fx/import_fx.py                       # effets de tools/fx/selection.json
    py tools/fx/import_fx.py --source D:/exode/spell --preview

Chaque animation est réduite à la taille utile pour le jeu (côté le plus long de chaque image
limité à "maxFrameSize" pixels), puis réécrite avec un atlas au même format. Options par effet
dans selection.json : "as" (nom de la planche produite), "grayscale" (niveaux de gris clairs, à
teinter dans le catalogue d'effets), "step" (une image sur n, pour les animations très longues).
La flèche des tirs de l'Archer, la bulle de bouclier, l'aura au sol des effets durables et le
signal d'équipe (anneau et flèche), absents des animations d'origine, sont dessinés par le script
(en blanc, teintés par le catalogue d'effets).
"""
import argparse
import json
import math
import os
import re
import sys

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HERE = os.path.dirname(os.path.abspath(__file__))
OUTPUT = os.path.join(ROOT, 'assets', 'spellsprites')


def read_atlas(path):
    """Régions d'un atlas libGDX, dans l'ordre de leur index."""
    regions = []
    current = None
    for line in open(path, encoding='utf-8', errors='replace').read().splitlines():
        if not line.strip():
            continue
        if not line.startswith(' ') and ':' not in line:
            current = {'name': line.strip()}
            regions.append(current)
            continue
        if current is None or ':' not in line:
            continue
        key, value = [part.strip() for part in line.split(':', 1)]
        if key in ('xy', 'size', 'orig', 'offset'):
            current[key] = tuple(int(v) for v in value.split(','))
        elif key == 'rotate':
            current[key] = value == 'true'
        elif key == 'index':
            current[key] = int(value)
    regions = [r for r in regions if 'xy' in r and 'size' in r]
    for region in regions:
        if region.get('rotate'):
            raise ValueError('%s : région pivotée non prise en charge' % path)
    return sorted(regions, key=lambda r: r.get('index', 0))


def resize(image, size):
    if image.size == size:
        return image
    return image.convert('RGBa').resize(size, Image.LANCZOS).convert('RGBA')


def write_sheet(name, frames, output):
    """Range les images en grille et écrit l'image et l'atlas."""
    width, height = frames[0].size
    columns = max(1, min(len(frames), int(math.ceil(math.sqrt(len(frames) * height / max(1, width))))))
    columns = min(columns, max(1, 2048 // width))
    rows = int(math.ceil(len(frames) / columns))
    sheet = Image.new('RGBA', (columns * width, rows * height), (0, 0, 0, 0))
    lines = ['', name + '.png', 'size: %d, %d' % sheet.size, 'format: RGBA8888', 'filter: Linear,Linear', 'repeat: none']
    for index, frame in enumerate(frames):
        x = (index % columns) * width
        y = (index // columns) * height
        sheet.paste(frame, (x, y))
        lines += [name, '  rotate: false', '  xy: %d, %d' % (x, y), '  size: %d, %d' % (width, height),
                  '  orig: %d, %d' % (width, height), '  offset: 0, 0', '  index: %d' % index]
    sheet.save(os.path.join(output, name + '.png'), optimize=True)
    with open(os.path.join(output, name + '.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    return sheet.size


def to_gray(image):
    """Niveaux de gris éclaircis (la couleur est donnée par le catalogue d'effets), transparence conservée."""
    r, g, b, a = image.split()
    luminance = Image.merge('RGB', (r, g, b)).convert('L').point(lambda v: int(255 * (v / 255.0) ** 0.6))
    return Image.merge('RGBA', (luminance, luminance, luminance, a))


def import_effect(source, name, max_frame, output, as_name=None, grayscale=False, step=1):
    anim = os.path.join(source, 'anim')
    regions = read_atlas(os.path.join(anim, name + '.txt'))[::max(1, step)]
    image = Image.open(os.path.join(anim, name + '.png')).convert('RGBA')
    width, height = regions[0]['size']
    factor = min(1.0, max_frame / float(max(width, height)))
    size = (max(1, round(width * factor)), max(1, round(height * factor)))
    frames = []
    for region in regions:
        x, y = region['xy']
        w, h = region['size']
        frame = resize(image.crop((x, y, x + w, y + h)), size)
        frames.append(to_gray(frame) if grayscale else frame)
    sheet = write_sheet(as_name or name, frames, output)
    return len(frames), size, sheet


def make_arrow(output):
    """Flèche (pointe à droite) : hampe en bois, pointe métallique, empennage."""
    k = 4
    width, height = 72, 18
    image = Image.new('RGBA', (width * k, height * k), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    middle = height * k / 2
    draw.rectangle((10 * k, middle - 1.2 * k, 58 * k, middle + 1.2 * k), fill=(122, 84, 46, 255), outline=(60, 38, 18, 255), width=k // 2)
    draw.polygon([(width * k - 2 * k, middle), (56 * k, middle - 5 * k), (59 * k, middle), (56 * k, middle + 5 * k)],
                 fill=(205, 212, 222, 255), outline=(70, 74, 82, 255))
    for side in (-1, 1):
        draw.polygon([(4 * k, middle + side * 7 * k), (16 * k, middle + side * 1.5 * k), (22 * k, middle + side * 1.5 * k), (12 * k, middle + side * 7 * k)],
                     fill=(236, 236, 236, 255), outline=(150, 40, 40, 255))
    frame = image.resize((width, height), Image.LANCZOS)
    write_sheet('arrow', [frame], output)


def make_bubble(output, frames=8, size=128):
    """Bulle de bouclier : sphère translucide au bord lumineux, qui pulse doucement."""
    k = 4
    result = []
    for i in range(frames):
        pulse = 0.5 + 0.5 * math.sin(2 * math.pi * i / frames)
        image = Image.new('RGBA', (size * k, size * k), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        center = size * k / 2
        radius = size * k * (0.40 + 0.02 * pulse)
        # Remplissage de plus en plus opaque vers le bord.
        for step in range(40, 0, -1):
            r = radius * step / 40
            alpha = int(18 + 70 * (step / 40) ** 3)
            draw.ellipse((center - r, center - r, center + r, center + r), fill=(255, 255, 255, alpha))
        draw.ellipse((center - radius, center - radius, center + radius, center + radius), outline=(255, 255, 255, 230), width=3 * k)
        # Reflet.
        hx, hy, hr = center - radius * 0.38, center - radius * 0.42, radius * 0.22
        draw.ellipse((hx - hr, hy - hr * 0.6, hx + hr, hy + hr * 0.6), fill=(255, 255, 255, int(150 + 60 * pulse)))
        result.append(image.resize((size, size), Image.LANCZOS))
    write_sheet('bubble', result, output)


def make_aura(output, frames=12, width=128, height=64):
    """Aura au sol : anneau elliptique lumineux qui pulse (blanc, teinté par le catalogue)."""
    k = 4
    result = []
    for i in range(frames):
        pulse = 0.5 + 0.5 * math.sin(2 * math.pi * i / frames)
        image = Image.new('RGBA', (width * k, height * k), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        cx, cy = width * k / 2, height * k / 2
        rx, ry = width * k * (0.36 + 0.03 * pulse), height * k * (0.30 + 0.03 * pulse)
        for step in range(12, 0, -1):
            grow = step * k * 0.9
            alpha = int((120 + 100 * pulse) * (1 - step / 13) ** 2)
            draw.ellipse((cx - rx - grow, cy - ry - grow / 2, cx + rx + grow, cy + ry + grow / 2), outline=(255, 255, 255, alpha), width=k)
        draw.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), outline=(255, 255, 255, int(170 + 80 * pulse)), width=2 * k)
        result.append(image.resize((width, height), Image.LANCZOS))
    write_sheet('aura', result, output)


def make_ping_ring(output, frames=12, width=128, height=64):
    """Signal d'équipe au sol : anneau elliptique qui s'élargit et s'efface (blanc, teinté par le catalogue)."""
    k = 4
    result = []
    for i in range(frames):
        t = i / float(frames)
        image = Image.new('RGBA', (width * k, height * k), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        cx, cy = width * k / 2, height * k / 2
        # Anneau qui s'élargit en s'effaçant, et un anneau fixe au centre.
        rx, ry = width * k * (0.12 + 0.34 * t), height * k * (0.12 + 0.34 * t)
        alpha = int(255 * (1 - t) ** 1.2)
        draw.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), outline=(255, 255, 255, alpha), width=3 * k)
        draw.ellipse((cx - width * k * 0.14, cy - height * k * 0.14, cx + width * k * 0.14, cy + height * k * 0.14),
                     outline=(255, 255, 255, 230), width=2 * k)
        result.append(image.resize((width, height), Image.LANCZOS))
    write_sheet('ping_ring', result, output)


def make_ping_arrow(output, frames=12, width=40, height=64):
    """Flèche du signal d'équipe : chevron pointé vers le bas qui rebondit (blanc, teinté par le catalogue)."""
    k = 4
    result = []
    for i in range(frames):
        bounce = abs(math.sin(math.pi * i / frames)) * 10 * k
        image = Image.new('RGBA', (width * k, height * k), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        top = 4 * k + (10 * k - bounce)
        cx = width * k / 2
        points = [(cx - 9 * k, top), (cx + 9 * k, top), (cx + 9 * k, top + 22 * k), (cx + 17 * k, top + 22 * k),
                  (cx, top + 46 * k), (cx - 17 * k, top + 22 * k), (cx - 9 * k, top + 22 * k)]
        draw.polygon(points, fill=(255, 255, 255, 255), outline=(60, 50, 20, 255))
        draw.line(points + [points[0]], fill=(60, 50, 20, 255), width=2 * k)
        result.append(image.resize((width, height), Image.LANCZOS))
    write_sheet('ping_arrow', result, output)


def make_preview(names, output, path):
    """Planche : image du milieu de chaque animation, avec son nom."""
    thumbs = []
    for name in names:
        regions = read_atlas(os.path.join(output, name + '.txt'))
        sheet = Image.open(os.path.join(output, name + '.png'))
        region = regions[len(regions) // 2]
        x, y = region['xy']
        w, h = region['size']
        frame = sheet.crop((x, y, x + w, y + h))
        frame.thumbnail((160, 160))
        thumbs.append((name, frame))
    columns = 6
    rows = int(math.ceil(len(thumbs) / columns))
    preview = Image.new('RGBA', (columns * 180, rows * 190), (36, 38, 52, 255))
    draw = ImageDraw.Draw(preview)
    for index, (name, frame) in enumerate(thumbs):
        cx = (index % columns) * 180
        cy = (index // columns) * 190
        preview.alpha_composite(frame, (cx + (180 - frame.width) // 2, cy + 8 + (160 - frame.height) // 2))
        draw.text((cx + 6, cy + 172), name, fill=(230, 230, 230, 255))
    preview.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--selection', default=os.path.join(HERE, 'selection.json'))
    parser.add_argument('--source', help='dossier "spell" du projet Exode (contient anim/ et sound/)')
    parser.add_argument('--preview', action='store_true', help='écrit tools/fx/preview.png')
    args = parser.parse_args()

    selection = json.load(open(args.selection, encoding='utf-8'))
    source = args.source or selection['source']
    os.makedirs(OUTPUT, exist_ok=True)

    names = []
    for effect in selection['effects']:
        name = effect.get('as', effect['name'])
        count, size, sheet = import_effect(source, effect['name'], effect.get('maxFrameSize', selection['maxFrameSize']), OUTPUT,
                                           name, effect.get('grayscale', False), effect.get('step', 1))
        names.append(name)
        print('%-24s %3d images de %dx%d (planche %dx%d)' % (name, count, size[0], size[1], sheet[0], sheet[1]))

    make_arrow(OUTPUT)
    make_bubble(OUTPUT)
    make_aura(OUTPUT)
    make_ping_ring(OUTPUT)
    make_ping_arrow(OUTPUT)
    names += ['arrow', 'bubble', 'aura', 'ping_ring', 'ping_arrow']
    print('arrow, bubble, aura, ping  dessinées')

    if args.preview:
        path = os.path.join(HERE, 'preview.png')
        make_preview(names, OUTPUT, path)
        print('Aperçu :', os.path.relpath(path, ROOT))


if __name__ == '__main__':
    sys.exit(main())
