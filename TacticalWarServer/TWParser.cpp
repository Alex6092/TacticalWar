#include "TWParser.h"
#include <iostream>
#include <cstdlib>
#include <stdexcept>

#include <StringUtils.h>
#include <PlayerManager.h>
#include <Match.h>
#include <EnvironmentManager.h>
#include <JsonFile.h>
#include <Message.h>
#include <Opcodes.h>
#include <PasswordHasher.h>


TWParser::TWParser(const tw::ServerConfig & config)
	: config(config),
	teamStore(tw::store::joinPath(config.dataDir, "teams.json")),
	credentials(tw::store::joinPath(config.dataDir, "exports/credentials.json"), tw::store::joinPath(config.dataDir, "exports/fiches-equipes.html")),
	teamStoreReadOnly(false),
	profiles(tw::store::joinPath(config.dataDir, "profiles.json")),
	tournaments(config.dataDir),
	replays(tw::store::joinPath(config.dataDir, "replays"))
{
	srand((unsigned int)time(NULL));
	net = NULL;
	admin = NULL;
	nextSessionId = 1;
	adminWatchedTournament = 0;
	http = NULL;
	publicDirty = true;
	lastPublicPublish = 0;

	loadEnvironments();
	loadTeams();
	std::string profileError;
	if (!profiles.load(&profileError))
		std::cout << "Profils des joueurs illisibles (data/profiles.json) : " << profileError << std::endl;

	std::string error;
	if (!gameData.loadFromFile("./assets/data/gamedata.json", error))
		throw std::runtime_error(error);
	std::cout << gameData.classes.size() << " classes chargées." << std::endl;

	loadTournaments();
}

void TWParser::loadEnvironments()
{
	std::vector<int> envIds = tw::EnvironmentManager::getInstance()->getAlreadyExistingIds();
	for (int i = 0; i < envIds.size(); i++)
	{
		tw::Environment * e = tw::EnvironmentManager::getInstance()->loadEnvironment(envIds[i]);
		if (e != NULL)
		{
			environments.push_back(e);
			if (e->isInTournamentPool())
				tournamentEnvironments.push_back(e);
			mapMessages[e->getId()] = "MP" + tw::EnvironmentManager::toJson(e, true, true) + "\n";
		}
	}

	if (tournamentEnvironments.empty())
		tournamentEnvironments = environments;
	std::cout << environments.size() << " carte(s) chargée(s), dont " << tournamentEnvironments.size() << " pour les tournois." << std::endl;
}

TWParser::~TWParser()
{
}

bool TWParser::isAuthorized(ClientState * client, const std::string & op)
{
	const tw::protocol::OpcodeInfo * info = tw::protocol::findOpcode(op.c_str());
	if (info == NULL || info->direction == tw::protocol::Direction::SERVER_TO_CLIENT)
		return false;

	switch (info->requiredRole)
	{
	case tw::protocol::Role::ANY:
		return true;
	case tw::protocol::Role::SPECTATOR:
		return client->isAdmin() || getPlayerFromClientState(client) != NULL
			|| std::find(spectatorModeClientDiffusionList.begin(), spectatorModeClientDiffusionList.end(), client) != spectatorModeClientDiffusionList.end();
	case tw::protocol::Role::PLAYER:
		return getPlayerFromClientState(client) != NULL;
	case tw::protocol::Role::ADMIN:
		return client->isAdmin();
	}
	return false;
}

bool TWParser::isAdminLoginAllowed(ClientState * client)
{
	const std::vector<std::string> & allowed = config.admin.allowedFrom;
	return allowed.empty() || std::find(allowed.begin(), allowed.end(), client->getRemoteAddress()) != allowed.end();
}

