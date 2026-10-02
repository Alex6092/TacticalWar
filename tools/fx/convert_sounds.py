"""Convertit les sons de sorts retenus (MP3 du projet Exode) en OGG Vorbis pour le jeu.

SFML 2.5 ne lit pas le MP3. Prérequis : py -m pip install miniaudio soundfile

Usage (depuis la racine du dépôt) :
    py tools/fx/convert_sounds.py                  # sons de tools/fx/selection.json
    py tools/fx/convert_sounds.py --source D:/exode/spell

Les sons sont écrits en mono 44,1 kHz dans assets/sound/spells/<nom>.ogg. Le silence en début
et en fin de fichier est retiré, la durée est limitée à 2,5 s (fondu de sortie) et le niveau est
normalisé (crête à -1 dB).
"""
import argparse
import json
import os
import sys

import miniaudio
import numpy as np
import soundfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HERE = os.path.dirname(os.path.abspath(__file__))
OUTPUT = os.path.join(ROOT, 'assets', 'sound', 'spells')
RATE = 44100
SILENCE = 0.01		# Amplitude (sur 1) sous laquelle un échantillon est considéré comme du silence
PEAK = 10 ** (-1 / 20)
MAX_SECONDS = 2.5
FADE_SECONDS = 0.4


def convert(source_path, target_path):
    decoded = miniaudio.decode_file(source_path, output_format=miniaudio.SampleFormat.FLOAT32, nchannels=1, sample_rate=RATE)
    samples = np.frombuffer(decoded.samples, dtype=np.float32).copy()
    loud = np.nonzero(np.abs(samples) > SILENCE)[0]
    if len(loud):
        # Garde 10 ms avant le premier son et 50 ms après le dernier.
        samples = samples[max(0, loud[0] - RATE // 100):min(len(samples), loud[-1] + RATE // 20)]
    if len(samples) > MAX_SECONDS * RATE:
        samples = samples[:int(MAX_SECONDS * RATE)]
        fade = int(FADE_SECONDS * RATE)
        samples[-fade:] *= np.linspace(1.0, 0.0, fade, dtype=np.float32)
    peak = float(np.max(np.abs(samples))) if len(samples) else 0.0
    if peak > 0:
        samples = samples * (PEAK / peak)
    soundfile.write(target_path, samples, RATE, format='OGG', subtype='VORBIS')
    return len(samples) / RATE


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--selection', default=os.path.join(HERE, 'selection.json'))
    parser.add_argument('--source', help='dossier "spell" du projet Exode (contient sound/)')
    args = parser.parse_args()

    selection = json.load(open(args.selection, encoding='utf-8'))
    source = os.path.join(args.source or selection['source'], 'sound')
    os.makedirs(OUTPUT, exist_ok=True)

    for name in selection['sounds']:
        target = os.path.join(OUTPUT, name + '.ogg')
        seconds = convert(os.path.join(source, name + '.mp3'), target)
        print('%-12s %4.1f s  %3d Ko' % (name, seconds, os.path.getsize(target) // 1024))


if __name__ == '__main__':
    sys.exit(main())
