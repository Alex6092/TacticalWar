"""Sons d'interface synthétisés (aucune source externe) : assets/sound/ui/<nom>.ogg.

Usage (depuis la racine du dépôt) :
    py -m pip install numpy soundfile
    py tools/fx/make_ui_sounds.py

- ping.ogg  : signal d'un coéquipier, deux notes brèves ;
- emote.ogg : bulle d'émote, petit « pop » ;
- combo.ogg : combinaison déclenchée, arpège montant et brillant.
"""
import os
import sys

import numpy as np
import soundfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUTPUT = os.path.join(ROOT, 'assets', 'sound', 'ui')
RATE = 44100


def tone(frequency, seconds, decay):
    """Note sinusoïdale avec une attaque douce et une décroissance exponentielle."""
    t = np.arange(int(RATE * seconds)) / RATE
    attack = np.minimum(1.0, t / 0.004)
    return np.sin(2 * np.pi * frequency * t) * attack * np.exp(-t * decay)


def sweep(start, end, seconds, decay):
    """Glissement de fréquence (pour un « pop »)."""
    t = np.arange(int(RATE * seconds)) / RATE
    frequency = start + (end - start) * t / seconds
    phase = 2 * np.pi * np.cumsum(frequency) / RATE
    attack = np.minimum(1.0, t / 0.003)
    return np.sin(phase) * attack * np.exp(-t * decay)


def save(name, samples, peak_db=-6.0):
    peak = np.max(np.abs(samples))
    if peak > 0:
        samples = samples / peak * (10 ** (peak_db / 20))
    os.makedirs(OUTPUT, exist_ok=True)
    path = os.path.join(OUTPUT, name + '.ogg')
    soundfile.write(path, samples.astype(np.float32), RATE, format='OGG', subtype='VORBIS')
    print(os.path.relpath(path, ROOT), '%.2f s' % (len(samples) / RATE))


def main():
    gap = np.zeros(int(RATE * 0.02))
    ping = np.concatenate([tone(880, 0.12, 22) * 0.9, gap, tone(1318.5, 0.28, 12)])
    save('ping', ping)

    pop = sweep(520, 980, 0.09, 30)
    save('emote', np.concatenate([pop, np.zeros(int(RATE * 0.03))]), peak_db=-9.0)

    # Arpège do-mi-sol-do, chaque note tenue sous la suivante, avec une octave pour le brillant.
    notes = [523.25, 659.25, 783.99, 1046.5]
    step = int(RATE * 0.055)
    combo = np.zeros(step * len(notes) + int(RATE * 0.45))
    for index, frequency in enumerate(notes):
        note = tone(frequency, 0.45, 9) + 0.35 * tone(frequency * 2, 0.45, 14)
        combo[index * step:index * step + len(note)] += note
    save('combo', combo, peak_db=-7.0)


if __name__ == '__main__':
    sys.exit(main())