void TWParser::handleMessage(ClientState * client, const std::string & toParse)
{
	bool spectatorMode = false;

	std::string op = toParse.substr(0, 2);
	if (!isAuthorized(client, op))
	{
		std::cout << "Message " << op << " refuse pour " << client->getRemoteAddress() << std::endl;
		return;
	}

	// Mode spectateur (contenu JSON) :
	if (op == "SL" || op == "SW" || op == "SU" || op == "RL" || op == "RP" || (op == "BR" && getPlayerFromClientState(client) == NULL))
	{
		tw::protocol::Message message;
		nlohmann::json body = nlohmann::json::object();
		if (tw::protocol::Message::decode(toParse, message) && message.hasJsonPayload())
			message.parseJson(body);

		if (op == "BR")
		{
			BattleSession * watched = spectatedSession(client);
			if (watched != NULL)
				sendBattleState(watched, client, NULL, false);
		}
		else
		{
			handleSpectatorMessage(client, op, body);
		}
		return;
	}

	// Actions de combat (contenu JSON) :
	if (op == "CP" || op == "Cs" || op == "Cm" || op == "CL" || op == "Ct" || op == "CE" || op == "CG" || op == "BR")
	{
		tw::protocol::Message message;
		nlohmann::json body = nlohmann::json::object();
		if (tw::protocol::Message::decode(toParse, message) && message.hasJsonPayload())
			message.parseJson(body);
		if (op == "CG")
			handlePing(client, body);
		else
			handleBattleAction(client, op, body);
		return;
	}

	// Défis entre équipes (contenu JSON) :
	if (op == "DL" || op == "DD" || op == "DA")
	{
		tw::protocol::Message message;
		nlohmann::json body = nlohmann::json::object();
		if (tw::protocol::Message::decode(toParse, message) && message.hasJsonPayload())
			message.parseJson(body);
		handleChallengeMessage(client, op, body);
		return;
	}

	// Matchs amicaux de l'admin (contenu JSON) :
	if (op == "FL" || op == "FC" || op == "FX")
	{
		tw::protocol::Message message;
		nlohmann::json body = nlohmann::json::object();
		if (tw::protocol::Message::decode(toParse, message) && message.hasJsonPayload())
			message.parseJson(body);
		handleFriendlyAdminMessage(client, op, body);
		return;
	}

	// Administration des tournois (contenu JSON) :
	if (op == "UL" || op == "UG" || op == "UC" || op == "UE" || op == "UB" || op == "UP" || op == "UD" || op == "UF" || op == "US" || op == "UX")
	{
		tw::protocol::Message message;
		nlohmann::json body = nlohmann::json::object();
		if (tw::protocol::Message::decode(toParse, message) && message.hasJsonPayload())
			message.parseJson(body);
		handleTournamentAdminMessage(client, op, body);
		return;
	}

	// Messages au format JSON (administration des équipes) :
	if (op == "TC" || op == "TU" || op == "TD" || op == "TA" || op == "TK" || op == "TI")
	{
		tw::protocol::Message message;
		nlohmann::json body = nlohmann::json::object();
		if (tw::protocol::Message::decode(toParse, message) && message.hasJsonPayload() && !message.parseJson(body))
		{
			sendTeamResult(client, false, "Requête invalide.");
			return;
		}
		handleTeamAdminMessage(client, op, body);
		return;
	}

	{
		// Connexion d'un client (login joueur ou spectateur)
		if (StringUtils::startsWith(toParse, "HG"))
		{
			std::string payload = toParse.substr(2);

			bool wrongIds = false;

			// Connexion joueur :
			if (payload.length() > 0)
			{
				std::vector<std::string> data = StringUtils::explode(payload, ';');
				if (data.size() >= 2)
				{
					std::string pseudo = data[0];
					std::string password = data[1];

					if (pseudo == config.admin.login)
					{
						if (!isAdminLoginAllowed(client) || !tw::PasswordHasher::verify(password, config.admin.passwordHash))
						{
							std::cout << "Connexion admin refusee depuis " << client->getRemoteAddress() << std::endl;
							wrongIds = true;
						}
						else
						{
							if (admin != NULL)
							{
								kick(admin);
							}

							client->setIsAdmin(true);
							admin = client;
							send(client, "AD\n");
							notifyPlanifiedAndPlayingMatch(admin);
							notifyFinishedMatch(admin);
							notifyTeamList(admin);
							notifyFriendlyMatches(admin);
						}
					}
					else if (teamStore.authenticate(pseudo, password, &pseudo) && playersMap.find(pseudo) != playersMap.end())
					{
						tw::Player * p = playersMap[pseudo];

						{
							std::cout << "Connexion du joueur " << pseudo.c_str() << std::endl;

							// Si compte déjà utilisé : déconnexion du client précédent
							if (connectedPlayerMap.find(p) != connectedPlayerMap.end())
							{
								std::cout << "Compte deja utilise, kick du client precedent" << std::endl;
								// Kick :
								kick(connectedPlayerMap[p]);
							}

							client->setPseudo(pseudo);
							connectedPlayerMap[p] = client;

							sendGameData(client);
							sendAppearances(client, pseudo);

							tw::Match * match = tw::PlayerManager::getCurrentOrNextMatchForPlayer(p);
							// Un match existe pour ce joueur :
							if (match != NULL)
							{
								BattleSession * session = sessionOfMatch(match);
								p->setHasJoinBattle(true);

								if (session != NULL && session->getPhase() == BattleSession::Phase::BATTLE)
								{
									// Retour en combat (reconnexion) :
									sendBattleState(session, client, p, true);
									onPlayerConnectionChanged(p, true);
								}
								else
								{
									// Envoi vers l'écran de choix de classe (avec la classe déjà verrouillée, s'il y en a une) :
									send(client, classSelectionMessage(p));
									if (session != NULL)
										sendBanState(session, client, p);
									if (session != NULL && session->chosenClass(p) != 0)
										send(client, "PO" + std::to_string(session->chosenClass(p)) + "\n");
								}

								notifyMatchConnectedPlayerChanged(match);
							}
							else
							{
								// Aucun match pour le moment :
								// Envoi vers l'écran d'attente de match
								send(client, "HW\n");
							}

							notifyTeamList(admin);
						}
					}
					else wrongIds = true;
				}
				else if (data.size() >= 1)
				{
					wrongIds = true;
				}
				else
					spectatorMode = true;
			}
			else // Connexion spectateur
				spectatorMode = true;

			if (wrongIds)
			{
				send(client, "HK\n");	// Kick
			}
			else if (spectatorMode)
			{
				spectatorModeClientDiffusionList.push_back(client);
				send(client, "HS\n");
				sendGameData(client);
				notifySessionList(client);
				
				notifyPlayingMatchList();
			}
		}
		// Demande de la liste des matchs en cours :
		else if (StringUtils::startsWith(toParse, "ML"))
		{
			notifyPlayingMatchList(client);
		}
		// Demande de la liste des équipes :
		else if (StringUtils::startsWith(toParse, "TL"))
		{
			notifyTeamList(client);
		}
		// Demande de la liste des match créés (planifiés et en cours) :
		else if (StringUtils::startsWith(toParse, "MC"))
		{
			notifyPlanifiedAndPlayingMatch(client);
			notifyFinishedMatch(client);
		}
		// Demande la création d'un match :
		else if (StringUtils::startsWith(toParse, "CM"))
		{
			// Seul un administrateur est autorisé à réaliser cette opération :
			if (client->isAdmin())
			{
				// Ancien format CM<nom>;<équipe A>;<équipe B> (carte au hasard).
				std::vector<std::string> matchData = StringUtils::explode(toParse.substr(2), ';');
				if (matchData.size() >= 3)
				{
					int team1 = std::atoi(matchData[1].c_str());
					int team2 = std::atoi(matchData[2].c_str());
					std::string error;
					if (team1 == team2)
						send(client, "CF\n");
					else if (createFriendlyMatch(matchData[0], team1, team2, 0, error) != NULL)
						send(client, "CO\n");
					else
						send(client, "CN\n");
				}
			}
		}
		// Choix de la classe :
		else if (StringUtils::startsWith(toParse, "PC"))
		{
			tw::Player * p = getPlayerFromClientState(client);
			if (p != NULL)
				handlePickClass(client, p, toParse.substr(2));
		}
		// Énigmes réussies sur le poste du joueur (apparences) :
		else if (StringUtils::startsWith(toParse, "PZ"))
		{
			tw::Player * p = getPlayerFromClientState(client);
			if (p != NULL)
				handlePuzzles(client, p, toParse.substr(2));
		}
		// Classe regardée sur l'écran de choix (montrée au coéquipier) :
		else if (StringUtils::startsWith(toParse, "PV"))
		{
			tw::Player * p = getPlayerFromClientState(client);
			if (p != NULL)
				handleViewClass(client, p, toParse.substr(2));
		}
		// Bannissement d'une classe :
		else if (StringUtils::startsWith(toParse, "PB"))
		{
			tw::Player * p = getPlayerFromClientState(client);
			if (p != NULL)
				handleBan(client, p, toParse.substr(2));
		}
	}
}

