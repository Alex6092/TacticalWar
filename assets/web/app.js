// Vue projetée du tournoi : à gauche le tableau (combats, poules ou arbre, meilleurs joueurs,
// cérémonie), à droite les onglets En direct, Commentaire, Derniers combats, À venir, Classement.
// Paramètres d'URL :
//   ?t=<id>                 tournoi à afficher (par défaut : le plus récent en cours, sinon le plus récent)
//   ?rotate=<secondes>      fait défiler les onglets de gauche automatiquement
//   ?rotate-side=<secondes> fait défiler les onglets de droite automatiquement
//   ?carousel=1             les deux défilent (20 s à gauche, 12 s à droite)
// Le bouton « Défilement » de chaque zone l'active ou l'arrête (retenu par le navigateur) ; un clic sur
// un onglet suspend le défilement de sa zone pendant une minute.
"use strict";

const params = new URLSearchParams(location.search);
const carousel = params.get("carousel") === "1";

let state = null;
let version = 0;
let activeTab = null;
let userPickedTab = false;
let lastBoardHtml = null;
let sideTab = null;
let sidePicked = false;
let visibleSideTabs = [];

// ---------------------------------------------------------------- défilement automatique

const PAUSE_AFTER_CLICK_MS = 60000;

function storedFlag(key) {
  try {
    const value = window.localStorage.getItem(key);
    return value === null ? null : value === "1";
  } catch (e) {
    return null;
  }
}

function storeFlag(key, value) {
  try {
    window.localStorage.setItem(key, value ? "1" : "0");
  } catch (e) {
    // Stockage indisponible (navigation privée...) : l'état n'est pas retenu.
  }
}

// Une zone qui défile : durée par onglet, état du bouton, pause après un clic.
function rotation(name, param, fallbackSeconds) {
  const seconds = parseInt(params.get(param) || "0", 10);
  const stored = storedFlag("tw.rotate." + name);
  return {
    name,
    seconds: seconds > 0 ? seconds : fallbackSeconds,
    enabled: stored !== null ? stored : seconds > 0 || carousel,
    pausedUntil: 0,
    switchedAt: Date.now(),
  };
}

const boardRotation = rotation("board", "rotate", 20);
const sideRotation = rotation("side", "rotate-side", 12);

function refreshRotateButtons() {
  for (const [id, zone] of [["board-rotate", boardRotation], ["side-rotate", sideRotation]]) {
    const button = el(id);
    const paused = zone.enabled && Date.now() < zone.pausedUntil;
    button.classList.toggle("on", zone.enabled);
    button.textContent = zone.enabled ? (paused ? "Défilement (pause)" : "Défilement ▶") : "Défilement";
  }
}

function toggleRotation(zone) {
  zone.enabled = !zone.enabled;
  zone.pausedUntil = 0;
  zone.switchedAt = Date.now();
  storeFlag("tw.rotate." + zone.name, zone.enabled);
  refreshRotateButtons();
}

// Onglet choisi à la main : la zone ne défile plus pendant une minute.
function pauseRotation(zone) {
  zone.pausedUntil = Date.now() + PAUSE_AFTER_CLICK_MS;
  zone.switchedAt = Date.now();
  refreshRotateButtons();
}

// ---------------------------------------------------------------- utilitaires

