// Diplômes du tournoi : une page A4 par joueur, à imprimer depuis le navigateur. Les données viennent
// de l'état public du serveur (/api/state) : résultats des matchs (joueurs, classes, bilans, hauts
// faits), classement final et noms des équipes.
//   ?tournament=<id> choisit le tournoi (par défaut : le dernier terminé, sinon le premier).

const params = new URLSearchParams(location.search);

function esc(text) {
  return String(text).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

function placeLabel(rank) {
  return rank === 1 ? "1re place" : rank + "e place";
}

function chooseTournament(state) {
  const list = state.tournaments || [];
  const wanted = parseInt(params.get("tournament") || "0", 10);
  return list.find((t) => t.id === wanted) || [...list].reverse().find((t) => t.status === "FINISHED") || list[0];
}

// Bilan de chaque joueur sur le tournoi, d'après les résultats des matchs.
function playerSummaries(tournament) {
  const players = new Map();
  for (const match of tournament.matches) {
    const result = match.result;
    if (!result || !result.players) continue;
    for (const p of result.players) {
      const team = p.side === 1 ? match.teamA : match.teamB;
      const key = team + "|" + p.name;
      if (!players.has(key)) {
        players.set(key, { name: p.name, team, matches: 0, wins: 0, classes: new Set(), dealt: 0, healed: 0, shielded: 0, kills: 0, mvp: 0, badges: new Map() });
      }
      const s = players.get(key);
      s.matches++;
      if (result.winner === team) s.wins++;
      if (p.class) s.classes.add(p.class);
      s.dealt += p.dealt || 0;
      s.healed += p.healed || 0;
      s.shielded += p.shielded || 0;
      s.kills += p.kills || 0;
      if (p.mvp) s.mvp++;
      for (const badge of p.badges || []) s.badges.set(badge, (s.badges.get(badge) || 0) + 1);
    }
  }
  return [...players.values()];
}

function render(state) {
  const tournament = chooseTournament(state);
  const box = document.getElementById("diplomas");
  if (!tournament) {
    document.getElementById("summary").textContent = "Aucun tournoi sur ce serveur.";
    return;
  }
  const teamName = (id) => (tournament.teamNames || {})[String(id)] || "Équipe " + id;
  const rankOf = (team) => {
    const entry = (tournament.ranking || []).find((r) => r.team === team);
    return entry ? entry.rank : 0;
  };
  const badgeInfo = new Map((tournament.badges || []).map((b) => [b.id, b]));
  const players = playerSummaries(tournament);
  // Classement de l'équipe d'abord, puis par nom.
  players.sort((a, b) => (rankOf(a.team) || 99) - (rankOf(b.team) || 99) || a.name.localeCompare(b.name, "fr"));

  document.getElementById("summary").textContent =
    `${tournament.name} : ${players.length} diplôme${players.length > 1 ? "s" : ""}` +
    (tournament.status === "FINISHED" ? "" : " (tournoi en cours : résultats provisoires)");

  const date = new Date().toLocaleDateString("fr-FR", { day: "numeric", month: "long", year: "numeric" });
  box.innerHTML = players.map((p) => {
    const rank = rankOf(p.team);
    const badges = [...p.badges.entries()].map(([id, count]) => {
      const info = badgeInfo.get(id);
      return `<li><strong>${esc(info ? info.name : id)}</strong>${count > 1 ? ` <span class="times">x${count}</span>` : ""}
        ${info && info.description ? `<span class="desc">${esc(info.description)}</span>` : ""}</li>`;
    }).join("");
    return `<section class="diploma">
      <div class="frame">
        <div class="brand">Tactical War</div>
        <div class="event">${esc(tournament.name)}</div>
        <h1>Diplôme</h1>
        <div class="given">décerné à</div>
        <div class="name">${esc(p.name)}</div>
        <div class="team"><strong>${esc(teamName(p.team))}</strong>${rank ? ` · ${placeLabel(rank)} du tournoi` : ""}</div>
        <div class="stats">
          <div><span class="value">${p.matches}</span><span class="label">match${p.matches > 1 ? "s" : ""} joué${p.matches > 1 ? "s" : ""}</span></div>
          <div><span class="value">${p.wins}</span><span class="label">victoire${p.wins > 1 ? "s" : ""}</span></div>
          <div><span class="value">${p.dealt}</span><span class="label">dégâts infligés</span></div>
          <div><span class="value">${p.healed + p.shielded}</span><span class="label">soins et boucliers</span></div>
          <div><span class="value">${p.kills}</span><span class="label">KO</span></div>
          <div><span class="value">${p.mvp}</span><span class="label">titre${p.mvp > 1 ? "s" : ""} de MVP</span></div>
        </div>
        <div class="classes">Classes jouées : ${[...p.classes].map(esc).join(", ") || "-"}</div>
        <div class="badges">
          <h2>Hauts faits</h2>
          ${badges ? `<ul>${badges}</ul>` : `<p class="none">La prochaine fois !</p>`}
        </div>
        <div class="footer"><span>Le ${esc(date)}</span><span class="sign">L'organisateur</span></div>
      </div>
    </section>`;
  }).join("");
}

fetch("/api/state")
  .then((response) => response.json())
  .then(render)
  .catch(() => { document.getElementById("summary").textContent = "Serveur injoignable."; });