//	Valeur de retour = Code d'erreur
//		0	: La team existe et est disponible.
//		-1	: La team existe mais est déjà en attente pour commencer un match
//		-2	: La team n'existe pas
int TWParser::isTeamAvailableForMatchCreation(int teamId)
{
	int result = 0;

	std::map<int, std::vector<tw::Player*>>::iterator it = teamIdToPlayerList.find(teamId);

	// La team existe
	if (it != teamIdToPlayerList.end())
	{
		std::vector<tw::Match *> matchs = tw::PlayerManager::getAllMatchsForPlayer((*it).second[0]);

		for (int i = 0; i < matchs.size(); i++)
		{
			tw::Match * match = matchs[i];
			// Il y a déjà un match en attente de démarrage pour cette équipe ...
			if (match->getStatus() == tw::MatchStatus::NOT_STARTED || match->getStatus() == tw::MatchStatus::STARTED)
			{
				result = -1;
				break;
			}
		}
	}
	else
	{
		// La team n'existe pas :
		result = -2;
	}

	return result;
}

void TWParser::notifyMatchCreated(tw::Match * m)
{
	notifyPlayingMatchList();
	notifyPlanifiedAndPlayingMatch(admin);
	notifyFinishedMatch(admin);
	
	// Switch the connected player to the class selection screen :
	notifySwitchToClassSelectionToConnectedPlayer(m->getTeam1());
	notifySwitchToClassSelectionToConnectedPlayer(m->getTeam2());

	// Notify the match players status :
	notifyMatchConnectedPlayerChanged(m);
}

