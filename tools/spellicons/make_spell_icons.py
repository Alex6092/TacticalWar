"""Génère une icône par sort (assets/spellicons/<id du sort>.png, 100 x 100) et met à jour gamedata.json.

Usage (depuis la racine du dépôt) : py tools/spellicons/make_spell_icons.py [--preview]

Style : fond dégradé à la couleur de la classe, symbole clair cerné de sombre. Les symboles sont
dessinés en vecteurs à 4x la taille finale puis réduits (Lanczos).
"""
import argparse
import json
import math
import os
import re

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
GAMEDATA = os.path.join(ROOT, 'assets', 'data', 'gamedata.json')
OUT_DIR = os.path.join(ROOT, 'assets', 'spellicons')
SIZE = 100
S = 4 * SIZE  # Taille de travail

LIGHT = (250, 240, 215, 255)
DARK = (25, 18, 14, 255)

CLASS_COLORS = {
    'Guerrier': ((170, 50, 35), (60, 15, 10)),
    'Archer': ((70, 140, 55), (18, 45, 18)),
    'Mage': ((80, 70, 190), (20, 15, 60)),
    'Protecteur': ((205, 160, 50), (80, 50, 10)),
}


def P(x, y):
    """Coordonnées en fraction de l'icône."""
    return (x * S, y * S)


def poly(draw, points, fill):
    draw.polygon([P(x, y) for x, y in points], fill=fill)


def line(draw, points, width, fill):
    draw.line([P(x, y) for x, y in points], fill=fill, width=int(width * S), joint='curve')
    for x, y in (points[0], points[-1]):
        r = width * S / 2
        draw.ellipse((x * S - r, y * S - r, x * S + r, y * S + r), fill=fill)


def circle(draw, cx, cy, r, fill=None, outline=None, width=0.0):
    draw.ellipse((P(cx - r, cy - r), P(cx + r, cy + r)), fill=fill, outline=outline, width=int(width * S))


def rotate(points, angle, cx=0.5, cy=0.5):
    c, s = math.cos(angle), math.sin(angle)
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in points]


def arrow(draw, x0, y0, x1, y1, width=0.05, head=0.14, fill=LIGHT, fletching=True):
    """Flèche (tir) de (x0, y0) vers (x1, y1)."""
    angle = math.atan2(y1 - y0, x1 - x0)
    ux, uy = math.cos(angle), math.sin(angle)
    nx, ny = -uy, ux
    bx, by = x1 - ux * head, y1 - uy * head
    line(draw, [(x0, y0), (bx, by)], width, fill)
    poly(draw, [(x1, y1), (bx + nx * head * 0.6, by + ny * head * 0.6), (bx - nx * head * 0.6, by - ny * head * 0.6)], fill)
    if fletching:
        for k in (0.0, 0.07):
            fx, fy = x0 + ux * k, y0 + uy * k
            poly(draw, [(fx + ux * 0.08, fy + uy * 0.08), (fx + nx * 0.07, fy + ny * 0.07), (fx - ux * 0.02 + nx * 0.07, fy - uy * 0.02 + ny * 0.07)], fill)
            poly(draw, [(fx + ux * 0.08, fy + uy * 0.08), (fx - nx * 0.07, fy - ny * 0.07), (fx - ux * 0.02 - nx * 0.07, fy - uy * 0.02 - ny * 0.07)], fill)


def shield_points(cx=0.5, cy=0.5, w=0.5, h=0.6):
    pts = [(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy)]
    for i in range(1, 12):
        t = i / 12
        pts.append((cx + w / 2 * (1 - t) ** 1.2, cy + h / 2 * math.sin(t * math.pi / 2)))
    pts.append((cx, cy + h / 2))
    for i in range(11, 0, -1):
        t = i / 12
        pts.append((cx - w / 2 * (1 - t) ** 1.2, cy + h / 2 * math.sin(t * math.pi / 2)))
    pts.append((cx - w / 2, cy))
    return pts


def star(cx, cy, r_out, r_in, branches=4, phase=-math.pi / 2):
    pts = []
    for i in range(branches * 2):
        r = r_out if i % 2 == 0 else r_in
        a = phase + i * math.pi / branches
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


# --- Symboles ----------------------------------------------------------------

