// Vue projetée du tournoi. Paramètres d'URL :
//   ?t=<id>            tournoi à afficher (par défaut : le plus récent en cours, sinon le plus récent)
//   ?rotate=<secondes> fait défiler les onglets du tableau (arbre, poules...) automatiquement
"use strict";

const params = new URLSearchParams(location.search);
const rotateSeconds = parseInt(params.get("rotate") || "0", 10);

let state = null;
let version = 0;
let activeTab = null;
let userPickedTab = false;

// ---------------------------------------------------------------- utilitaires

function esc(text) {
  return String(text ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

function el(id) {
  return document.getElementById(id);
}

const REASONS = { KO: "KO", ROUND_LIMIT: "aux PV", FORFEIT: "forfait", ADMIN: "arbitrage", BYE: "exempt", OBJECTIVE: "zone" };
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
  if (tournament.standings.length) {
    const swiss = tournament.stages.some((s) => s.type === "SWISS");
    tabs.push({ id: "standings", label: swiss ? "Classement suisse" : "Poules" });
  }
  const elimination = tournament.stages.some((s, i) => s.built && (s.type === "SINGLE_ELIMINATION" || s.type === "DOUBLE_ELIMINATION"));
  if (elimination) tabs.push({ id: "bracket", label: "Arbre" });
  if (tournament.leaders && tournament.leaders.length) tabs.push({ id: "leaders", label: "Meilleurs joueurs" });
  return tabs;
}

function renderBoard(tournament) {
  const board = el("board");
  const tabsBox = el("board-tabs");
  if (!tournament) {
    board.innerHTML = '<p class="empty">Aucun tournoi en cours.</p>';
    tabsBox.innerHTML = "";
    return;
  }

  const tabs = boardTabs(tournament);
  if (!tabs.length) {
    board.innerHTML = '<p class="empty">Le tournoi n\'a pas encore commencé.</p>';
    tabsBox.innerHTML = "";
    return;
  }

  // Par défaut : l'arbre dès qu'il existe (phase finale), sinon les poules.
  if (!activeTab || !tabs.some((t) => t.id === activeTab) || !userPickedTab) {
    activeTab = tabs.some((t) => t.id === "bracket") ? "bracket" : tabs[0].id;
    if (userPickedTab && !tabs.some((t) => t.id === activeTab)) userPickedTab = false;
  }

  tabsBox.innerHTML = tabs.length > 1
    ? tabs.map((t) => `<button data-tab="${t.id}" class="${t.id === activeTab ? "active" : ""}">${esc(t.label)}</button>`).join("")
    : "";
  tabsBox.querySelectorAll("button").forEach((button) => {
    button.onclick = () => {
      activeTab = button.dataset.tab;
      userPickedTab = true;
      render();
    };
  });

  el("board-title").textContent = tabs.find((t) => t.id === activeTab).label;
  board.innerHTML = activeTab === "bracket" ? renderBracket(tournament)
    : activeTab === "leaders" ? renderLeaders(tournament)
    : renderStandings(tournament);
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

// ---------------------------------------------------------------- colonne de droite

function renderLive(tournament) {
  const battles = (state ? state.live : []).filter((b) => !tournament || !b.tournament || b.tournament === tournament.id);
  el("live-count").textContent = battles.length ? `${battles.length} combat${battles.length > 1 ? "s" : ""}` : "";
  if (!battles.length) {
    el("live").innerHTML = '<p class="empty">Aucun combat en cours.</p>';
    return;
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
}

// Derniers combats terminés : vainqueur et MVP (meilleur bilan du combat).
function renderRecent(tournament) {
  const panel = el("recent-panel");
  const battles = (state && state.recent ? state.recent : [])
    .filter((b) => !tournament || !b.tournament || b.tournament === tournament.id)
    .slice(0, 3);
  if (!battles.length) {
    panel.hidden = true;
    return;
  }
  panel.hidden = false;
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
}

function renderUpcoming(tournament) {
  const list = el("upcoming");
  if (!tournament) {
    list.innerHTML = "";
    return;
  }
  const next = tournament.matches
    .filter((m) => m.status === "READY" || (m.status === "PENDING" && (m.teamA > 0 || m.teamB > 0)))
    .sort((a, b) => (a.status === b.status ? a.id - b.id : a.status === "READY" ? -1 : 1))
    .slice(0, 6);
  list.innerHTML = next.length
    ? next.map((m) => `<li><span class="label">${esc(tournament.labels[String(m.id)] || "")}</span>${esc(teamName(tournament, m.teamA))} — ${esc(teamName(tournament, m.teamB))}</li>`).join("")
    : '<p class="empty">Aucun match en attente.</p>';
}

function renderRanking(tournament) {
  const panel = el("ranking-panel");
  if (!tournament || !tournament.ranking.length) {
    panel.hidden = true;
    return;
  }
  panel.hidden = false;
  const byRank = (rank) => tournament.ranking.filter((r) => r.rank === rank).map((r) => teamName(tournament, r.team)).join(" / ");
  const rest = tournament.ranking.filter((r) => r.rank > 3);
  el("ranking").innerHTML = `
    <div class="podium">
      <div class="p2"><span class="rank">2</span>${esc(byRank(2))}</div>
      <div class="p1"><span class="rank">1</span>${esc(byRank(1))}</div>
      <div class="p3"><span class="rank">3</span>${esc(byRank(3))}</div>
    </div>
    <ol class="ranking-rest">${rest.map((r) => `<li value="${r.rank}">${esc(teamName(tournament, r.team))}</li>`).join("")}</ol>`;
}

function render() {
  const tournament = currentTournament();
  renderHeader(tournament);
  renderBoard(tournament);
  renderLive(tournament);
  renderRecent(tournament);
  renderUpcoming(tournament);
  renderRanking(tournament);
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

if (rotateSeconds > 0) {
  setInterval(() => {
    const tournament = currentTournament();
    if (!tournament) return;
    const tabs = boardTabs(tournament);
    if (tabs.length < 2) return;
    const index = tabs.findIndex((t) => t.id === activeTab);
    activeTab = tabs[(index + 1) % tabs.length].id;
    userPickedTab = true;
    render();
  }, rotateSeconds * 1000);
}

tickClock();
setInterval(tickClock, 10000);
fetchState().catch(() => {}).finally(connectEvents);