function esc(text) {
  return String(text ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

function el(id) {
  return document.getElementById(id);
}

const REASONS = { KO: "KO", ROUND_LIMIT: "aux PV", FORFEIT: "forfait", SURRENDER: "abandon", ADMIN: "arbitrage", BYE: "exempt", OBJECTIVE: "zone" };
const STATUS = { RUNNING: "En cours", FINISHED: "Terminé", DRAFT: "Préparation" };

function teamName(tournament, id) {
  if (id === -1) return "Exempt";
  if (!id) return "…";
  return tournament.teamNames[String(id)] || "Équipe " + id;
}

function currentTournament() {
  if (!state || !state.tournaments.length) return null;
  const wanted = parseInt(params.get("t") || "0", 10);
  if (wanted) return state.tournaments.find((t) => t.id === wanted) || null;
  const running = state.tournaments.filter((t) => t.status === "RUNNING");
  const list = running.length ? running : state.tournaments;
  return list.reduce((a, b) => (b.id > a.id ? b : a));
}

// ---------------------------------------------------------------- en-tête

function renderHeader(tournament) {
  el("tournament-name").textContent = tournament ? tournament.name : "En attente d'un tournoi...";
  const pill = el("tournament-status");
  if (!tournament) {
    pill.textContent = "";
    pill.className = "pill";
    return;
  }
  const paused = tournament.paused && tournament.status === "RUNNING";
  pill.textContent = paused ? "En pause" : STATUS[tournament.status] || tournament.status;
  pill.className = "pill " + (paused ? "paused" : tournament.status.toLowerCase());
}

// ---------------------------------------------------------------- tableau / poules

function boardTabs(tournament) {
  const tabs = [];
  // Tournoi terminé : la cérémonie (podium, MVP, hauts faits rares) en premier.
  if (tournament.status === "FINISHED" && tournament.ranking && tournament.ranking.length) tabs.push({ id: "ceremony", label: "Cérémonie" });
  if (tournament.standings.length) {
    const swiss = tournament.stages.some((s) => s.type === "SWISS");
    tabs.push({ id: "standings", label: swiss ? "Classement suisse" : "Poules" });
  }
  const elimination = tournament.stages.some((s, i) => s.built && (s.type === "SINGLE_ELIMINATION" || s.type === "DOUBLE_ELIMINATION"));
  if (elimination) tabs.push({ id: "bracket", label: "Arbre" });
  if (tournament.leaders && tournament.leaders.length) tabs.push({ id: "leaders", label: "Meilleurs joueurs" });
  return tabs;
}

// Onglets du tableau, avec la mosaïque des combats en premier quand des combats sont en cours.
function allTabs(tournament) {
  const tabs = tournament ? boardTabs(tournament) : [];
  if (liveFights(tournament).length) tabs.unshift({ id: "mosaic", label: "Combats" });
  return tabs;
}

function renderBoard(tournament) {
  const board = el("board");
  const tabsBox = el("board-tabs");
  const tabs = allTabs(tournament);
  if (!tabs.length) {
    board.innerHTML = tournament ? '<p class="empty">Le tournoi n\'a pas encore commencé.</p>' : '<p class="empty">Aucun tournoi en cours.</p>';
    lastBoardHtml = null;
    tabsBox.innerHTML = "";
    return;
  }

  // Par défaut : les combats en cours, sinon l'arbre dès qu'il existe (phase finale), sinon les poules.
  if (!activeTab || !tabs.some((t) => t.id === activeTab) || !userPickedTab) {
    activeTab = tabs.some((t) => t.id === "mosaic") ? "mosaic" : tabs.some((t) => t.id === "ceremony") ? "ceremony"
      : tabs.some((t) => t.id === "bracket") ? "bracket" : tabs[0].id;
    if (userPickedTab && !tabs.some((t) => t.id === activeTab)) userPickedTab = false;
  }

  tabsBox.innerHTML = tabs.length > 1
    ? tabs.map((t) => `<button data-tab="${t.id}" class="${t.id === activeTab ? "active" : ""}">${esc(t.label)}</button>`).join("")
    : "";
  tabsBox.querySelectorAll("button").forEach((button) => {
    button.onclick = () => {
      activeTab = button.dataset.tab;
      userPickedTab = true;
      pauseRotation(boardRotation);
      render();
    };
  });

  el("board-title").textContent = tabs.find((t) => t.id === activeTab).label;
  const html = activeTab === "mosaic" ? renderMosaic(tournament)
    : activeTab === "ceremony" ? renderCeremony(tournament)
    : activeTab === "bracket" ? renderBracket(tournament)
    : activeTab === "leaders" ? renderLeaders(tournament)
    : renderStandings(tournament);
  // Contenu inchangé : rien n'est redessiné (les animations de la cérémonie ne repartent pas à zéro).
  if (html !== lastBoardHtml) {
    board.innerHTML = html;
    lastBoardHtml = html;
  }
}

// ---------------------------------------------------------------- mosaïque des combats

// Cartes compactes (/api/map/<id>), chargées une fois.
const maps = {};
function mapOf(id) {
  if (maps[id] === undefined) {
    maps[id] = null;
    fetch(`api/map/${id}`).then((response) => response.json()).then((map) => {
      maps[id] = map;
      lastBoardHtml = null;
      render();
    }).catch(() => { delete maps[id]; });
  }
  return maps[id];
}

function liveFights(tournament) {
  return (state ? state.live : []).filter((b) => b.fighters && b.mapId && (!tournament || !b.tournament || b.tournament === tournament.id));
}

const CELL_COLORS = { ".": "#2e4a36", "#": "#11151f", "~": "#1d4a72", h: "#3f6e33", e: "#7a3418", s: "#1d6c6c" };
const ORB_COLORS = { soin: "#4fd18b", energie: "#ffd34d", protection: "#7fb6ff" };
const TEAM_COLORS = { 1: "#4c8dff", 2: "#ff5d55" };

function teamHp(battle, team) {
  const fighters = battle.fighters.filter((f) => f.team === team);
  const max = fighters.reduce((sum, f) => sum + Math.max(1, f.initialMaxHp), 0);
  const hp = fighters.reduce((sum, f) => sum + (f.alive ? Math.max(0, f.hp) : 0), 0);
  return max ? Math.round(hp * 100 / max) : 0;
}

// Carte vue de dessus : cases, zone à tenir, murs, orbes, puis les combattants (initiale de la
// classe, anneau de PV, le combattant actif cerclé d'or).
function miniMap(battle, map) {
  const S = 10;
  const parts = [];
  for (let y = 0; y < map.height; y++) {
    for (let x = 0; x < map.width; x++) {
      const kind = map.cells[y * map.width + x] || ".";
      parts.push(`<rect x="${x * S}" y="${y * S}" width="${S}" height="${S}" fill="${CELL_COLORS[kind] || CELL_COLORS["."]}"/>`);
    }
  }
  // Carte qui rétrécit : cases fermées en noir.
  (battle.closed || []).forEach(([x, y]) => parts.push(`<rect x="${x * S}" y="${y * S}" width="${S}" height="${S}" fill="#05060a"/>`));
  if (battle.zone && battle.zone.cells) {
    battle.zone.cells.forEach(([x, y]) => parts.push(`<rect x="${x * S}" y="${y * S}" width="${S}" height="${S}" fill="#ffd34d" opacity="0.28"/>`));
  }
  (battle.blocks || []).forEach((b) => {
    parts.push(`<rect x="${b.x * S + 1}" y="${b.y * S + 1}" width="${S - 2}" height="${S - 2}" rx="1.5" fill="${b.move ? "#9aa3b8" : "#cfe9ff"}" opacity="${b.move ? 1 : 0.6}"/>`);
    parts.push(`<rect x="${b.x * S + 1}" y="${b.y * S + S - 2.4}" width="${(S - 2) * Math.max(0, b.hp) / Math.max(1, b.maxHp)}" height="1.4" fill="#ff5d55"/>`);
  });
  (battle.orbs || []).forEach((o) => {
    const cx = o.x * S + S / 2;
    const cy = o.y * S + S / 2;
    parts.push(`<path d="M${cx} ${cy - 3.2} L${cx + 3.2} ${cy} L${cx} ${cy + 3.2} L${cx - 3.2} ${cy} Z" fill="${ORB_COLORS[o.kind] || "#fff"}"/>`);
  });
  battle.fighters.forEach((f) => {
    const cx = f.x * S + S / 2;
    const cy = f.y * S + S / 2;
    if (!f.alive) {
      parts.push(`<text x="${cx}" y="${cy + 2.2}" text-anchor="middle" font-size="6" fill="#8b97bd">✕</text>`);
      return;
    }
    const ratio = Math.max(0, Math.min(1, f.hp / Math.max(1, f.initialMaxHp)));
    const ring = 2 * Math.PI * 4.6;
    if (f.id === battle.active) parts.push(`<circle cx="${cx}" cy="${cy}" r="6.4" fill="none" stroke="#ffd34d" stroke-width="1.4"/>`);
    parts.push(`<circle cx="${cx}" cy="${cy}" r="3.6" fill="${TEAM_COLORS[f.team] || "#fff"}" stroke="#000" stroke-width="0.5"/>`);
    parts.push(`<circle cx="${cx}" cy="${cy}" r="4.6" fill="none" stroke="#4fd18b" stroke-width="1.1" stroke-dasharray="${(ring * ratio).toFixed(2)} ${ring.toFixed(2)}" transform="rotate(-90 ${cx} ${cy})"/>`);
    parts.push(`<text x="${cx}" y="${cy + 1.7}" text-anchor="middle" font-size="4.6" font-weight="700" fill="#fff">${esc((f.className || "?").charAt(0))}</text>`);
  });
  return `<svg viewBox="0 0 ${map.width * S} ${map.height * S}" preserveAspectRatio="xMidYMid meet">${parts.join("")}</svg>`;
}

function renderMosaic(tournament) {
  const fights = liveFights(tournament);
  if (!fights.length) return '<p class="empty">Aucun combat en cours.</p>';
  return `<div class="mosaic">${fights.map((battle) => {
    const map = mapOf(battle.mapId);
    const label = tournament && battle.match ? tournament.labels[String(battle.match)] : battle.name;
    const phase = battle.phase === "PLACEMENT" ? "Placement" : `Tour ${battle.round}`;
    const score = battle.zone ? `<span class="mini-zone">${battle.zone.scores[0]} - ${battle.zone.scores[1]}</span>` : '<span class="mini-vs">VS</span>';
    return `<div class="mini">
      <div class="battle-head"><span>${esc(label || "")}</span><span>${esc(phase)}</span></div>
      <div class="mini-map">${map ? miniMap(battle, map) : '<p class="empty">Carte...</p>'}</div>
      <div class="mini-teams">
        <span class="t1">${esc(battle.teams[0])} <strong>${teamHp(battle, 1)} %</strong></span>${score}<span class="t2"><strong>${teamHp(battle, 2)} %</strong> ${esc(battle.teams[1])}</span>
      </div>
    </div>`;
  }).join("")}</div>`;
}

// Cérémonie de fin : podium des trois premières équipes (avec leurs joueurs), MVP du tournoi (meilleur
// bilan cumulé), hauts faits les plus rares, et des confettis.
function renderCeremony(tournament) {
  const playersOf = (team) => ((tournament.teamPlayers || {})[String(team)] || []).map(esc).join(" · ");
  const step = (rank) => {
    const teams = tournament.ranking.filter((r) => r.rank === rank);
    if (!teams.length) return `<div class="step s${rank}"><span class="place">${rank}</span></div>`;
    return `<div class="step s${rank}"><span class="place">${rank}</span>${teams.map((r) =>
      `<div class="team">${esc(teamName(tournament, r.team))}</div><div class="players">${playersOf(r.team)}</div>`).join("")}</div>`;
  };
  const mvp = tournament.leaders && tournament.leaders.length ? tournament.leaders[0] : null;
  const mvpCard = mvp ? `<div class="mvp-card"><div class="label">★ MVP du tournoi</div>
      <div class="name">${esc(mvp.name)}</div><div class="sub">${esc(mvp.class)} · ${esc(teamName(tournament, mvp.team))}</div>
      <div class="stats">${mvp.dealt} dégâts · ${mvp.healed} soins · ${mvp.kills} KO · ${mvp.mvp} fois MVP</div></div>` : "";
  const rare = (tournament.badges || []).slice(0, 3).map((b) => `<li><strong>${esc(b.name)}</strong>
      <span class="count">${b.count === 1 ? "une seule fois" : b.count + " fois"}</span>
      <div class="who">${b.players.map(esc).join(", ")}</div></li>`).join("");
  const colors = ["#ffd34d", "#ff6b6b", "#5ec8ff", "#8dff8a", "#ff9bf2", "#ffffff"];
  let confetti = "";
  for (let i = 0; i < 40; i++) {
    confetti += `<span style="left:${(i * 37) % 100}%;background:${colors[i % colors.length]};animation-delay:${(i * 0.23) % 4}s;animation-duration:${3 + (i % 5) * 0.6}s"></span>`;
  }
  return `<div class="ceremony"><div class="confetti">${confetti}</div>
    <h2>Bravo à tous les joueurs !</h2>
    <div class="big-podium">${step(2)}${step(1)}${step(3)}</div>
    <div class="ceremony-row">${mvpCard}
      ${rare ? `<div class="rare-card"><div class="label">Hauts faits les plus rares</div><ol>${rare}</ol></div>` : ""}</div>
  </div>`;
}

// Meilleurs joueurs du tournoi : bilan cumulé sur les matchs joués (score : dégâts + soins
// + boucliers / 2 + 25 par KO).
function renderLeaders(tournament) {
  const rows = tournament.leaders.map((p, i) => `<tr class="${i < 3 ? "top" : ""}">
    <td class="rank">${i + 1}</td>
    <td><strong>${esc(p.name)}</strong> <small>${esc(p.class)} · ${esc(teamName(tournament, p.team))}</small></td>
    <td>${p.dealt}</td><td>${p.healed}</td><td>${p.shielded}</td><td>${p.kills}</td>
    <td class="mvp-count">${p.mvp ? "★ " + p.mvp : ""}</td><td class="badge-count">${p.badges ? p.badges : ""}</td><td>${p.matches}</td>
  </tr>`).join("");
  return `<table class="leaders">
    <thead><tr><th>#</th><th>Joueur</th><th>Dégâts</th><th>Soins</th><th>Boucliers</th><th>KO</th><th>MVP</th><th>Hauts faits</th><th>Matchs</th></tr></thead>
    <tbody>${rows}</tbody>
  </table>`;
}

function renderStandings(tournament) {
  const pools = tournament.stages.find((s) => s.type === "ROUND_ROBIN_POOLS");
  const qualifiers = tournament.settings.qualifiersPerPool || 0;
  const swiss = tournament.stages.some((s) => s.type === "SWISS");

  const tables = tournament.standings.map((group) => {
    const rows = group.rows.map((row, index) => {
      const qualified = pools && index < qualifiers;
      const hp = Math.round(row.hp);
      return `<tr class="${qualified ? "qualified" : ""}">
        <td>${index + 1}</td>
        <td class="team">${esc(teamName(tournament, row.team))}</td>
        <td>${row.played}</td><td>${row.wins}</td><td>${row.losses}</td>
        <td><strong>${row.points}</strong></td>
        ${swiss ? `<td>${row.buchholz}</td>` : ""}
        <td>${hp > 0 ? "+" : ""}${hp}</td>
      </tr>`;
    }).join("");
    return `<table class="standings">
      <caption>${esc(group.title)}</caption>
      <thead><tr><th>#</th><th class="team">Équipe</th><th>J</th><th>V</th><th>D</th><th>Pts</th>${swiss ? "<th>Buchholz</th>" : ""}<th>± PV</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>`;
  });

  return `<div class="pools">${tables.join("")}</div>`;
}

// Arbre en SVG : colonnes = tours ; chaque match est centré entre les matchs dont il reçoit les vainqueurs.
function renderBracket(tournament) {
  const stageIndex = tournament.stages.findIndex((s) => s.built && (s.type === "SINGLE_ELIMINATION" || s.type === "DOUBLE_ELIMINATION"));
  const matches = tournament.matches.filter((m) => m.stage === stageIndex);
  const byId = new Map(matches.map((m) => [m.id, m]));

  const BOX_W = 270, BOX_H = 68, COL_GAP = 60, ROW_GAP = 20, TITLE_H = 34;
  const positions = new Map();
  let svg = "";
  let maxX = 0, maxY = 0;

  function layoutBracket(bracket, originX, originY, title) {
    const list = matches.filter((m) => m.bracket === bracket);
    if (!list.length) return { width: 0, height: 0 };
    const rounds = [...new Set(list.map((m) => m.round))].sort((a, b) => a - b);
    let height = 0;

    rounds.forEach((round, column) => {
      const inRound = list.filter((m) => m.round === round).sort((a, b) => a.order - b.order);
      inRound.forEach((match, index) => {
        const children = [match.slotA, match.slotB]
          .filter((s) => s && s.kind === "WINNER_OF" && byId.has(s.value) && byId.get(s.value).bracket === bracket && positions.has(s.value))
          .map((s) => positions.get(s.value).y);
        let y = children.length ? children.reduce((a, b) => a + b, 0) / children.length : originY + TITLE_H + index * (BOX_H + ROW_GAP);
        // Évite les chevauchements dans une même colonne.
        const previous = inRound.slice(0, index).map((m) => positions.get(m.id).y);
        if (previous.length && y < Math.max(...previous) + BOX_H + ROW_GAP) y = Math.max(...previous) + BOX_H + ROW_GAP;
        positions.set(match.id, { x: originX + column * (BOX_W + COL_GAP), y });
        height = Math.max(height, y + BOX_H - originY);
      });
    });

    svg += `<text class="section-title" x="${originX}" y="${originY + 18}">${esc(title)}</text>`;
    return { width: rounds.length * (BOX_W + COL_GAP), height };
  }

  const double = tournament.stages[stageIndex].type === "DOUBLE_ELIMINATION";
  const winners = layoutBracket("W", 10, 0, double ? "Tableau des gagnants" : "Phase finale");
  let bottom = winners.height + 30;
  if (double) {
    const losers = layoutBracket("L", 10, bottom, "Tableau des perdants");
    bottom += losers.height + 30;
  }

  // Grande finale / petite finale.
  const wFinal = matches.filter((m) => m.bracket === "W").sort((a, b) => b.round - a.round)[0];
  const finalPos = wFinal ? positions.get(wFinal.id) : { x: 10, y: 0 };
  let extraX = Math.max(winners.width + 10, finalPos.x + BOX_W + COL_GAP);
  for (const bracket of ["GF", "GF2"]) {
    const match = matches.find((m) => m.bracket === bracket);
    if (match) {
      positions.set(match.id, { x: extraX, y: finalPos.y });
      svg += `<text class="section-title" x="${extraX}" y="${finalPos.y - 8}">${bracket === "GF" ? "Grande finale" : "Revanche"}</text>`;
      extraX += BOX_W + COL_GAP;
    }
  }
  const third = matches.find((m) => m.bracket === "3P");
  if (third) {
    positions.set(third.id, { x: finalPos.x, y: finalPos.y + BOX_H + 60 });
    svg += `<text class="section-title" x="${finalPos.x}" y="${finalPos.y + BOX_H + 52}">Petite finale</text>`;
  }

  // Liaisons entre un match et celui qui reçoit son vainqueur.
  for (const match of matches) {
    const to = positions.get(match.id);
    if (!to) continue;
    for (const slot of [match.slotA, match.slotB]) {
      if (!slot || slot.kind !== "WINNER_OF" || !positions.has(slot.value)) continue;
      const from = positions.get(slot.value);
      const x1 = from.x + BOX_W, y1 = from.y + BOX_H / 2, x2 = to.x, y2 = to.y + BOX_H / 2;
      const mid = (x1 + x2) / 2;
      svg += `<path class="connector" d="M${x1},${y1} H${mid} V${y2} H${x2}"/>`;
    }
  }

  for (const match of matches) {
    const pos = positions.get(match.id);
    if (!pos) continue;
    svg += matchBox(tournament, match, pos.x, pos.y, BOX_W, BOX_H);
    maxX = Math.max(maxX, pos.x + BOX_W);
    maxY = Math.max(maxY, pos.y + BOX_H);
  }

  return `<svg viewBox="0 0 ${maxX + 20} ${maxY + 20}" xmlns="http://www.w3.org/2000/svg">${svg}</svg>`;
}

function matchBox(tournament, match, x, y, w, h) {
  const result = match.result;
  const live = match.status === "IN_PROGRESS";
  const ready = match.status === "READY";
  const cls = live ? "live" : ready ? "ready" : "";
  const line = (teamId, dy, hp) => {
    let c = "";
    if (result) c = result.winner === teamId ? "winner" : "loser";
    const name = teamName(tournament, teamId);
    const score = result && result.reason !== "BYE" ? `${Math.round(hp)}%` : "";
    return `<text class="${c}" x="${x + 10}" y="${y + dy}">${esc(name.length > 20 ? name.slice(0, 19) + "…" : name)}</text>
            <text class="score" x="${x + w - 10}" y="${y + dy}" text-anchor="end">${score}</text>`;
  };
  const caption = live ? "en cours" : result ? REASONS[result.reason] || "" : "";
  return `<g class="match-box ${cls}">
    <rect class="frame" x="${x}" y="${y}" width="${w}" height="${h}" rx="6"/>
    ${line(match.teamA, 27, result ? result.hpA : 0)}
    ${line(match.teamB, 55, result ? result.hpB : 0)}
    <text class="caption" x="${x + w - 10}" y="${y - 4}" text-anchor="end">${esc(caption)}</text>
  </g>`;
}

// ---------------------------------------------------------------- colonne de droite (onglets)

const SIDE_TABS = [
  { id: "live", label: "En direct" },
  { id: "comments", label: "Commentaire" },
  { id: "recent", label: "Derniers combats" },
  { id: "upcoming", label: "À venir" },
  { id: "ranking", label: "Classement" },
];

// Un onglet à la fois, sur toute la hauteur ; les onglets sans contenu sont masqués (En direct reste
// quand rien d'autre n'est à montrer).
function renderSide(tournament) {
  const counts = {
    live: renderLive(tournament),
    comments: renderComments(tournament),
    recent: renderRecent(tournament),
    upcoming: renderUpcoming(tournament),
    ranking: renderRanking(tournament),
  };
  let tabs = SIDE_TABS.filter((t) => counts[t.id] > 0);
  if (!tabs.length) tabs = [SIDE_TABS[0]];
  if (!sideTab || !tabs.some((t) => t.id === sideTab) || !sidePicked) {
    sideTab = tabs[0].id;
    sidePicked = false;
  }
  const box = el("side-tabs");
  box.innerHTML = tabs.map((t) => `<button data-tab="${t.id}" class="${t.id === sideTab ? "active" : ""}">${esc(t.label)}${
    t.id === "live" && counts.live > 0 ? ` <span class="tab-count">${counts.live}</span>` : ""}</button>`).join("");
  box.querySelectorAll("button").forEach((button) => {
    button.onclick = () => {
      sideTab = button.dataset.tab;
      sidePicked = true;
      pauseRotation(sideRotation);
      render();
    };
  });
  for (const t of SIDE_TABS) el("side-" + t.id).hidden = t.id !== sideTab;
  visibleSideTabs = tabs;
}

// Retourne le nombre de combats (0 : aucun).
function renderLive(tournament) {
  const battles = (state ? state.live : []).filter((b) => !tournament || !b.tournament || b.tournament === tournament.id);
  if (!battles.length) {
    el("live").innerHTML = '<p class="empty">Aucun combat en cours.</p>';
    return 0;
  }

  el("live").innerHTML = battles.map((battle) => {
    const label = tournament && battle.match ? tournament.labels[String(battle.match)] : battle.name;
    let body;
    if (!battle.fighters) {
      body = `<p class="phase-note">${battle.phase === "BAN" ? "Bannissement des classes..." : "Choix des classes..."}</p>`;
    } else {
      const side = (team) => battle.fighters.filter((f) => f.team === team).map((f) => {
        const max = Math.max(1, f.initialMaxHp);
        const hp = Math.max(0, f.hp) / max * 100;
        const shield = Math.min(100 - hp, f.shield / max * 100);
        return `<div class="fighter ${f.alive ? "" : "dead"} ${f.id === battle.active ? "active" : ""}">
          <div class="fighter-name"><span>${esc(f.name)} <small>${esc(f.className)}</small></span><span>${f.alive ? f.hp : "KO"}${f.alive && f.shield > 0 ? ` <span class="shield-num">+${f.shield}</span>` : ""}</span></div>
          <div class="bar"><span class="hp" style="width:${hp}%"></span><span class="shield" style="left:${hp}%;width:${shield}%"></span></div>
          ${f.talents && f.talents.length ? `<div class="talents">${f.talents.map(esc).join(" · ")}</div>` : ""}
        </div>`;
      }).join("");
      body = `<div class="battle-teams">
        <div class="side-1"><div class="team-name">${esc(battle.teams[0])}</div>${side(1)}</div>
        <div class="vs">${battle.zone
          ? `<span class="zone-score">${battle.zone.scores[0]} - ${battle.zone.scores[1]}</span><small class="zone-goal">zone, premier à ${battle.zone.points}</small>`
          : "VS"}</div>
        <div class="side-2"><div class="team-name">${esc(battle.teams[1])}</div>${side(2)}</div>
      </div>`;
    }
    const phase = battle.phase === "PLACEMENT" ? "Placement" : battle.phase === "FIGHT" ? `Tour ${battle.round}` : "";
    // Classes interdites à chaque équipe par le bannissement.
    const forbidden = battle.forbidden
      ? `<div class="forbidden">Interdit : ${[0, 1].map((i) => `${esc(battle.teams[i])} <strong>${esc(battle.forbidden[i] || "rien")}</strong>`).join(" · ")}</div>`
      : "";
    return `<div class="battle"><div class="battle-head"><span>${esc(label || "")}</span><span>${esc(phase)}</span></div>${body}${forbidden}</div>`;
  }).join("");
  return battles.length;
}

// Commentaire automatique des combats : les dernières phrases, les nouvelles en surbrillance (un
// nouveau commentaire ne change pas d'onglet).
const seenComments = new Set();
let commentsPrimed = false;
function renderComments(tournament) {
  const lines = (state && state.comments ? state.comments : [])
    .filter((c) => !tournament || !c.tournament || c.tournament === tournament.id)
    .slice(0, 12);
  if (!lines.length) {
    el("comments").innerHTML = "";
    return 0;
  }
  el("comments").innerHTML = lines.map((c) => {
    const key = `${c.session}:${c.at}:${c.text}`;
    // Au premier affichage de la page, rien n'est mis en surbrillance.
    const fresh = commentsPrimed && !seenComments.has(key);
    seenComments.add(key);
    return `<li class="${fresh ? "fresh" : ""}"><span class="comment-match">${esc(c.match || "")}</span>${esc(c.text)}</li>`;
  }).join("");
  commentsPrimed = true;
  return lines.length;
}

// Derniers combats terminés : vainqueur et MVP (meilleur bilan du combat).
function renderRecent(tournament) {
  const battles = (state && state.recent ? state.recent : [])
    .filter((b) => !tournament || !b.tournament || b.tournament === tournament.id)
    .slice(0, 6);
  if (!battles.length) {
    el("recent").innerHTML = "";
    return 0;
  }
  el("recent").innerHTML = battles.map((b) => {
    const label = tournament && b.match ? tournament.labels[String(b.match)] : b.name;
    const winner = b.teams[b.winner - 1] || "";
    const mvp = b.mvp && b.mvp.name
      ? `<div class="mvp"><span class="star">★ MVP</span> <strong>${esc(b.mvp.name)}</strong> <small>${esc(b.mvp.class)}</small>
          <div class="mvp-stats">${b.mvp.dealt} dégâts · ${b.mvp.healed} soins · ${b.mvp.shielded} boucliers · ${b.mvp.kills} KO</div>
          ${b.mvp.badges && b.mvp.badges.length ? `<div class="mvp-badges">${b.mvp.badges.map(esc).join(" · ")}</div>` : ""}</div>`
      : "";
    return `<div class="recent-battle">
      <div class="battle-head"><span>${esc(label || "")}</span><span>${esc(REASONS[b.reason] || "")}</span></div>
      <div class="winner">Victoire : <strong>${esc(winner)}</strong></div>${mvp}
    </div>`;
  }).join("");
  return battles.length;
}

function renderUpcoming(tournament) {
  const list = el("upcoming");
  if (!tournament) {
    list.innerHTML = "";
    return 0;
  }
  const next = tournament.matches
    .filter((m) => m.status === "READY" || (m.status === "PENDING" && (m.teamA > 0 || m.teamB > 0)))
    .sort((a, b) => (a.status === b.status ? a.id - b.id : a.status === "READY" ? -1 : 1))
    .slice(0, 6);
  list.innerHTML = next.length
    ? next.map((m) => `<li><span class="label">${esc(tournament.labels[String(m.id)] || "")}</span>${esc(teamName(tournament, m.teamA))} — ${esc(teamName(tournament, m.teamB))}</li>`).join("")
    : "";
  return next.length;
}

function renderRanking(tournament) {
  if (!tournament || !tournament.ranking.length) {
    el("ranking").innerHTML = "";
    return 0;
  }
  const byRank = (rank) => tournament.ranking.filter((r) => r.rank === rank).map((r) => teamName(tournament, r.team)).join(" / ");
  const rest = tournament.ranking.filter((r) => r.rank > 3);
  el("ranking").innerHTML = `
    <div class="podium">
      <div class="p2"><span class="rank">2</span>${esc(byRank(2))}</div>
      <div class="p1"><span class="rank">1</span>${esc(byRank(1))}</div>
      <div class="p3"><span class="rank">3</span>${esc(byRank(3))}</div>
    </div>
    <ol class="ranking-rest">${rest.map((r) => `<li value="${r.rank}">${esc(teamName(tournament, r.team))}</li>`).join("")}</ol>`;
  return tournament.ranking.length;
}

function render() {
  const tournament = currentTournament();
  renderHeader(tournament);
  renderBoard(tournament);
  renderSide(tournament);
  refreshRotateButtons();
}

// ---------------------------------------------------------------- mises à jour en direct

async function fetchState(since) {
  const url = since === undefined ? "/api/state" : `/api/state?since=${since}`;
  const response = await fetch(url, { cache: "no-store" });
  if (!response.ok) throw new Error(response.status);
  version = parseInt(response.headers.get("X-State-Version") || "0", 10);
  state = await response.json();
  el("connection").classList.add("ok");
  render();
}

function connectEvents() {
  let failures = 0;
  const source = new EventSource("/api/events");
  source.onmessage = () => {
    failures = 0;
    fetchState().catch(() => el("connection").classList.remove("ok"));
  };
  source.onerror = () => {
    el("connection").classList.remove("ok");
    if (++failures >= 3) {
      // Trop de pages ouvertes ou proxy : bascule en long-poll.
      source.close();
      longPoll();
    }
  };
}

async function longPoll() {
  for (;;) {
    try {
      await fetchState(version);
    } catch (e) {
      el("connection").classList.remove("ok");
      await new Promise((resolve) => setTimeout(resolve, 3000));
    }
  }
}

function tickClock() {
  el("clock").textContent = new Date().toLocaleTimeString("fr-FR", { hour: "2-digit", minute: "2-digit" });
}

// Défilement : chaque seconde, une zone active et non suspendue passe à l'onglet suivant quand sa
// durée est écoulée. À la fin du tournoi, la cérémonie reste affichée à gauche.
function nextTab(tabs, current) {
  const index = tabs.findIndex((t) => t.id === current);
  return tabs[(index + 1) % tabs.length].id;
}

function rotationDue(zone, now) {
  return zone.enabled && now >= zone.pausedUntil && now - zone.switchedAt >= zone.seconds * 1000;
}

setInterval(() => {
  const now = Date.now();
  const tournament = currentTournament();
  let changed = false;
  if (rotationDue(boardRotation, now)) {
    boardRotation.switchedAt = now;
    const tabs = allTabs(tournament);
    const ceremony = tournament && tournament.status === "FINISHED" && tabs.some((t) => t.id === "ceremony");
    if (tabs.length > 1 && !ceremony) {
      activeTab = nextTab(tabs, activeTab);
      userPickedTab = true;
      changed = true;
    }
  }
  if (rotationDue(sideRotation, now)) {
    sideRotation.switchedAt = now;
    if (visibleSideTabs.length > 1) {
      sideTab = nextTab(visibleSideTabs, sideTab);
      sidePicked = true;
      changed = true;
    }
  }
  if (changed) render();
  else refreshRotateButtons();
}, 1000);

el("board-rotate").onclick = () => toggleRotation(boardRotation);
el("side-rotate").onclick = () => toggleRotation(sideRotation);
refreshRotateButtons();

tickClock();
setInterval(tickClock, 10000);
fetchState().catch(() => {}).finally(connectEvents);
