"""Icônes affichées au-dessus des personnages en combat (assets/ui/characterdata).

Usage (depuis la racine du dépôt) : py tools/ui/make_overlay_icons.py

Le cœur (PV), l'étoile (PA) et le carré (PM) sont d'origine. Le script dessine l'écusson du bouclier
(shield_bg.png, 64 x 64) dans le même style plat : forme pleine, léger dégradé, contour sombre.
Dessiné à 4 fois la taille finale puis réduit (Lanczos).
"""
import math
import os

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUTPUT = os.path.join(ROOT, 'assets', 'ui', 'characterdata')
SIZE = 64
K = 4


def shield_outline(cx, cy, w, h, steps=24):
    """Écusson : bord supérieur légèrement bombé, flancs droits puis pointe arrondie vers le bas."""
    top = cy - h / 2
    points = []
    for i in range(steps + 1):
        t = i / steps
        x = cx - w / 2 + w * t
        points.append((x, top + h * 0.06 * (1 - (2 * t - 1) ** 2) * -1 + h * 0.06))
    for i in range(1, steps + 1):
        t = i / steps
        # Flanc droit : vertical sur le premier tiers, puis courbe jusqu'à la pointe.
        y = top + h * 0.06 + (h * 0.94) * t
        x = cx + (w / 2) * (1 if t < 0.35 else math.cos((t - 0.35) / 0.65 * math.pi / 2))
        points.append((x, y))
    for i in range(steps - 1, -1, -1):
        t = i / steps
        y = top + h * 0.06 + (h * 0.94) * t
        x = cx - (w / 2) * (1 if t < 0.35 else math.cos((t - 0.35) / 0.65 * math.pi / 2))
        points.append((x, y))
    return points


def make_shield(path):
    size = SIZE * K
    image = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    cx, cy = size / 2, size / 2 + size * 0.02
    w, h = size * 0.78, size * 0.86

    # Contour sombre, puis remplissage en dégradé vertical (bleu clair en haut, bleu soutenu en bas).
    mask = Image.new('L', (size, size), 0)
    ImageDraw.Draw(mask).polygon(shield_outline(cx, cy, w, h), fill=255)
    inner = Image.new('L', (size, size), 0)
    ImageDraw.Draw(inner).polygon(shield_outline(cx, cy, w - 6 * K, h - 6 * K), fill=255)

    outline = Image.new('RGBA', (size, size), (20, 45, 95, 255))
    image.paste(outline, (0, 0), mask)
    gradient = Image.new('RGBA', (size, size))
    draw = ImageDraw.Draw(gradient)
    for y in range(size):
        t = y / (size - 1)
        color = (int(95 + (40 - 95) * t), int(170 + (105 - 170) * t), int(245 + (205 - 245) * t), 255)
        draw.line([(0, y), (size, y)], fill=color)
    image.paste(gradient, (0, 0), inner)

    # Reflet clair sur le haut de l'écusson.
    shine = Image.new('L', (size, size), 0)
    ImageDraw.Draw(shine).ellipse((cx - w * 0.28, cy - h * 0.42, cx + w * 0.1, cy - h * 0.18), fill=90)
    image.paste(Image.new('RGBA', (size, size), (255, 255, 255, 255)), (0, 0), Image.composite(shine, Image.new('L', (size, size), 0), inner))

    image.resize((SIZE, SIZE), Image.LANCZOS).save(path, optimize=True)
    print(os.path.relpath(path, ROOT))


if __name__ == '__main__':
    os.makedirs(OUTPUT, exist_ok=True)
    make_shield(os.path.join(OUTPUT, 'shield_bg.png'))
