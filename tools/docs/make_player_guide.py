"""Guide du joueur imprimable : assets/web/guide.html (une feuille A4 recto-verso).

Usage (depuis la racine du dépôt) : py tools/docs/make_player_guide.py

Le recto explique le jeu (but, tour, commandes, cases spéciales, signaux, tournoi, talents) ; le verso
présente les 4 classes (passif et 6 sorts), les combinaisons entre classes et des astuces. Les chiffres
viennent des données du jeu (assets/data/gamedata.json, assets/tiles/tileset.json) : relancer le script
après une modification des règles, des classes, des sorts ou des talents. Le guide garde l'empreinte de
gamedata.json, et un test (TacticalWarTests, GuideTests.cpp) échoue s'il n'a pas été régénéré.

Le serveur sert la page sur http://<serveur>:8080/guide.html ; les icônes y sont incluses.
"""
import base64
import html
import io
import json
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
GAMEDATA = os.path.join(ROOT, 'assets', 'data', 'gamedata.json')
TILESET = os.path.join(ROOT, 'assets', 'tiles', 'tileset.json')
TARGET = os.path.join(ROOT, 'assets', 'web', 'guide.html')

CLASS_COLORS = {'guerrier': '#c0392b', 'archer': '#2e8b57', 'mage': '#3f5fbf', 'protecteur': '#b8860b'}

# Mise en pratique de chaque combinaison (par nom de combinaison) : l'ordre des sorts qui marche.
COMBO_PRACTICE = {
    'Brise-glace': "La Prison gèle tout de suite, le Glyphe au début du tour de l'ennemi. Gardez la "
                   "Taillade pour la cible gelée.",
    'Cible immobile': "La cible entravée est aussi ralentie ; le Mage enchaîne jusqu'à trois Éclair renforcés.",
    'Dans le mille': "La Provocation attire l'ennemi ; l'Archer tire deux fois, de loin (Tireur d'élite).",
    'Jugement ardent': "La Boule de feu touche aussi les alliés : près d'eux, préférez la Vague de flammes. "
                       "Chaque Châtiment soigne le Protecteur.",
}


def esc(text):
    return html.escape(str(text), quote=True)


def fnv1a(data):
    """Empreinte de gamedata.json (sans BOM ni retours chariot), recalculée par le test du guide."""
    value = 0x811C9DC5
    for byte in data:
        value ^= byte
        value = (value * 0x01000193) & 0xFFFFFFFF
    return '%08x' % value


def gamedata_hash():
    data = open(GAMEDATA, 'rb').read()
    if data.startswith(b'\xef\xbb\xbf'):
        data = data[3:]
    return fnv1a(data.replace(b'\r', b''))


def image_uri(path, size):
    """Icône incluse dans la page (data URI), réduite si Pillow est disponible."""
    full = os.path.join(ROOT, path[2:] if path.startswith('./') else path)
    try:
        from PIL import Image
        image = Image.open(full).convert('RGBA')
        image.thumbnail((size, size), Image.LANCZOS)
        buffer = io.BytesIO()
        image.save(buffer, 'PNG', optimize=True)
        data = buffer.getvalue()
    except ImportError:
        data = open(full, 'rb').read()
    return 'data:image/png;base64,' + base64.b64encode(data).decode('ascii')


# --- Sorts -------------------------------------------------------------------------------------

def other_class_pattern(own, classes):
    names = [c['name'] for c in classes if c['name'] != own]
    return re.compile(r"(du|de l'|de la|le|la|l')\s?(%s)\b" % '|'.join(map(re.escape, names)))


def short_description(spell, own, classes):
    """Description du sort sans ce qui parle des combinaisons (elles ont leur propre tableau)."""
    other = other_class_pattern(own, classes)
    text = re.sub(r'\s*\([^)]*\)', lambda m: '' if other.search(m.group(0)) else m.group(0), spell['description'])
    sentences = re.split(r'(?<=[.!])\s+(?=[A-ZÉÈÀ+])', text)
    kept = [s for s in sentences if not s.startswith('Combo') and not other.search(s)]
    return ' '.join(kept)


def spell_range(spell):
    if spell.get('launch') == 'SELF':
        zone = spell.get('zone')
        return 'sur soi' if not zone else 'autour de soi (%d)' % zone.get('size', 0)
    low, high = spell.get('range', [0, 0])
    text = 'portée %d' % high if low in (0, high) else 'portée %d-%d' % (low, high)
    if low == 0:
        text += ', ou soi'
    if spell.get('launch') == 'LINE':
        text += ' en ligne'
    if spell.get('lineOfSight') is False:
        text += ', sans ligne de vue'
    return text