void TWParser::notifySwitchToClassSelectionToConnectedPlayer(std::vector<tw::Player*> team)
{
	for (int i = 0; i < team.size(); i++)
	{
		tw::Player * p = team[i];
		// Notification des clients déjà connectés (entrée en mode choix de classe)
		if (connectedPlayerMap.find(p) != connectedPlayerMap.end())
		{
			ClientState * client = connectedPlayerMap[p];
			send(client, classSelectionMessage(p));
			p->setHasJoinBattle(true);
		}
	}
}

void TWParser::notifyPlanifiedAndPlayingMatch(ClientState * c)
{
	// Envoi de la liste des matchs en cours
	std::vector<tw::Match*> playingMatch = tw::PlayerManager::getPlanifiedAndPlayingMatchs();

	std::string matchData = "";

	for (int i = 0; i < playingMatch.size(); i++)
	{
		if (i != 0)
			matchData += ';';
		matchData += playingMatch[i]->serialize();
	}

	matchData = "MC" + matchData + '\n';

	// Si envoi à un client spécifique, envoi uniquement au client passé en paramètre
	if (c != NULL)
	{
		send(c, matchData);
	}
}

void TWParser::notifyFinishedMatch(ClientState * c)
{
	// Envoi de la liste des matchs en cours
	std::vector<tw::Match*> playingMatch = tw::PlayerManager::getFinishedMatchs();

	std::string matchData = "";

	for (int i = 0; i < playingMatch.size(); i++)
	{
		if (i != 0)
			matchData += ';';
		matchData += playingMatch[i]->serialize();
	}

	matchData = "MF" + matchData + '\n';

	// Si envoi à un client spécifique, envoi uniquement au client passé en paramètre
	if (c != NULL)
	{
		send(c, matchData);
	}
}

void TWParser::notifyTeamList(ClientState * c)
{
	// Liste réservée à l'admin : elle contient les logins et les mots de passe connus.
	if (c == NULL || !c->isAdmin())
		return;

	nlohmann::json teams = nlohmann::json::array();
	for (const tw::Team & team : teamStore.getTeams())
	{
		nlohmann::json teamJson = tw::teamToJson(team, false);
		for (std::size_t i = 0; i < team.players.size(); i++)
		{
			const std::string & login = team.players[i].login;
			std::map<std::string, tw::Player*>::iterator it = playersMap.find(login);
			bool connected = it != playersMap.end() && getClientStateFromPlayer(it->second) != NULL;

			teamJson["players"][i]["connected"] = connected;
			teamJson["players"][i]["password"] = credentials.get(login);
		}
		teamJson["busy"] = teamHasPendingMatch(team.id);
		teams.push_back(teamJson);
	}

	nlohmann::json body = {
		{ "teams", teams },
		{ "readOnly", teamStoreReadOnly },
		{ "credentialSheet", credentials.getHtmlPath() }
	};

	send(c, tw::protocol::Message::encode("TL", body));
}

