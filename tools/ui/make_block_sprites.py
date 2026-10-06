"""Images des blocs de mur des sorts de terrain : assets/ui/blocks/<sort>.png.

Usage (depuis la racine du dépôt) : py tools/ui/make_block_sprites.py [--preview]

Chaque image fait 120 x 150 pixels ; le centre de la case (losange de 120 x 60) est en (60, 120),
point d'ancrage utilisé par le rendu (IsometricRenderer, "props"). Dessin à 4x puis réduction
(Lanczos), comme les icônes des sorts.
"""
import argparse
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT_DIR = os.path.join(ROOT, 'assets', 'ui', 'blocks')
WIDTH, HEIGHT = 120, 150
ANCHOR = (60, 120)
K = 4


def P(x, y):
    return (x * K, y * K)


def poly(draw, points, fill, outline=None, width=0):
    draw.polygon([P(x, y) for x, y in points], fill=fill)
    if outline is not None:
        draw.line([P(x, y) for x, y in points + [points[0]]], fill=outline, width=int(width * K), joint='curve')


def canvas():
    return Image.new('RGBA', (WIDTH * K, HEIGHT * K), (0, 0, 0, 0))


def finish(image):
    return image.resize((WIDTH, HEIGHT), Image.LANCZOS)


def shadow(image, rx=34, ry=14, alpha=90):
    """Ombre au sol, sous l'objet."""
    layer = Image.new('L', image.size, 0)
    cx, cy = ANCHOR
    ImageDraw.Draw(layer).ellipse((P(cx - rx, cy - ry), P(cx + rx, cy + ry)), fill=alpha)
    layer = layer.filter(ImageFilter.GaussianBlur(4 * K))
    image.paste(Image.new('RGBA', image.size, (0, 0, 0, 255)), (0, 0), layer)


def eboulis():
    image = canvas()
    shadow(image, 40, 17, 110)
    d = ImageDraw.Draw(image)
    rng = random.Random(7)
    dark = (40, 36, 34, 255)

    def boulder(cx, cy, rx, ry, base):
        points = []
        for i in range(9):
            a = math.pi * 2 * i / 9
            r = 1 + rng.uniform(-0.12, 0.12)
            points.append((cx + rx * r * math.cos(a), cy + ry * r * math.sin(a)))
        poly(d, points, base, dark, 1.6)
        # Facette éclairée en haut à gauche, face sombre en bas à droite.
        light = tuple(min(255, c + 45) for c in base[:3]) + (255,)
        shade = tuple(max(0, c - 40) for c in base[:3]) + (255,)
        poly(d, [points[4], points[5], points[6], (cx, cy - ry * 0.1)], light)
        poly(d, [points[0], points[1], points[2], (cx + rx * 0.1, cy + ry * 0.1)], shade)

    boulder(60, 92, 34, 28, (128, 120, 112, 255))
    boulder(36, 112, 16, 11, (112, 104, 98, 255))
    boulder(84, 110, 18, 12, (120, 112, 104, 255))
    boulder(62, 64, 20, 16, (140, 132, 122, 255))
    return finish(image)


def mur_de_glace():
    image = canvas()
    shadow(image, 38, 16, 70)
    d = ImageDraw.Draw(image)
    cx, cy = ANCHOR
    hw, hh, h = 40, 20, 78
    left = [(cx - hw, cy), (cx, cy + hh), (cx, cy + hh - h), (cx - hw, cy - h)]
    right = [(cx, cy + hh), (cx + hw, cy), (cx + hw, cy - h), (cx, cy + hh - h)]
    top = [(cx - hw, cy - h), (cx, cy + hh - h), (cx + hw, cy - h), (cx, cy - hh - h)]
    edge = (225, 248, 255, 255)
    poly(d, left, (110, 185, 235, 215))
    poly(d, right, (75, 150, 215, 215))
    poly(d, top, (200, 240, 255, 235))
    for face in (left, right, top):
        d.line([P(x, y) for x, y in face + [face[0]]], fill=edge, width=int(1.4 * K))
    # Reflets et fissures.
    d.line([P(cx - 30, cy - 60), P(cx - 22, cy - 6)], fill=(255, 255, 255, 170), width=int(3 * K))
    d.line([P(cx - 20, cy - 66), P(cx - 15, cy - 30)], fill=(255, 255, 255, 120), width=int(2 * K))
    d.line([P(cx + 16, cy - 50), P(cx + 22, cy - 34), P(cx + 14, cy - 20), P(cx + 24, cy - 4)], fill=(40, 100, 160, 200), width=int(1.4 * K))
    d.line([P(cx - 10, cy - 30), P(cx - 4, cy - 18)], fill=(40, 100, 160, 180), width=int(1.2 * K))
    return finish(image)


