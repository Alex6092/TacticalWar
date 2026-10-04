"""Images des orbes bonus (bonus sur la carte) : assets/ui/orbs/<orbe>.png.

Usage (depuis la racine du dépôt) : py tools/ui/make_orb_sprites.py [--preview]

Même cadre que les blocs de mur (tools/ui/make_block_sprites.py) : 120 x 150 pixels, centre de la
case en (60, 120). L'orbe flotte au-dessus de son ombre ; le jeu le fait monter et descendre.
"""
import argparse
import math
import os

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT_DIR = os.path.join(ROOT, 'assets', 'ui', 'orbs')
WIDTH, HEIGHT = 120, 150
CENTER = (60, 86)
RADIUS = 17
K = 4

ORBS = {
    'soin': (90, 215, 110),
    'energie': (255, 196, 50),
    'protection': (95, 160, 255),
}


def P(x, y):
    return (x * K, y * K)


def symbol(draw, name):
    cx, cy = CENTER
    white = (255, 255, 255, 240)
    if name == 'soin':
        draw.rectangle((P(cx - 3, cy - 10), P(cx + 3, cy + 10)), fill=white)
        draw.rectangle((P(cx - 10, cy - 3), P(cx + 10, cy + 3)), fill=white)
    elif name == 'energie':
        points = []
        for i in range(10):
            r = 11 if i % 2 == 0 else 4.5
            a = -math.pi / 2 + i * math.pi / 5
            points.append(P(cx + r * math.cos(a), cy + r * math.sin(a)))
        draw.polygon(points, fill=white)
    else:
        points = [P(cx - 9, cy - 9), P(cx + 9, cy - 9), P(cx + 9, cy + 1), P(cx, cy + 11), P(cx - 9, cy + 1)]
        draw.polygon(points, fill=white)


def orb(name, color):
    image = Image.new('RGBA', (WIDTH * K, HEIGHT * K), (0, 0, 0, 0))
    cx, cy = CENTER
    # Ombre au sol et halo.
    shadow = Image.new('L', image.size, 0)
    ImageDraw.Draw(shadow).ellipse((P(60 - 16, 120 - 6), P(60 + 16, 120 + 6)), fill=110)
    image.paste(Image.new('RGBA', image.size, (0, 0, 0, 255)), (0, 0), shadow.filter(ImageFilter.GaussianBlur(3 * K)))
    halo = Image.new('L', image.size, 0)
    ImageDraw.Draw(halo).ellipse((P(cx - RADIUS - 8, cy - RADIUS - 8), P(cx + RADIUS + 8, cy + RADIUS + 8)), fill=150)
    image.paste(Image.new('RGBA', image.size, color + (255,)), (0, 0), halo.filter(ImageFilter.GaussianBlur(6 * K)))
    # Sphère : dégradé du bord sombre vers un reflet clair en haut à gauche.
    sphere = Image.new('RGBA', image.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(sphere)
    steps = 24
    for i in range(steps):
        t = i / (steps - 1)
        r = RADIUS * (1 - 0.75 * t)
        ox, oy = -5 * t, -5 * t
        shade = tuple(int(c * (0.55 + 0.45 * t) + 255 * 0.35 * t * t) for c in color)
        shade = tuple(min(255, c) for c in shade)
        d.ellipse((P(cx + ox - r, cy + oy - r), P(cx + ox + r, cy + oy + r)), fill=shade + (255,))
    d.ellipse((P(cx - RADIUS, cy - RADIUS), P(cx + RADIUS, cy + RADIUS)), outline=(255, 255, 255, 200), width=int(1.4 * K))
    symbol(d, name)
    d.ellipse((P(cx - 10, cy - 13), P(cx - 3, cy - 8)), fill=(255, 255, 255, 170))
    image.alpha_composite(sphere)
    return image.resize((WIDTH, HEIGHT), Image.LANCZOS)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--preview', action='store_true')
    args = parser.parse_args()
    os.makedirs(OUT_DIR, exist_ok=True)
    images = []
    for name, color in ORBS.items():
        image = orb(name, color)
        image.save(os.path.join(OUT_DIR, name + '.png'), optimize=True)
        images.append(image)
    print('%d orbes générés dans assets/ui/orbs' % len(images))
    if args.preview:
        sheet = Image.new('RGBA', (WIDTH * len(images), HEIGHT), (70, 120, 70, 255))
        d = ImageDraw.Draw(sheet)
        for i, image in enumerate(images):
            cx, cy = i * WIDTH + 60, 120
            d.polygon([(cx - 60, cy), (cx, cy - 30), (cx + 60, cy), (cx, cy + 30)], fill=(90, 150, 80, 255), outline=(60, 100, 50, 255))
            sheet.alpha_composite(image, (i * WIDTH, 0))
        sheet.save(os.path.join(os.path.dirname(__file__), 'orbs_preview.png'))


if __name__ == '__main__':
    main()