void TWParser::notifyPlayingMatchList(ClientState * c)
{
	// Envoi de la liste des matchs en cours
	std::vector<tw::Match*> playingMatch = tw::PlayerManager::getCurrentlyPlayingMatchs();
	
	std::string matchData = "";

	for (int i = 0; i < playingMatch.size(); i++)
	{
		if (i != 0)
			matchData += ';';
		matchData += playingMatch[i]->serialize();
	}

	matchData = "ML" + matchData + '\n';

	// Si envoi à un client spécifique, envoi uniquement au client passé en paramètre
	if (c != NULL)
	{
		send(c, matchData);
	}
	// Envoi à tout le monde (mise à jour de la liste suite à une modif)
	else
	{
		for (int i = 0; i < spectatorModeClientDiffusionList.size(); i++)
		{
			send(spectatorModeClientDiffusionList[i], matchData);
		}
	}
}

void TWParser::setNetServer(tw::net::NetServer * net)
{
	this->net = net;
}

void TWParser::send(ClientState * client, const std::string & data)
{
	if (net != NULL && client != NULL)
		net->send(client->getConnId(), data);
}

void TWParser::kick(ClientState * client)
{
	// La connexion est fermée plus tard par la boucle réseau : on détache tout de suite
	// le client pour que le compte puisse être repris par la nouvelle connexion.
	if (client == admin)
		admin = NULL;

	std::vector<ClientState*>::iterator it = std::find(spectatorModeClientDiffusionList.begin(), spectatorModeClientDiffusionList.end(), client);
	if (it != spectatorModeClientDiffusionList.end())
		spectatorModeClientDiffusionList.erase(it);

	client->setIsAdmin(false);
	client->setPseudo("");

	if (net != NULL)
		net->close(client->getConnId());
}

void TWParser::onConnected(tw::net::ConnId id, const std::string & remoteAddress)
{
	std::cout << "Client connecte (" << remoteAddress << ")" << std::endl;
	clients[id] = new ClientState(id, remoteAddress);
}

void TWParser::onMessage(tw::net::ConnId id, const std::string & line)
{
	std::map<tw::net::ConnId, ClientState*>::iterator it = clients.find(id);
	if (it != clients.end())
		handleMessage(it->second, line);
}

void TWParser::onTick(tw::net::Clock::time_point now)
{
	tickBattles();
	tickReplays(nowMs());
	tickChallenges(nowMs());
}

void TWParser::onDisconnected(tw::net::ConnId id)
{
	std::map<tw::net::ConnId, ClientState*>::iterator clientIt = clients.find(id);
	if (clientIt == clients.end())
		return;

	ClientState * client = clientIt->second;
	clients.erase(clientIt);

	if (client == admin)
	{
		admin = NULL;
	}

	// If spectator, clear it from the spectator diffusion list :
	std::vector<ClientState*>::iterator it = std::find(spectatorModeClientDiffusionList.begin(), spectatorModeClientDiffusionList.end(), client);
	if (it != spectatorModeClientDiffusionList.end())
	{
		spectatorModeClientDiffusionList.erase(it);
	}
	removeSpectator(client);
	playbacks.erase(client->getConnId());

	// Clear connected player map (only if this connection is still the one bound to the account) :
	if (client->getPseudo().length() > 0 && playersMap.find(client->getPseudo()) != playersMap.end())
	{
		tw::Player * p = playersMap[client->getPseudo()];

		if (getClientStateFromPlayer(p) == client)
		{
			p->setHasJoinBattle(false);

			onPlayerConnectionChanged(p, false);

			connectedPlayerMap.erase(p);
			notifyMatchConnectedPlayerChanged(tw::PlayerManager::getCurrentOrNextMatchForPlayer(p));
			notifyTeamList(admin);
			std::cout << "Client " << p->getPseudo().c_str() << " disconnected ..." << std::endl;
		}
	}

	delete client;
}