def taillade(d, accent):
    blade = rotate([(0.47, 0.12), (0.53, 0.12), (0.55, 0.62), (0.5, 0.7), (0.45, 0.62)], math.radians(40))
    poly(d, blade, LIGHT)
    guard = rotate([(0.36, 0.62), (0.64, 0.62), (0.64, 0.67), (0.36, 0.67)], math.radians(40))
    poly(d, guard, accent)
    hilt = rotate([(0.475, 0.67), (0.525, 0.67), (0.525, 0.84), (0.475, 0.84)], math.radians(40))
    poly(d, hilt, accent)
    for k, off in enumerate((0.0, 0.08)):
        d.arc((P(0.08 + off, 0.12 + off), P(0.78 + off, 0.82 + off)), 200, 250, fill=LIGHT, width=int(0.03 * S))


def charge(d, accent):
    poly(d, [(0.3, 0.4), (0.62, 0.4), (0.62, 0.28), (0.86, 0.5), (0.62, 0.72), (0.62, 0.6), (0.3, 0.6)], LIGHT)
    for y, x0 in ((0.32, 0.12), (0.5, 0.06), (0.68, 0.12)):
        line(d, [(x0, y), (x0 + 0.14, y)], 0.04, accent)


def rempart(d, accent):
    wall = [(0.18, 0.3), (0.28, 0.3), (0.28, 0.4), (0.38, 0.4), (0.38, 0.3), (0.48, 0.3), (0.48, 0.4), (0.52, 0.4),
            (0.52, 0.3), (0.62, 0.3), (0.62, 0.4), (0.72, 0.4), (0.72, 0.3), (0.82, 0.3), (0.82, 0.8), (0.18, 0.8)]
    poly(d, wall, LIGHT)
    for y in (0.52, 0.66):
        line(d, [(0.2, y), (0.8, y)], 0.018, accent)
    for x, y0, y1 in ((0.4, 0.42, 0.52), (0.62, 0.42, 0.52), (0.3, 0.52, 0.66), (0.52, 0.52, 0.66), (0.72, 0.52, 0.66), (0.4, 0.66, 0.8), (0.62, 0.66, 0.8)):
        line(d, [(x, y0), (x, y1)], 0.018, accent)


def provocation(d, accent):
    # Aimant en U qui attire.
    d.arc((P(0.22, 0.2), P(0.78, 0.76)), 180, 360, fill=LIGHT, width=int(0.13 * S))
    poly(d, [(0.22, 0.48), (0.35, 0.48), (0.35, 0.72), (0.22, 0.72)], LIGHT)
    poly(d, [(0.65, 0.48), (0.78, 0.48), (0.78, 0.72), (0.65, 0.72)], LIGHT)
    poly(d, [(0.22, 0.64), (0.35, 0.64), (0.35, 0.72), (0.22, 0.72)], accent)
    poly(d, [(0.65, 0.64), (0.78, 0.64), (0.78, 0.72), (0.65, 0.72)], accent)
    for x in (0.4, 0.5, 0.6):
        line(d, [(x, 0.8), (x, 0.9)], 0.025, LIGHT)


def tir_precis(d, accent):
    circle(d, 0.5, 0.5, 0.3, outline=LIGHT, width=0.04)
    circle(d, 0.5, 0.5, 0.15, outline=LIGHT, width=0.035)
    for a, b in (((0.5, 0.1), (0.5, 0.27)), ((0.5, 0.73), (0.5, 0.9)), ((0.1, 0.5), (0.27, 0.5)), ((0.73, 0.5), (0.9, 0.5))):
        line(d, [a, b], 0.035, LIGHT)
    circle(d, 0.5, 0.5, 0.05, fill=accent)


def fleche_empoisonnee(d, accent):
    arrow(d, 0.18, 0.82, 0.7, 0.3)
    drop = [(0.74, 0.5)]
    for i in range(0, 181, 15):
        a = math.radians(i)
        drop.append((0.74 + 0.11 * math.cos(a), 0.72 + 0.11 * math.sin(a)))
    poly(d, drop, (120, 230, 70, 255))


def fleche_recul(d, accent):
    arrow(d, 0.1, 0.5, 0.58, 0.5)
    for x in (0.64, 0.76):
        line(d, [(x, 0.36), (x + 0.1, 0.5), (x, 0.64)], 0.045, accent)


def fleche_entravante(d, accent):
    arrow(d, 0.15, 0.85, 0.75, 0.25)
    for cx, cy in ((0.3, 0.38), (0.42, 0.5), (0.54, 0.62)):
        d.ellipse((P(cx - 0.08, cy - 0.05), P(cx + 0.08, cy + 0.05)), outline=accent, width=int(0.03 * S))


