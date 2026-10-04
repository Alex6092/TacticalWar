// État public publié pour la vue projetée (page web) : tournois et combats en cours.
// Aucun login ni mot de passe : uniquement les noms d'équipes et les noms affichés des joueurs.
#include "TWParser.h"

#include <chrono>
#include <ctime>

#include "http/HttpFrontend.h"
#include <Message.h>

void TWParser::setHttpFrontend(HttpFrontend * http)
{
	this->http = http;
	publicDirty = true;
}

bool TWParser::isStandIn(tw::Player * player) const
{
	std::map<int, tw::Player*>::const_iterator it = player != NULL ? standIns.find(player->getTeamNumber()) : standIns.end();
	return it != standIns.end() && it->second == player;
}

std::string TWParser::displayNameOf(tw::Player * player)
{
	// Second personnage d'un joueur seul : « Léa (2) ».
	if (isStandIn(player))
	{
		const tw::Team * team = teamStore.findTeam(player->getTeamNumber());
		std::string owner = team != NULL ? team->players[0].displayName : std::string("Joueur");
		return owner + " (2)";
	}
	int index = 0;
	const tw::Team * team = teamStore.findTeamByLogin(player->getPseudo(), &index);
	return team != NULL ? team->players[index].displayName : player->getPseudo();
}

nlohmann::json TWParser::publicStateJson()
{
	nlohmann::json tournamentsJson = nlohmann::json::array();
	for (int id : tournaments.ids())
	{
		if (tournaments.find(id)->get().status == tw::tournament::TournamentStatus::DRAFT)
			continue;

		nlohmann::json state = tournamentStateJson(id);
		for (nlohmann::json & match : state["matches"])
			match.erase("sessionId");
		tournamentsJson.push_back(state);
	}

	nlohmann::json live = nlohmann::json::array();
	for (auto & entry : sessions)
	{
		BattleSession * session = entry.second;
		if (session->getPhase() == BattleSession::Phase::ENDED)
			continue;

		tw::Match * match = session->getMatch();
		nlohmann::json battle = {
			{ "session", session->getId() },
			{ "name", match->getMatchName() },
			{ "tournament", session->getTournamentId() },
			{ "match", session->getTournamentMatchId() },
			{ "teams", nlohmann::json::array({ teamName(match->getTeam1()[0]->getTeamNumber()), teamName(match->getTeam2()[0]->getTeamNumber()) }) },
			{ "phase", session->getPhase() == BattleSession::Phase::BAN ? "BAN"
				: session->getPhase() == BattleSession::Phase::CLASS_SELECTION ? "CLASS_SELECTION" : "BATTLE" }
		};
		// Classes interdites à chaque équipe, connues à la fin du bannissement.
		if (session->hasBanPhase() && session->getPhase() != BattleSession::Phase::BAN)
		{
			nlohmann::json forbidden = nlohmann::json::array();
			for (int team = 1; team <= 2; team++)
			{
				const tw::battle::ClassDef * classDef = gameData.findClass(session->forbiddenClass(team));
				forbidden.push_back(classDef != nullptr ? classDef->name : std::string());
			}
			battle["forbidden"] = forbidden;
		}

		if (session->getPhase() == BattleSession::Phase::BATTLE)
		{
			const tw::battle::BattleState & state = session->getEngine()->getState();
			battle["phase"] = tw::battle::toString(state.phase);
			battle["round"] = state.round;
			battle["active"] = state.activeFighterId();

			nlohmann::json fighters = nlohmann::json::array();
			for (const tw::battle::Fighter & fighter : state.fighters)
			{
				const tw::battle::ClassDef * classDef = gameData.findClass(fighter.classId);
				nlohmann::json talents = nlohmann::json::array();
				for (const std::string & id : fighter.talents)
				{
					const tw::battle::TalentDef * talent = gameData.findTalent(id);
					talents.push_back(talent != nullptr ? talent->name : id);
				}
				fighters.push_back({
					{ "id", fighter.id },
					{ "team", fighter.team },
					{ "name", fighter.name },
					{ "className", classDef != nullptr ? classDef->name : std::string() },
					{ "hp", fighter.hp },
					{ "maxHp", fighter.maxHp },
					{ "initialMaxHp", fighter.initialMaxHp() },
					{ "shield", fighter.shield },
					{ "alive", fighter.alive },
					{ "connected", fighter.connected },
					{ "talents", talents }
				});
			}
			battle["fighters"] = fighters;
			if (state.zone.enabled)
				battle["zone"] = { { "scores", { state.zone.scores[1], state.zone.scores[2] } }, { "points", state.zone.pointsToWin } };
		}

		live.push_back(battle);
	}

	nlohmann::json recent = nlohmann::json::array();
	for (const nlohmann::json & battle : recentBattles)
		recent.push_back(battle);

	return {
		{ "generatedAt", (long long)std::time(nullptr) },
		{ "tournaments", tournamentsJson },
		{ "live", live },
		{ "recent", recent }
	};
}

void TWParser::publishPublicState(bool force)
{
	if (http == NULL)
		return;

	std::int64_t now = nowMs();
	bool battlesRunning = false;
	for (auto & entry : sessions)
		battlesRunning = battlesRunning || entry.second->getPhase() == BattleSession::Phase::BATTLE;

	// Au plus 4 publications par seconde ; une par seconde pendant les combats (barres de vie).
	bool due = publicDirty || force || (battlesRunning && now - lastPublicPublish >= 1000);
	if (!due || now - lastPublicPublish < 250)
		return;

	http->publish(tw::protocol::dumpJson(publicStateJson()));
	lastPublicPublish = now;
	publicDirty = false;
}