void TWParser::onMatchStatusChanged(tw::Match * match, tw::MatchStatus oldStatus, tw::MatchStatus newStatus)
{
	// Notify spectator mode clients
	notifyPlayingMatchList();

	// Notify admin
	notifyPlanifiedAndPlayingMatch(admin);
	notifyFinishedMatch(admin);
	for (const FriendlyMatch & friendly : friendlyMatches)
	{
		if (friendly.match == match)
		{
			notifyFriendlyMatches();
			break;
		}
	}
}

tw::Match * TWParser::createFriendlyMatch(const std::string & rawName, int teamA, int teamB, int mapId, std::string & error)
{
	std::string name = rawName;
	name.erase(0, name.find_first_not_of(" \t"));
	name.erase(name.find_last_not_of(" \t") + 1);
	if (name.empty())
		name = teamName(teamA) + " - " + teamName(teamB);
	if (name.size() > 60)
		name.resize(60);
	if (teamA == teamB)
	{
		error = u8"Choisissez deux équipes différentes.";
		return NULL;
	}
	for (int team : { teamA, teamB })
	{
		auto players = teamIdToPlayerList.find(team);
		if (players == teamIdToPlayerList.end() || players->second.size() < 2)
		{
			error = u8"Équipe inconnue ou incomplète : " + std::to_string(team) + ".";
			return NULL;
		}
		if (isTeamAvailableForMatchCreation(team) != 0)
		{
			error = teamName(team) + u8" a déjà un match prévu ou en cours.";
			return NULL;
		}
	}
	tw::Environment * environment = NULL;
	for (tw::Environment * candidate : environments)
	{
		if (candidate->getId() == mapId)
			environment = candidate;
	}
	if (mapId != 0 && environment == NULL)
	{
		error = u8"Carte inconnue : " + std::to_string(mapId) + ".";
		return NULL;
	}
	if (environment == NULL)
	{
		if (environments.empty())
		{
			error = u8"Aucune carte chargée.";
			return NULL;
		}
		environment = environments[rand() % environments.size()];
	}

	tw::Match * match = new tw::Match(name);
	std::vector<tw::Player*> & first = teamIdToPlayerList[teamA];
	std::vector<tw::Player*> & second = teamIdToPlayerList[teamB];
	match->setTeam1Players(first[0], first[1]);
	match->setTeam2Players(second[0], second[1]);
	match->setEnvironment(environment);
	match->addEventListener(this);
	tw::PlayerManager::addMatch(match);

	FriendlyMatch friendly;
	friendly.id = nextFriendlyId++;
	friendly.name = name;
	friendly.teamA = teamA;
	friendly.teamB = teamB;
	friendly.mapId = environment->getId();
	friendly.match = match;
	friendlyMatches.push_back(friendly);

	createSession(match);
	notifyMatchCreated(match);
	notifyFriendlyMatches();
	return match;
}

nlohmann::json TWParser::friendlyListJson()
{
	nlohmann::json matches = nlohmann::json::array();
	// Les plus récents d'abord.
	for (auto it = friendlyMatches.rbegin(); it != friendlyMatches.rend(); ++it)
	{
		const FriendlyMatch & friendly = *it;
		std::string status = "planned";
		if (friendly.cancelled)
			status = "cancelled";
		else if (friendly.match->getStatus() == tw::MatchStatus::FINISHED)
			status = "finished";
		else if (friendly.match->getStatus() == tw::MatchStatus::STARTED)
			status = "playing";
		BattleSession * session = friendly.cancelled ? NULL : sessionOfMatch(friendly.match);
		int winner = status == "finished" ? friendly.match->getWinnerTeamId() : 0;
		std::string mapName;
		for (tw::Environment * environment : environments)
		{
			if (environment->getId() == friendly.mapId)
				mapName = environment->getName();
		}
		matches.push_back({
			{ "id", friendly.id },
			{ "name", friendly.name },
			{ "teamA", { { "id", friendly.teamA }, { "name", teamName(friendly.teamA) } } },
			{ "teamB", { { "id", friendly.teamB }, { "name", teamName(friendly.teamB) } } },
			{ "map", { { "id", friendly.mapId }, { "name", mapName } } },
			{ "status", status },
			{ "winner", winner == 1 ? friendly.teamA : winner == 2 ? friendly.teamB : 0 },
			{ "session", session != NULL && status != "finished" ? session->getId() : 0 }
		});
	}
	nlohmann::json maps = nlohmann::json::array();
	for (tw::Environment * environment : environments)
		maps.push_back({ { "id", environment->getId() }, { "name", environment->getName() } });
	return { { "matches", matches }, { "maps", maps } };
}