def eclair(d, accent):
    poly(d, [(0.56, 0.08), (0.24, 0.54), (0.46, 0.54), (0.38, 0.92), (0.76, 0.4), (0.53, 0.4), (0.66, 0.08)], LIGHT)
    poly(d, [(0.6, 0.12), (0.4, 0.44), (0.5, 0.44)], accent)


def boule_de_feu(d, accent):
    flame = [(0.12, 0.12), (0.42, 0.34), (0.3, 0.2), (0.58, 0.36), (0.55, 0.22), (0.72, 0.42), (0.6, 0.78), (0.3, 0.62)]
    poly(d, flame, (255, 150, 40, 255))
    circle(d, 0.6, 0.6, 0.2, fill=(255, 190, 70, 255))
    circle(d, 0.63, 0.57, 0.11, fill=LIGHT)


def glyphe_givre(d, accent):
    circle(d, 0.5, 0.5, 0.38, outline=accent, width=0.03)
    for k in range(6):
        a = k * math.pi / 3
        x, y = 0.5 + 0.3 * math.cos(a), 0.5 + 0.3 * math.sin(a)
        line(d, [(0.5, 0.5), (x, y)], 0.04, LIGHT)
        for t in (0.55, 0.75):
            bx, by = 0.5 + 0.3 * t * math.cos(a), 0.5 + 0.3 * t * math.sin(a)
            for side in (-1, 1):
                b = a + side * math.radians(40)
                line(d, [(bx, by), (bx + 0.08 * math.cos(b), by + 0.08 * math.sin(b))], 0.03, LIGHT)


def transposition(d, accent):
    d.arc((P(0.18, 0.18), P(0.82, 0.82)), 200, 340, fill=LIGHT, width=int(0.06 * S))
    d.arc((P(0.18, 0.18), P(0.82, 0.82)), 20, 160, fill=accent, width=int(0.06 * S))
    tip = (0.5 + 0.32 * math.cos(math.radians(340)), 0.5 + 0.32 * math.sin(math.radians(340)))
    poly(d, [(tip[0] + 0.1, tip[1] + 0.02), (tip[0] - 0.06, tip[1] - 0.08), (tip[0] - 0.02, tip[1] + 0.1)], LIGHT)
    tip = (0.5 + 0.32 * math.cos(math.radians(160)), 0.5 + 0.32 * math.sin(math.radians(160)))
    poly(d, [(tip[0] - 0.1, tip[1] - 0.02), (tip[0] + 0.06, tip[1] + 0.08), (tip[0] + 0.02, tip[1] - 0.1)], accent)
    circle(d, 0.5, 0.5, 0.07, fill=LIGHT)


def chatiment(d, accent):
    head = rotate([(0.3, 0.2), (0.7, 0.2), (0.7, 0.42), (0.3, 0.42)], math.radians(-35))
    poly(d, head, LIGHT)
    handle = rotate([(0.47, 0.42), (0.53, 0.42), (0.53, 0.88), (0.47, 0.88)], math.radians(-35))
    poly(d, handle, accent)
    for a in (200, 240, 280):
        r = math.radians(a)
        line(d, [(0.78 + 0.08 * math.cos(r), 0.72 + 0.08 * math.sin(r)), (0.78 + 0.16 * math.cos(r), 0.72 + 0.16 * math.sin(r))], 0.03, LIGHT)


def soin(d, accent):
    circle(d, 0.5, 0.5, 0.36, fill=(255, 255, 255, 60))
    poly(d, [(0.4, 0.18), (0.6, 0.18), (0.6, 0.4), (0.82, 0.4), (0.82, 0.6), (0.6, 0.6), (0.6, 0.82), (0.4, 0.82), (0.4, 0.6), (0.18, 0.6), (0.18, 0.4), (0.4, 0.4)], LIGHT)
    poly(d, [(0.45, 0.24), (0.55, 0.24), (0.55, 0.45), (0.76, 0.45), (0.76, 0.55), (0.55, 0.55), (0.55, 0.76), (0.45, 0.76), (0.45, 0.55), (0.24, 0.55), (0.24, 0.45), (0.45, 0.45)], (120, 220, 110, 255))


def bouclier_sacre(d, accent):
    poly(d, shield_points(0.5, 0.52, 0.56, 0.7), LIGHT)
    poly(d, shield_points(0.5, 0.52, 0.42, 0.56), accent)
    poly(d, star(0.5, 0.48, 0.17, 0.06, 4), LIGHT)