def spell_value(spell):
    for effect in spell['effects']:
        if effect.get('type') in ('DAMAGE', 'LIFESTEAL'):
            return 'dégâts %d-%d' % (effect['min'], effect['max'])
        if effect.get('type') == 'HEAL':
            return 'soin %d-%d' % (effect['min'], effect['max'])
    return None


def all_effects(spell):
    for effect in spell['effects']:
        yield effect
        for inner in effect.get('glyph', {}).get('effects', []):
            yield inner


def spell_row(spell, cls, classes):
    chips = ['%d PA' % spell['ap'], spell_range(spell)]
    if spell.get('castsPerTurn'):
        chips.append('%d×/tour' % spell['castsPerTurn'])
    if spell.get('cooldown'):
        chips.append('relance %d' % spell['cooldown'])
    value = spell_value(spell)
    if value:
        chips.append(value)
    tags = []
    for effect in all_effects(spell):
        if effect.get('type') == 'STATE' and effect.get('negative'):
            tags.append('<span class="tag mark">marque %s</span>' % esc(effect['name']))
        if effect.get('combo'):
            combo = effect['combo']
            tags.append('<span class="tag combo">★ %s +%d %%</span>' % (esc(combo['name']), combo['percent']))
    return ('<li><img src="%s" alt=""><div><b>%s</b> <span class="chips">%s</span><br>%s %s</div></li>'
            % (image_uri(spell['icon'], 40), esc(spell['name']), esc(' · '.join(chips)),
               esc(short_description(spell, cls['name'], classes)), ''.join(dict.fromkeys(tags))))


def class_role(cls):
    """Rôle de la classe : début de sa description (« Combattant de mêlée robuste »)."""
    return re.split(r'[.:]', cls['description'])[0].strip()


def class_card(cls, classes):
    stats = cls['stats']
    chips = ['%d PV' % stats['MAX_HP'], '%d PA' % stats['AP'], '%d PM' % stats['MP'],
             'initiative %d' % stats['INITIATIVE']]
    if stats.get('RESISTANCE'):
        chips.append('résistance %d %%' % stats['RESISTANCE'])
    if stats.get('POWER'):
        chips.append('puissance +%d %%' % stats['POWER'])
    passive = cls['passive']
    spells = ''.join(spell_row(spell, cls, classes) for spell in cls['spells'])
    return """<section class="class-card" style="--class: %s">
  <header><img src="%s" alt=""><div><h3>%s <span class="role">%s</span></h3><div class="stats">%s</div></div></header>
  <p class="passive"><b>Passif « %s »</b> : %s</p>
  <ul class="spells">%s</ul>
</section>""" % (CLASS_COLORS.get(cls['key'], '#555'), image_uri(cls['icon'], 64), esc(cls['name']),
                 esc(class_role(cls)), esc(' · '.join(chips)), esc(passive['name']), esc(passive['description']), spells)


# --- Combinaisons ------------------------------------------------------------------------------

def join_names(names):
    names = list(dict.fromkeys(names))
    return names[0] if len(names) == 1 else ', '.join(names[:-1]) + ' ou ' + names[-1]


def combos(classes):
    """Pour chaque combinaison : la marque, les sorts qui la posent et ceux qui en profitent."""
    setters, marks = {}, {}
    for cls in classes:
        for spell in cls['spells']:
            for effect in all_effects(spell):
                if effect.get('type') == 'STATE' and effect.get('negative'):
                    setters.setdefault(effect['state'], []).append((cls['name'], spell['name']))
                    marks[effect['state']] = effect['name']
    found = {}
    for cls in classes:
        for spell in cls['spells']:
            for effect in spell['effects']:
                combo = effect.get('combo')
                if combo:
                    entry = found.setdefault(combo['name'], {'combo': combo, 'finishers': []})
                    entry['finishers'].append((cls['name'], spell['name']))
    rows = []
    for name, entry in found.items():
        combo = entry['combo']
        by = setters.get(combo['state'], [])
        rows.append({
            'name': name, 'percent': combo['percent'], 'consumes': combo.get('consumes', False),
            'mark': marks.get(combo['state'], combo['state']),
            'setter_class': by[0][0] if by else '?', 'setters': join_names(s for _, s in by),
            'finisher_class': entry['finishers'][0][0], 'finishers': join_names(s for _, s in entry['finishers']),
        })
    return rows