void TWParser::notifyFriendlyMatches(ClientState * only)
{
	ClientState * target = only != NULL ? only : admin;
	if (target != NULL)
		send(target, tw::protocol::Message::encode("FL", friendlyListJson()));
}

void TWParser::handleFriendlyAdminMessage(ClientState * client, const std::string & op, const nlohmann::json & body)
{
	if (!client->isAdmin())
		return;
	if (op == "FL")
	{
		notifyFriendlyMatches(client);
		return;
	}

	std::string error;
	std::string success;
	if (op == "FC")
	{
		tw::Match * match = createFriendlyMatch(body.value("name", std::string()), body.value("teamA", 0), body.value("teamB", 0),
			body.value("map", 0), error);
		if (match != NULL)
			success = u8"Match créé : " + match->getMatchName() + u8". Les joueurs connectés passent au choix des classes.";
	}
	else if (op == "FX")
	{
		int id = body.value("id", 0);
		FriendlyMatch * found = NULL;
		for (FriendlyMatch & friendly : friendlyMatches)
		{
			if (friendly.id == id)
				found = &friendly;
		}
		if (found == NULL)
			error = u8"Match amical inconnu.";
		else if (found->cancelled || found->match->getStatus() == tw::MatchStatus::FINISHED)
			error = u8"Ce match est déjà terminé.";
		else
		{
			BattleSession * session = sessionOfMatch(found->match);
			found->cancelled = true;
			if (session != NULL)
				cancelSession(session);
			else
				found->match->setWinnerTeam(0);
			success = u8"Match annulé : " + found->name + ".";
		}
	}
	send(client, tw::protocol::Message::encode("FR", { { "ok", error.empty() }, { "message", error.empty() ? success : error } }));
	notifyFriendlyMatches(client);
}

void TWParser::sendToMatch(tw::Match * match, std::string str)
{
	std::vector<tw::Player*> players = match->getPlayers();
	for (int i = 0; i < players.size(); i++)
	{
		ClientState * c = getClientStateFromPlayer(players[i]);
		if (c != NULL)
		{
			send(c, str);
		}
	}
}


void TWParser::notifyMatchConnectedPlayerChanged(tw::Match * match)
{
	if (match != NULL)
	{
		std::vector<tw::Player*> team1 = match->getTeam1();
		std::vector<tw::Player*> team2 = match->getTeam2();

		std::string playerStatus;
		std::vector<tw::Player*> diffusionList;

		// Joueurs des deux équipes ; le second personnage d'un joueur seul n'est pas un joueur à attendre.
		std::vector<tw::Player*> everyone = team1;
		everyone.insert(everyone.end(), team2.begin(), team2.end());
		for (tw::Player * p : everyone)
		{
			if (isStandIn(p))
				continue;
			if (!playerStatus.empty())
				playerStatus += ";";
			playerStatus += p->getPseudo() + "," + std::to_string((p->getHasJoinBattle() ? 1 : 0));

			if (p->getHasJoinBattle())
				diffusionList.push_back(p);
		}

		playerStatus = "PS" + playerStatus + "\n";

		// Notify online players :
		for (int i = 0; i < diffusionList.size(); i++)
		{
			ClientState * c = getClientStateFromPlayer(diffusionList[i]);
			if (c != NULL)
			{
				send(c, playerStatus);
			}
		}

		// Choix de classe en cours : chacun retrouve l'état de son coéquipier (arrivée, départ).
		BattleSession * session = sessionOfMatch(match);
		if (session != NULL)
		{
			for (tw::Player * participant : session->getParticipants())
				sendTeammateStates(session, participant);
		}
	}
}