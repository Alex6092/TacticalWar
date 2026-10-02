"""Génère docs/protocol.md à partir de la table des opcodes (ProtocolLib/Opcodes.h).

Usage (depuis la racine du dépôt) : py tools/gen_protocol_doc.py
"""
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SOURCE = os.path.join(ROOT, 'ProtocolLib', 'Opcodes.h')
TARGET = os.path.join(ROOT, 'docs', 'protocol.md')

DIRECTIONS = {'CLIENT_TO_SERVER': 'C → S', 'SERVER_TO_CLIENT': 'S → C', 'BOTH': 'C ↔ S'}
ROLES = {'ANY': 'tous', 'SPECTATOR': 'spectateur', 'PLAYER': 'joueur', 'ADMIN': 'admin'}

HEADER = """# Protocole réseau

> Fichier généré par `py tools/gen_protocol_doc.py` à partir de `ProtocolLib/Opcodes.h` : ne pas le modifier à la main.

Le client et le serveur échangent des **lignes de texte UTF-8** sur TCP (port {game_port} par défaut).
Chaque ligne commence par un **opcode de 2 caractères**, suivi du contenu du message :

- pour la plupart des messages, un objet **JSON compact** sur une seule ligne (`CL{{"slot":0,"x":4,"y":7}}`) ;
- pour quelques messages hérités de la première version, un texte simple (`HG<identifiant>;<mot de passe>`, `HG<numéro de carte>`).

Le serveur fait autorité : il valide chaque action et diffuse des **événements** aux valeurs absolues
(PV, bouclier, PA, PM, positions) par lots numérotés (`BV`, champ `seq`). Un client qui détecte un trou
dans la numérotation redemande l'état complet (`BR`, réponse `BI`).

Version du protocole : **{version}**. Une page web de suivi du tournoi est servie en HTTP sur le port {http_port}
(`/`, `/api/state`, `/api/events` en Server-Sent Events, `/api/health`).

**Rôle requis** : rôle minimal du client pour envoyer le message au serveur (le serveur ignore les messages
non autorisés). « Spectateur » inclut les joueurs et l'administrateur.

"""


def main():
    text = open(SOURCE, encoding='utf-8-sig').read()
    version = re.search(r'PROTOCOL_VERSION\s*=\s*(\d+)', text).group(1)
    game_port = re.search(r'DEFAULT_GAME_PORT\s*=\s*(\d+)', text).group(1)
    http_port = re.search(r'DEFAULT_HTTP_PORT\s*=\s*(\d+)', text).group(1)

    table = text[text.index('OPCODES[]'):]
    out = [HEADER.format(version=version, game_port=game_port, http_port=http_port)]
    section = None
    for line in table.splitlines():
        comment = re.match(r'\s*//\s*(.+)', line)
        if comment:
            section = comment.group(1).strip()
            out.append('\n## %s\n\n| Opcode | Sens | Rôle requis | Description |\n|---|---|---|---|\n' % section)
            continue
        entry = re.match(r'\s*\{\s*"(\w\w)",\s*Direction::(\w+),\s*Role::(\w+),\s*"(.*)"\s*\},?', line)
        if entry:
            op, direction, role, description = entry.groups()
            description = description.replace('\\"', '"').replace('|', '\\|').replace('<', '&lt;').replace('>', '&gt;')
            out.append('| `%s` | %s | %s | %s |\n' % (op, DIRECTIONS[direction], ROLES[role], description))

    os.makedirs(os.path.dirname(TARGET), exist_ok=True)
    with open(TARGET, 'w', encoding='utf-8', newline='\n') as f:
        f.write(''.join(out))
    print('Écrit :', os.path.relpath(TARGET, ROOT))


if __name__ == '__main__':
    main()