def palissade():
    image = canvas()
    shadow(image, 36, 14, 80)
    d = ImageDraw.Draw(image)
    dark = (50, 32, 18, 255)
    for x, y, h in ((36, 114, 56), (60, 104, 66), (84, 114, 56)):
        w = 8
        body = [(x - w, y), (x - w, y - h), (x, y - h - 14), (x + w, y - h), (x + w, y)]
        poly(d, body, (150, 100, 55, 255), dark, 1.4)
        poly(d, [(x, y), (x, y - h - 14), (x + w, y - h), (x + w, y)], (120, 78, 42, 255))
        d.line([P(x - w, y - h), P(x, y - h - 14), P(x + w, y - h)], fill=dark, width=int(1.4 * K))
        for k in (0.35, 0.7):
            d.line([P(x - w + 2, y - h * k), P(x - 2, y - h * k + 4)], fill=(95, 60, 32, 255), width=int(1 * K))
    # Traverse de corde.
    d.line([P(28, 86), P(60, 74), P(92, 86)], fill=(200, 170, 110, 255), width=int(3 * K))
    return finish(image)


def voile_sacre():
    image = canvas()
    cx, cy = ANCHOR
    bands = Image.new('L', image.size, 0)
    g = ImageDraw.Draw(bands)
    # Rideau de lumière : bandes ondulées, qui s'estompent vers le haut.
    for i, x in enumerate((30, 44, 58, 72, 86)):
        points = [(x + 4 * math.sin(t * 0.35 + i), cy + 6 - t * 2.4) for t in range(0, 41)]
        g.line([P(px, py) for px, py in points], fill=255, width=int(8 * K))
    bands = bands.filter(ImageFilter.GaussianBlur(1.5 * K))
    fade = Image.new('L', image.size, 0)
    fd = ImageDraw.Draw(fade)
    for y in range(HEIGHT * K):
        level = max(0, min(255, int(255 * (y / K - 20) / 50)))
        fd.line([(0, y), (WIDTH * K, y)], fill=level)
    alpha = Image.composite(bands, Image.new('L', image.size, 0), fade)
    halo = alpha.filter(ImageFilter.GaussianBlur(7 * K))
    image.paste(Image.new('RGBA', image.size, (255, 205, 90, 255)), (0, 0), Image.eval(halo, lambda v: min(255, v * 2) * 130 // 255))
    image.paste(Image.new('RGBA', image.size, (255, 240, 170, 255)), (0, 0), Image.eval(alpha, lambda v: v * 215 // 255))
    d = ImageDraw.Draw(image)
    rng = random.Random(3)
    for _ in range(16):
        x = rng.uniform(26, 94)
        y = rng.uniform(44, 118)
        r = rng.uniform(1.2, 2.4)
        d.ellipse((P(x - r, y - r), P(x + r, y + r)), fill=(255, 255, 235, 240))
    return finish(image)


SPRITES = {'eboulis': eboulis, 'mur_de_glace': mur_de_glace, 'palissade': palissade, 'voile_sacre': voile_sacre}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--preview', action='store_true')
    args = parser.parse_args()
    os.makedirs(OUT_DIR, exist_ok=True)
    images = []
    for name, draw in SPRITES.items():
        image = draw()
        image.save(os.path.join(OUT_DIR, name + '.png'), optimize=True)
        images.append(image)
    print('%d images de blocs générées dans assets/ui/blocks' % len(images))
    if args.preview:
        sheet = Image.new('RGBA', (WIDTH * len(images), HEIGHT), (70, 120, 70, 255))
        d = ImageDraw.Draw(sheet)
        for i, image in enumerate(images):
            cx, cy = i * WIDTH + ANCHOR[0], ANCHOR[1]
            d.polygon([(cx - 60, cy), (cx, cy - 30), (cx + 60, cy), (cx, cy + 30)], fill=(90, 150, 80, 255), outline=(60, 100, 50, 255))
            sheet.alpha_composite(image, (i * WIDTH, 0))
        sheet.save(os.path.join(os.path.dirname(__file__), 'blocks_preview.png'))


if __name__ == '__main__':
    main()