def combo_section(classes):
    items = []
    for row in combos(classes):
        practice = COMBO_PRACTICE.get(row['name'], '')
        end = ' : la cible dégèle' if row['consumes'] else ''
        items.append("""<li><div class="combo-head"><b>%s</b> <span class="bonus">+%d %%</span></div>
  <div><span class="who">%s</span> %s <span class="arrow">➜</span> marque <b>%s</b> <span class="arrow">➜</span>
  <span class="who">%s</span> %s%s.</div>
  <div class="practice">%s</div></li>""" % (
            esc(row['name']), row['percent'], esc(row['setter_class']), esc(row['setters']), esc(row['mark']),
            esc(row['finisher_class']), esc(row['finishers']), esc(end), esc(practice)))
    return '<ul class="combos">%s</ul>' % ''.join(items)


# --- Recto -------------------------------------------------------------------------------------

def special_tiles():
    tiles = json.load(open(TILESET, encoding='utf-8-sig'))['tiles']
    rows = []
    for tile in tiles:
        if tile.get('group') != 'Cases spéciales':
            continue
        turn = tile.get('turnStart', {})
        if turn.get('damage'):
            rule = '%d dégâts au début du tour de qui s\'y trouve' % turn['damage']
        elif turn.get('heal'):
            rule = '+%d PV au début du tour de qui s\'y trouve' % turn['heal']
        elif tile.get('blocksLineOfSight'):
            rule = 'on y marche, mais elles bloquent la ligne de vue : on s\'y cache des tirs'
        else:
            continue
        rows.append('<tr><th>%s</th><td>%s</td></tr>' % (esc(tile['name']), esc(rule)))
    return '<table class="tiles">%s</table>' % ''.join(rows)


def talents_table(data):
    return '<ul class="talents">%s</ul>' % ''.join(
        '<li><b>%s</b> %s</li>' % (esc(t['name']), esc(t['description'])) for t in data['talents'])


def protector_aura(classes):
    for cls in classes:
        passive = cls['passive']
        if passive.get('type') == 'ALLY_AURA':
            return cls['name'], passive
    return None, None


def build():
    data = json.load(open(GAMEDATA, encoding='utf-8-sig'))
    rules = data['rules']
    classes = data['classes']
    aura_class, aura = protector_aura(classes)
    aura_tip = ('<b>Restez groupés</b> près du %s (aura : +%d %% de résistance à %d cases), mais hors de la croix '
                'd\'une Boule de feu !' % (esc(aura_class), aura['bonus'], aura['distance'])) if aura else ''
    values = {
        'hash': gamedata_hash(),
        'turn': rules['turnSeconds'], 'bank': rules['timeBankSeconds'], 'placement': rules['placementSeconds'],
        'sudden': rules['suddenDeathRound'], 'sudden_pct': rules['suddenDeathPercentPerRound'],
        'max_rounds': rules['maxRounds'], 'max_res': rules['maxResistance'],
        'collision': rules['collisionDamagePerCell'], 'collision_hit': rules['collisionDamageToHit'],
        'class_count': len(classes), 'spell_count': len(classes[0]['spells']),
        'tiles': special_tiles(), 'talents': talents_table(data),
        'classes': ''.join(class_card(cls, classes) for cls in classes),
        'combos': combo_section(classes), 'aura_tip': aura_tip,
    }
    return PAGE % values