def purification(d, accent):
    poly(d, star(0.45, 0.48, 0.3, 0.07, 4), LIGHT)
    poly(d, star(0.75, 0.25, 0.12, 0.03, 4), LIGHT)
    poly(d, star(0.74, 0.74, 0.09, 0.025, 4), accent)
    poly(d, star(0.2, 0.2, 0.07, 0.02, 4), accent)


def tourbillon(d, accent):
    # Épée au centre, entourée de trois arcs qui tournent.
    blade = [(0.47, 0.2), (0.53, 0.2), (0.54, 0.6), (0.5, 0.66), (0.46, 0.6)]
    poly(d, blade, LIGHT)
    poly(d, [(0.4, 0.6), (0.6, 0.6), (0.6, 0.64), (0.4, 0.64)], accent)
    poly(d, [(0.48, 0.64), (0.52, 0.64), (0.52, 0.78), (0.48, 0.78)], accent)
    for start in (20, 140, 260):
        d.arc((P(0.1, 0.1), P(0.9, 0.9)), start, start + 70, fill=LIGHT, width=int(0.045 * S))
        a = math.radians(start + 70)
        tip = (0.5 + 0.4 * math.cos(a), 0.5 + 0.4 * math.sin(a))
        poly(d, rotate([(tip[0], tip[1] - 0.06), (tip[0] + 0.08, tip[1]), (tip[0], tip[1] + 0.06)], a + math.pi / 2, tip[0], tip[1]), LIGHT)


def cri_de_guerre(d, accent):
    # Bouche qui crie et ondes sonores.
    poly(d, [(0.12, 0.38), (0.36, 0.3), (0.36, 0.7), (0.12, 0.62)], LIGHT)
    d.ellipse((P(0.2, 0.42), P(0.32, 0.58)), fill=accent)
    for k, r in enumerate((0.16, 0.27, 0.38)):
        d.arc((P(0.36 - r, 0.5 - r), P(0.36 + r, 0.5 + r)), -45, 45, fill=LIGHT if k != 1 else accent, width=int(0.045 * S))


def pluie_de_fleches(d, accent):
    for x0, y0 in ((0.12, 0.08), (0.36, 0.04), (0.6, 0.08), (0.24, 0.3), (0.5, 0.28)):
        arrow(d, x0, y0, x0 + 0.2, y0 + 0.42, width=0.035, head=0.1, fletching=False)
    line(d, [(0.1, 0.88), (0.9, 0.88)], 0.03, accent)


def piege(d, accent):
    # Mâchoires dentées d'un piège à loup, vues de face.
    d.arc((P(0.12, 0.3), P(0.88, 0.95)), 180, 360, fill=LIGHT, width=int(0.06 * S))
    d.arc((P(0.12, 0.05), P(0.88, 0.7)), 0, 180, fill=LIGHT, width=int(0.06 * S))
    for i in range(5):
        x = 0.24 + i * 0.13
        poly(d, [(x - 0.05, 0.6), (x + 0.05, 0.6), (x, 0.45)], LIGHT)
        poly(d, [(x - 0.05, 0.38), (x + 0.05, 0.38), (x, 0.53)], accent)
    circle(d, 0.5, 0.88, 0.05, fill=accent)


def vague_de_flammes(d, accent):
    # Trois flammes de plus en plus hautes, en ligne.
    for i, (x, h) in enumerate(((0.22, 0.32), (0.48, 0.46), (0.74, 0.6))):
        base = 0.84
        flame = [(x - 0.11, base), (x - 0.08, base - h * 0.55), (x - 0.02, base - h * 0.7), (x, base - h), (x + 0.04, base - h * 0.62),
                 (x + 0.1, base - h * 0.5), (x + 0.11, base)]
        poly(d, flame, (255, 150 - i * 20, 40, 255))
        poly(d, [(x - 0.05, base), (x, base - h * 0.5), (x + 0.05, base)], LIGHT)


def prison_de_glace(d, accent):
    # Cristaux de glace autour d'un cœur.
    for x, h in ((0.2, 0.5), (0.35, 0.68), (0.5, 0.78), (0.65, 0.68), (0.8, 0.5)):
        poly(d, [(x - 0.08, 0.86), (x, 0.86 - h), (x + 0.08, 0.86)], LIGHT)
        line(d, [(x, 0.86), (x, 0.9 - h)], 0.015, accent)
    line(d, [(0.1, 0.87), (0.9, 0.87)], 0.03, accent)