PAGE = """<!doctype html>
<html lang="fr">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="gamedata-hash" content="%(hash)s">
  <title>Tactical War - Guide du joueur</title>
  <!-- Fichier généré par py tools/docs/make_player_guide.py : ne pas le modifier à la main. -->
  <style>
    @page { size: A4 portrait; margin: 9mm; }
    :root { --gold: #b8860b; --ink: #1d2233; --soft: #f4efe0; }
    * { box-sizing: border-box; }
    body { margin: 0; background: #3a3f52; font-family: "Segoe UI", Arial, sans-serif; color: var(--ink); font-size: 8.6pt; line-height: 1.32; }
    .toolbar { position: sticky; top: 0; z-index: 1; display: flex; justify-content: space-between; align-items: center;
      padding: 0.6rem 1rem; background: #1d2233; color: #fff; font-size: 11pt; }
    .toolbar button { font-size: 11pt; padding: 0.35rem 1.1rem; border: 0; border-radius: 6px; background: #ffd34d; cursor: pointer; }
    .sheet { width: 210mm; height: 297mm; margin: 1rem auto; padding: 9mm; background: #fff; overflow: hidden;
      break-after: page; page-break-after: always; }
    .sheet:last-child { break-after: auto; page-break-after: auto; }
    h1 { margin: 0; font-size: 21pt; letter-spacing: 0.06em; }
    h2 { margin: 0 0 1.2mm; font-size: 11pt; color: var(--gold); border-bottom: 1.5px solid var(--gold); padding-bottom: 0.4mm; }
    h3 { margin: 0; font-size: 12pt; color: var(--class); }
    p { margin: 0 0 1mm; }
    ul { margin: 0 0 1mm; padding-left: 4mm; }
    li { margin: 0 0 0.6mm; }
    .banner { display: flex; justify-content: space-between; align-items: flex-end; border-bottom: 3px double var(--gold);
      padding-bottom: 1.5mm; margin-bottom: 3mm; }
    .banner.small { margin-bottom: 2mm; }
    .banner.small h1 { font-size: 15pt; }
    .banner .brand { white-space: nowrap; }
    .banner .sub { font-size: 10pt; opacity: 0.8; }
    .banner .brand { color: var(--gold); }
    .cols { display: grid; grid-template-columns: 1fr 1fr; gap: 3mm 5mm; }
    .box { margin-bottom: 2.6mm; }
    .keys { width: 100%%; border-collapse: collapse; }
    .keys th { text-align: left; white-space: nowrap; padding: 0.5mm 2mm 0.5mm 0; vertical-align: top; }
    .keys td { padding: 0.5mm 0; }
    .keys tr + tr { border-top: 1px solid #eee; }
    kbd { display: inline-block; border: 1px solid #999; border-bottom-width: 2px; border-radius: 3px; padding: 0 1.2mm;
      font-family: inherit; font-size: 8pt; background: #fafafa; }
    .tiles { width: 100%%; border-collapse: collapse; }
    .tiles th { text-align: left; white-space: nowrap; padding: 0.5mm 2mm 0.5mm 0; }
    .talents { columns: 2; column-gap: 4mm; list-style: none; padding: 0; }
    .talents li { break-inside: avoid; }
    .note { background: var(--soft); border-left: 3px solid var(--gold); padding: 1.2mm 2mm; }
    .classes { display: grid; grid-template-columns: 1fr 1fr; gap: 2mm; }
    .class-card { border: 1.5px solid var(--class); border-radius: 2mm; padding: 1.6mm 2.2mm; }
    .class-card header { display: flex; gap: 2mm; align-items: center; }
    .class-card header img { width: 11mm; height: 11mm; object-fit: contain; }
    .class-card .stats { font-size: 8pt; opacity: 0.85; }
    .class-card .role { font-size: 8.4pt; font-weight: 400; font-style: italic; color: var(--ink); }
    .class-card .passive { margin: 0.6mm 0 0.8mm; font-size: 8.1pt; line-height: 1.22; }
    .spells { list-style: none; padding: 0; margin: 0; }
    .spells li { display: flex; gap: 1.6mm; align-items: flex-start; margin-bottom: 0.6mm; font-size: 7.7pt; line-height: 1.17; }
    .spells img { width: 6mm; height: 6mm; border-radius: 1mm; flex: none; margin-top: 0.3mm; }
    .chips { color: #555; font-size: 7.1pt; }
    .tag { display: inline-block; font-size: 7.2pt; border-radius: 1mm; padding: 0 1mm; margin-left: 0.6mm; }
    .tag.mark { border: 1px solid var(--gold); color: #7a5a00; }
    .tag.combo { background: var(--gold); color: #fff; }
    .combos { list-style: none; padding: 0; display: grid; grid-template-columns: 1fr 1fr; gap: 1.6mm 4mm; }
    .combos li { border-left: 3px solid var(--gold); padding-left: 2mm; }
    .combo-head .bonus { color: var(--gold); font-weight: 700; }
    .combos .who { font-weight: 600; }
    .combos .arrow { color: var(--gold); }
    .combos .practice { font-size: 8pt; opacity: 0.85; }
    .tips { margin: 0; padding-left: 5mm; columns: 2; column-gap: 6mm; }
    .tips li { break-inside: avoid; }
    @media print {
      body { background: #fff; }
      .toolbar { display: none; }
      .sheet { margin: 0; width: auto; height: 279mm; padding: 0; }
    }
  </style>
</head>
<body>
<div class="toolbar"><span>Guide du joueur : une feuille A4 recto-verso</span><button onclick="window.print()">Imprimer</button></div>

<article class="sheet">
  <div class="banner">
    <div><h1><span class="brand">Tactical War</span> · Guide du joueur</h1>
      <div class="sub">2 contre 2, tour par tour, %(class_count)s classes. À lire avant le tournoi, et à garder à côté du clavier.</div></div>
    <div class="sub">Au dos : les classes et les combinaisons</div>
  </div>
  <div class="cols">
    <div>
      <section class="box"><h2>Le but du jeu</h2>
        <p>Deux équipes de deux joueurs s'affrontent sur une carte en cases. Chaque joueur dirige un personnage.</p>
        <ul>
          <li><b>Victoire</b> : mettre les deux adversaires hors combat (KO).</li>
          <li><b>Zone à tenir</b> (si l'organisateur la choisit) : à la fin de chaque tour complet, une équipe marque
            1 point si elle est seule dans la zone dorée. La première au score annoncé gagne ; un KO des deux
            adversaires fait toujours gagner.</li>
          <li><b>Mort subite</b> : à partir du tour %(sudden)s, chacun perd %(sudden_pct)s %% de ses PV max au début de
            son tour, puis un peu plus à chaque tour. Au tour %(max_rounds)s, l'équipe qui a gardé la plus grande part de
            ses PV gagne.</li>
        </ul>
      </section>
      <section class="box"><h2>Votre tour</h2>
        <ul>
          <li><b>Placement</b> : choisissez votre case de départ, puis « Prêt » (%(placement)s s).</li>
          <li><b>Ordre</b> : selon l'initiative ; la frise en haut de l'écran montre qui joue après qui.</li>
          <li><b>%(turn)s secondes</b> par tour, plus une <b>réserve de %(bank)s s</b> pour tout le combat (minuteur orange).</li>
          <li><b>PA</b> (étoile jaune) : lancer des sorts. <b>PM</b> (carré vert) : 1 PM par case. Ils reviennent à
            chaque tour : dépensez-les !</li>
          <li><b>Relance</b> : un sort grisé avec un chiffre revient dans ce nombre de tours. D'autres sorts se lancent
            plusieurs fois par tour.</li>
          <li>« Passer le tour » quand il n'y a plus rien à faire.</li>
        </ul>
      </section>
      <section class="box"><h2>Les commandes</h2>
        <table class="keys">
          <tr><th>Clic sur une case verte</th><td>se déplacer</td></tr>
          <tr><th><kbd>1</kbd> à <kbd>4</kbd> ou clic sur l'icône</th><td>choisir un sort, puis clic sur une case bleue pour le lancer ; <kbd>Échap</kbd> annule</td></tr>
          <tr><th>Survol d'un personnage</th><td>ses détails, et où il pourra aller à son prochain tour</td></tr>
          <tr><th><kbd>Alt</kbd> + clic</th><td>roue des signaux ; clic molette : signal « Ici »</td></tr>
          <tr><th><kbd>F1</kbd> à <kbd>F6</kbd></th><td>émotes</td></tr>
          <tr><th>Molette, clic droit maintenu</th><td>zoom, déplacer la vue ; <kbd>F</kbd> suit le personnage actif, <kbd>C</kbd> recentre</td></tr>
          <tr><th><kbd>H</kbd> ou bouton « ? »</th><td>aide des commandes, en plein combat</td></tr>
        </table>
      </section>
      <section class="box"><h2>À savoir</h2>
        <ul>
          <li><b>Aperçu</b> : pendant la visée, les dégâts prévus s'affichent sur chaque cible, avec « KO ! » si elle tombe.</li>
          <li><b>Bouclier</b> (écusson bleu) : il absorbe les dégâts avant les PV.</li>
          <li><b>Résistance</b> : réduit les dégâts reçus, de %(max_res)s %% au plus.</li>
          <li><b>Tacle</b> : quitter le contact d'un ennemi peut coûter des PM et des PA ; plus son tacle dépasse
            votre fuite, plus vous perdez.</li>
          <li><b>Ligne de vue</b> : rochers, arbres, hautes herbes et personnages bloquent la plupart des sorts à distance.</li>
          <li><b>Collision</b> : un personnage poussé contre un obstacle subit %(collision)s dégâts par case non
            parcourue ; s'il heurte un personnage, celui-ci en subit %(collision_hit)s.</li>
        </ul>
      </section>
    </div>
    <div>
      <section class="box"><h2>Les cases spéciales</h2>
        %(tiles)s
        <p>L'effet s'applique au début du tour (pas en traversant). Survolez une case : la ligne d'aide donne sa règle.</p>
      </section>
      <section class="box"><h2>Jouer avec son coéquipier</h2>
        <ul>
          <li><b>Signaux</b> : <kbd>Alt</kbd> + clic sur une case, puis Ici, Attaquez, Repli ou Danger. Seul votre coéquipier les voit.</li>
          <li><b>Émotes</b> : « Bien joué ! », « Merci ! », « GG »… (<kbd>F1</kbd> à <kbd>F6</kbd>), visibles de tous.</li>
          <li><b>Parlez-vous</b> : choisissez ensemble la cible, et qui pose la marque d'une combinaison (au dos).</li>
        </ul>
      </section>
      <section class="box"><h2>Le tournoi</h2>
        <p><b>Avant chaque match</b>, sur l'écran de choix de classe :</p>
        <ul>
          <li>choisissez votre <b>classe</b> : le bloc « Votre coéquipier » montre la sienne et vos combinaisons possibles ;</li>
          <li>emportez <b>4 sorts sur les %(spell_count)s</b> de la classe ;</li>
          <li>choisissez vos <b>talents</b> : un par match déjà joué par votre équipe (3 au plus) ;</li>
          <li>parfois, un <b>bannissement</b> d'abord : chaque équipe interdit une classe à l'autre ;</li>
          <li>puis « Verrouiller mon choix ».</li>
        </ul>
        <ul>
          <li><b>Seul dans l'équipe, ou coéquipier absent</b> : vous jouez les deux personnages (vous choisissez
            aussi la classe du second). Un coéquipier qui revient reprend la main.</li>
          <li><b>Déconnecté ?</b> Reconnectez-vous avec les mêmes identifiants : vous retrouvez votre combat.</li>
          <li><b>Après le combat</b> : le bilan désigne le <b>MVP</b> et décerne des <b>hauts faits</b> (Premier
            sang, Coup double, Maître des combos…). Ils figurent sur votre diplôme !</li>
        </ul>
      </section>
      <section class="box"><h2>Les talents</h2>
        %(talents)s
      </section>
      <section class="box"><h2>S'entraîner</h2>
        <p class="note">Depuis l'écran de connexion, sans serveur : le <b>Tutoriel</b> (les bases en quelques
          minutes), l'<b>Entraînement</b> contre l'ordinateur (1 contre 1, 2 contre 2, ou en jouant les deux
          personnages) et les <b>Énigmes</b> (6 défis tactiques à résoudre en un tour).</p>
      </section>
    </div>
  </div>
</article>

<article class="sheet">
  <div class="banner small">
    <div><h1>Les classes</h1><div class="sub">4 sorts sur %(spell_count)s par match · ★ : bonus sur une cible marquée
      par un coéquipier</div></div>
    <div class="sub brand">Tactical War</div>
  </div>
  <div class="classes">%(classes)s</div>
  <section class="box" style="margin-top: 2mm"><h2>Les combinaisons : la marque d'abord, le coup ensuite</h2>
    %(combos)s
  </section>
  <section class="box"><h2>Astuces</h2>
    <ol class="tips">
      <li><b>Regardez la frise des tours</b> : le premier des deux pose la marque, l'autre en profite.</li>
      <li><b>Visez la même cible</b>, désignée avec le signal « Attaquez » : à deux, elle tombe vite.</li>
      <li>%(aura_tip)s</li>
      <li><b>Survolez les ennemis</b> : la zone orange montre où ils pourront aller. Cachez-vous derrière un rocher ou dans les hautes herbes.</li>
      <li><b>Ne gaspillez rien</b> : 2 PA restants ? Un petit sort. Gardez la réserve de temps pour les tours décisifs.</li>
    </ol>
  </section>
</article>
</body>
</html>
"""


def main():
    page = build()
    with open(TARGET, 'w', encoding='utf-8', newline='\n') as out:
        out.write(page)
    print('Guide écrit :', os.path.relpath(TARGET, ROOT), '(%d Ko)' % (len(page.encode('utf-8')) // 1024))


if __name__ == '__main__':
    main()