def barriere(d, accent):
    # Bouclier frappé d'une ancre (inamovible).
    poly(d, shield_points(0.5, 0.52, 0.6, 0.74), LIGHT)
    poly(d, shield_points(0.5, 0.52, 0.46, 0.6), accent)
    line(d, [(0.5, 0.3), (0.5, 0.68)], 0.05, LIGHT)
    line(d, [(0.4, 0.38), (0.6, 0.38)], 0.04, LIGHT)
    d.arc((P(0.36, 0.48), P(0.64, 0.74)), 0, 180, fill=LIGHT, width=int(0.045 * S))
    circle(d, 0.5, 0.27, 0.04, outline=LIGHT, width=0.025)


def lien_de_vie(d, accent):
    # Deux cœurs reliés par un lien.
    for cx, cy, r, color in ((0.32, 0.4, 0.14, LIGHT), (0.68, 0.62, 0.14, (120, 220, 110, 255))):
        circle(d, cx - r * 0.5, cy, r * 0.6, fill=color)
        circle(d, cx + r * 0.5, cy, r * 0.6, fill=color)
        poly(d, [(cx - r * 1.05, cy + r * 0.15), (cx + r * 1.05, cy + r * 0.15), (cx, cy + r * 1.3)], color)
    line(d, [(0.4, 0.55), (0.6, 0.48)], 0.03, accent)
    for t in (0.3, 0.7):
        circle(d, 0.4 + 0.2 * t, 0.55 - 0.07 * t, 0.035, outline=accent, width=0.02)


def eboulis(d, accent):
    # Rochers qui tombent sur un tas.
    poly(d, [(0.14, 0.86), (0.2, 0.66), (0.36, 0.56), (0.52, 0.6), (0.6, 0.74), (0.56, 0.86)], LIGHT)
    poly(d, [(0.46, 0.86), (0.52, 0.7), (0.68, 0.62), (0.84, 0.7), (0.88, 0.86)], LIGHT)
    poly(d, [(0.4, 0.38), (0.48, 0.24), (0.62, 0.22), (0.7, 0.34), (0.62, 0.46), (0.46, 0.48)], accent)
    for x in (0.4, 0.55, 0.7):
        line(d, [(x - 0.04, 0.06), (x, 0.16)], 0.025, LIGHT)
    line(d, [(0.08, 0.9), (0.92, 0.9)], 0.03, accent)


def palissade(d, accent):
    # Trois pieux taillés en pointe, reliés par une traverse.
    for x in (0.26, 0.5, 0.74):
        poly(d, [(x - 0.08, 0.88), (x - 0.08, 0.34), (x, 0.16), (x + 0.08, 0.34), (x + 0.08, 0.88)], LIGHT)
        line(d, [(x - 0.03, 0.4), (x - 0.03, 0.8)], 0.012, accent)
    line(d, [(0.14, 0.6), (0.86, 0.6)], 0.045, accent)


def mur_de_glace(d, accent):
    # Mur de blocs de glace, éclats au sommet.
    for row, (y0, y1) in enumerate(((0.62, 0.86), (0.38, 0.62))):
        offset = 0.0 if row == 0 else 0.11
        x = 0.1 + offset
        while x < 0.86:
            right = min(x + 0.22, 0.9)
            poly(d, [(x + 0.01, y0 + 0.01), (right - 0.01, y0 + 0.01), (right - 0.01, y1 - 0.01), (x + 0.01, y1 - 0.01)], LIGHT)
            x = right
    for x in (0.26, 0.5, 0.74):
        poly(d, [(x - 0.07, 0.38), (x, 0.14), (x + 0.07, 0.38)], accent)


def voile_sacre(d, accent):
    # Voile de lumière ondulé sous une étoile.
    for x in (0.24, 0.4, 0.56, 0.72):
        pts = [(x + 0.035 * math.sin(t * math.pi * 2), 0.3 + 0.6 * t) for t in [i / 12 for i in range(13)]]
        line(d, pts, 0.07, LIGHT)
    poly(d, star(0.5, 0.17, 0.13, 0.04), accent)


SYMBOLS = {
    'eboulis': eboulis, 'palissade': palissade, 'mur_de_glace': mur_de_glace, 'voile_sacre': voile_sacre,
    'taillade': taillade, 'charge': charge, 'rempart': rempart, 'provocation': provocation,
    'tir_precis': tir_precis, 'fleche_empoisonnee': fleche_empoisonnee, 'fleche_recul': fleche_recul,
    'fleche_entravante': fleche_entravante, 'eclair': eclair, 'boule_de_feu': boule_de_feu,
    'glyphe_givre': glyphe_givre, 'transposition': transposition, 'chatiment': chatiment, 'soin': soin,
    'bouclier_sacre': bouclier_sacre, 'purification': purification,
    'tourbillon': tourbillon, 'cri_de_guerre': cri_de_guerre, 'pluie_de_fleches': pluie_de_fleches, 'piege': piege,
    'vague_de_flammes': vague_de_flammes, 'prison_de_glace': prison_de_glace, 'barriere': barriere, 'lien_de_vie': lien_de_vie,
}


def make_icon(spell_id, class_name):
    top, bottom = CLASS_COLORS.get(class_name, ((120, 120, 120), (30, 30, 30)))
    icon = Image.new('RGBA', (S, S))
    gradient = ImageDraw.Draw(icon)
    for y in range(S):
        t = y / (S - 1)
        color = tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)) + (255,)
        gradient.line([(0, y), (S, y)], fill=color)
    # Halo central.
    glow = Image.new('L', (S, S), 0)
    ImageDraw.Draw(glow).ellipse(P(0.15, 0.15) + P(0.85, 0.85), fill=90)
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.12))
    icon.paste(Image.new('RGBA', (S, S), (255, 255, 255, 255)), (0, 0), Image.eval(glow, lambda v: v // 2))

    symbol = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    accent = tuple(min(255, int(c * 1.25 + 40)) for c in top) + (255,)
    SYMBOLS[spell_id](ImageDraw.Draw(symbol), accent)

    alpha = symbol.getchannel('A')
    outline = alpha.filter(ImageFilter.MaxFilter(int(S * 0.035) | 1))
    shadow = outline.filter(ImageFilter.GaussianBlur(S * 0.02))
    icon.paste(Image.new('RGBA', (S, S), (0, 0, 0, 255)), (int(S * 0.015), int(S * 0.025)), Image.eval(shadow, lambda v: v * 3 // 5))
    icon.paste(Image.new('RGBA', (S, S), DARK), (0, 0), outline)
    icon.alpha_composite(symbol)

    # Cadre.
    frame = ImageDraw.Draw(icon)
    frame.rectangle((0, 0, S - 1, S - 1), outline=(15, 10, 8, 255), width=int(S * 0.025))
    frame.rectangle((int(S * 0.025), int(S * 0.025), S - 1 - int(S * 0.025), S - 1 - int(S * 0.025)),
                    outline=tuple(min(255, c + 70) for c in top) + (255,), width=int(S * 0.012))
    return icon.resize((SIZE, SIZE), Image.LANCZOS)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--preview', action='store_true')
    args = parser.parse_args()

    text = open(GAMEDATA, encoding='utf-8-sig').read()
    data = json.loads(text)
    icons = []
    for cls in data['classes']:
        for spell in cls['spells']:
            if spell['id'] not in SYMBOLS:
                print('Pas de symbole pour', spell['id'])
                continue
            icon = make_icon(spell['id'], cls['name'])
            path = 'assets/spellicons/%s.png' % spell['id']
            icon.convert('RGB').save(os.path.join(ROOT, path), optimize=True)
            icons.append(icon)
            # Mise à jour ciblée du fichier (sa mise en forme est conservée).
            pattern = r'("id":\s*"%s"[^{}]*?"icon":\s*")[^"]*(")' % re.escape(spell['id'])
            text, count = re.subn(pattern, r'\g<1>./' + path + r'\g<2>', text, count=1)
            if count != 1:
                print('Icône non mise à jour dans gamedata.json :', spell['id'])
    open(GAMEDATA, 'w', encoding='utf-8', newline='\n').write(text)
    print('%d icônes générées' % len(icons))

    if args.preview:
        # Une ligne par classe.
        per_class = max(len(cls['spells']) for cls in data['classes'])
        sheet = Image.new('RGB', (per_class * 110, len(data['classes']) * 110), (30, 30, 40))
        for i, icon in enumerate(icons):
            sheet.paste(icon.convert('RGB'), ((i % per_class) * 110 + 5, (i // per_class) * 110 + 5))
        sheet.save(os.path.join(os.path.dirname(__file__), 'preview.png'))


if __name__ == '__main__':
    main()
