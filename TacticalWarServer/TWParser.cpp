#include "TWParser.h"
#include <iostream>

#include "TcpServer.h"
#include <StringUtils.h>
#include <PlayerManager.h>
#include <Match.h>
#include <CharacterFactory.h>
#include <EnvironmentManager.h>
#include <Pathfinder.h>
#include <ZoneAndSightCalculator.h>


TWParser::TWParser()
{	
	initRandom = false;
	loadEnvironments();
	
	players = tw::PlayerManager::loadPlayers();
	std::cout << players.size() << " joueurs charges :" << std::endl;

	// Construct player pseudo to player data map :
	for (int i = 0; i < players.size(); i++)
	{
		std::cout << players[i]->getPseudo().c_str() << " (equipe " << players[i]->getTeamNumber() << ")" << std::endl;
		playersMap[players[i]->getPseudo()] = players[i];
		teamIdToPlayerList[players[i]->getTeamNumber()].push_back(players[i]);
	}

	admin = NULL;
	//tw::PlayerManager::subscribeToAllMatchEvent(this);
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
		}
	}
}

TWParser::~TWParser()
{
}

bool TWParser::hasCompleteMessage(ClientState * client)
{
	bool result = false;
	std::deque<unsigned char> & buffer = client->getBuffer();

	for (int i = 0; i < buffer.size(); i++)
	{
		if (buffer[i] == '\n')
		{
			result = true;
			break;
		}
	}

	return result;
}

std::string TWParser::extractCompleteMessageFromBuffer(ClientState * client)
{
	std::string result;
	int i = 0;
	std::deque<unsigned char> & buffer = client->getBuffer();
	
	while (buffer.front() != '\n')
	{
		result += buffer.front();
		buffer.pop_front();
	}

	buffer.pop_front();	// To remove the '\n' from the buffer.

	return result;
}

void TWParser::parse(ClientState * client, std::vector<unsigned char> & receivedPacket)
{
	if (!initRandom)
	{
		srand(time(NULL));
		initRandom = true;
	}

	bool spectatorMode = false;
	std::deque<unsigned char> & buffer = client->getBuffer();

	for (int i = 0; i < receivedPacket.size(); i++)
	{
		unsigned char c = receivedPacket[i];
		buffer.push_back(c);
	}

	// Implémentation du protocole :
	if (hasCompleteMessage(client))
	{
		std::string toParse = extractCompleteMessageFromBuffer(client);

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

					if (pseudo == "admin" && password == "admin")
					{
						if (admin != NULL)
						{
							kick(admin);
						}

						client->setIsAdmin(true);
						admin = client;
						TcpServer<TWParser, ClientState>::Send(client, (char*)"AD\n", 3);
						notifyPlanifiedAndPlayingMatch(admin);
						notifyFinishedMatch(admin);
						notifyTeamList(admin);
					}
					else if (playersMap.find(pseudo) != playersMap.end())
					{
						tw::Player * p = playersMap[pseudo];

						if (password == p->getPassword())
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

							tw::Match * match = tw::PlayerManager::getCurrentOrNextMatchForPlayer(p);
							// Un match existe pour ce joueur :
							if (match != NULL)
							{
								if (match->getStatus() == tw::MatchStatus::STARTED)
								{
									// Retour en jeu (reconnexion en combat)
									Battle * b = (Battle*)match->getBattlePayload();

									// Retour en combat :
									enterBattleState(b->getMatch(), client);
									p->setHasJoinBattle(true);
									notifyMatchConnectedPlayerChanged(match);
									synchronizeBattleState(b->getMatch(), client);
									
									// TODO : Notify that the player is back.
									//TcpServer<TWParser, ClientState>::Send(client, (char*)"HG\n", 3);
									//TcpServer<TWParser, ClientState>::Send(client, (char*)"CA\n", 3);
									//TcpServer<TWParser, ClientState>::Send(client, (char*)"CS\n", 3);
								}
								else
								{
									// Envoi vers l'écran de choix de classe
									TcpServer<TWParser, ClientState>::Send(client, (char*)"HC\n", 3);
									p->setHasJoinBattle(true);
									notifyMatchConnectedPlayerChanged(match);

									// Si la classe a déjà été verrouillée précédemment :
									if (p->getCharacter() != NULL)
									{
										notifyClassChoiceLocked(client);
									}
								}
							}
							else
							{
								// Aucun match pour le moment :
								// Envoi vers l'écran d'attente de match
								TcpServer<TWParser, ClientState>::Send(client, (char*)"HW\n", 3);
							}
						}
						else
							wrongIds = true;
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
				TcpServer<TWParser, ClientState>::Send(client, (char*)"HK\n", 3);	// Kick
			}
			else if (spectatorMode)
			{
				spectatorModeClientDiffusionList.push_back(client);
				TcpServer<TWParser, ClientState>::Send(client, (char*)"HS\n", 3);
				
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
		}
		// Demande la création d'un match :
		else if (StringUtils::startsWith(toParse, "CM"))
		{
			// Seul un administrateur est autorisé à réaliser cette opération :
			if (client->isAdmin())
			{
				std::string payload = toParse.substr(2);

				std::vector<std::string> matchData = StringUtils::explode(payload, ';');

				std::string name = matchData[0];
				int team1 = std::atoi(matchData[1].c_str());
				int team2 = std::atoi(matchData[2].c_str());

				if (team1 != team2)
				{
					tw::Match * m = new tw::Match(name);

					int team1Status = isTeamAvailableForMatchCreation(team1);
					int team2Status = isTeamAvailableForMatchCreation(team2);

					// Les 2 equipes sont libres pour un match à venir :
					if (team1Status == 0 && team2Status == 0)
					{
						std::vector<tw::Player*> teamA = teamIdToPlayerList[team1];
						std::vector<tw::Player*> teamB = teamIdToPlayerList[team2];

						m->setTeam1Players(teamA[0], teamA[1]);
						m->setTeam2Players(teamB[0], teamB[1]);

						m->setEnvironment(environments[rand() % environments.size()]);

						m->addEventListener(this);
						tw::PlayerManager::addMatch(m);
						notifyMatchCreated(m);
						TcpServer<TWParser, ClientState>::Send(client, (char*)"CO\n", 3);
					}
					else
					{
						TcpServer<TWParser, ClientState>::Send(client, (char*)"CN\n", 3);
					}
				}
				else
				{
					TcpServer<TWParser, ClientState>::Send(client, (char*)"CF\n", 3);
				}
			}
		}
		// Validation du choix de la classe :
		else if (StringUtils::startsWith(toParse, "PC"))
		{
			if (client->getPseudo().size() > 0)
			{
				tw::Player * p = playersMap[client->getPseudo()];

				if (p->getHasJoinBattle())
				{
					std::string classIdStr = toParse.substr(2);
					int classId = std::atoi(classIdStr.c_str());
					std::vector<int> classes = CharacterFactory::getInstance()->getClassesIds();
					// La classe existe :
					if (std::find(classes.begin(), classes.end(), classId) != classes.end())
					{
						if (p->getCharacter() == NULL)
						{
							tw::Match * m = tw::PlayerManager::getCurrentOrNextMatchForPlayer(p);
							if (m != NULL && m->playerIsInThisMatch(p))
							{
								tw::Point2D cell = m->getRandomAvailableCellForPlayer(p);
								bool isTeam1 = m->playerIsInTeam1(p);

								if (cell.getX() != -1 && cell.getY() != -1)
								{
									p->setCharacter(CharacterFactory::getInstance()->constructCharacter(m->getEnvironment(), classId, (isTeam1 ? 1 : 2), cell.getX(), cell.getY(), m));
									p->getCharacter()->setPseudo(p->getPseudo());

									notifyClassChoiceLocked(client);
									
									// Si tout le monde est prêt : démarrage du combat.
									if (everybodyReadyForBattle(m))
									{
										Battle * b = new Battle(m);
										b->addEventListener(this);
										switchParticipantToBattleState(b);
									}
								}
								else
								{
									std::cout << "[ERREUR] Probleme environment : Il n'y a pas de cellule de demarrage pour l'equipe !" << std::endl;
								}
							}
						}
					}
				}
			}
		}
		else if (StringUtils::startsWith(toParse, "Cs"))	// Validation position (joueur prêt)
		{
			if (client->getPseudo().size() > 0)
			{
				tw::Player * p = getPlayerFromClientState(client);
				if (p != NULL && p->getHasJoinBattle() && p->getCharacter() != NULL && !p->getCharacter()->isPlayerReady())
				{
					p->getCharacter()->setReadyStatus(true);
					tw::Match * m = tw::PlayerManager::getCurrentOrNextMatchForPlayer(p);
					if (m != NULL)
					{
						Battle * b = (Battle*)m->getBattlePayload();
						int playerId = b->getIdForPlayer(p);

						std::vector<tw::Player*> players = b->getTimeline();
						for (int i = 0; i < players.size(); i++)
						{
							ClientState * toNotify = getClientStateFromPlayer(players[i]);
							if (toNotify != NULL)
							{
								notifyReadyState(toNotify, playerId, p);
							}
						}


						// Si tous les joueurs sont prêts : Démarrage du combat
						if (m->allPlayersReady())
						{
							if (b != NULL)
							{
								b->enterBattlePhase();
							}
						}
					}
				}
			}
		}
		else if (StringUtils::startsWith(toParse, "CP"))		// Demande un changement de position de départ
		{
			if (client->getPseudo().size() > 0)
			{
				tw::Player * p = getPlayerFromClientState(client);
				if (p != NULL && p->getHasJoinBattle() && p->getCharacter() != NULL && !p->getCharacter()->isPlayerReady())
				{
					tw::Match * m = tw::PlayerManager::getCurrentOrNextMatchForPlayer(p);
					if (m != NULL)
					{
						std::string data = toParse.substr(2);
						std::vector<std::string> positionData = StringUtils::explode(data, ';');
						int cellX = std::atoi(positionData[0].c_str());
						int cellY = std::atoi(positionData[1].c_str());

						Battle * b = (Battle*)m->getBattlePayload();
						if (b != NULL && m->isStartCellAvailableForPlayer(p, cellX, cellY))
						{
							p->getCharacter()->setCurrentX(cellX);
							p->getCharacter()->setCurrentY(cellY);
							int playerId = b->getIdForPlayer(p);

							std::vector<tw::Player*> players = b->getTimeline();
							for (int i = 0; i < players.size(); i++)
							{
								ClientState * c = getClientStateFromPlayer(players[i]);
								if (c != NULL)
								{
									notifyCharacterPositionChanged(c, playerId, p);
								}
							}
						}
					}
				}
			}
		}
		else if (StringUtils::startsWith(toParse, "Cm"))	// Demande de déplacement (mouvement)
		{
			if (client->getPseudo().size() > 0)
			{
				tw::Player * p = getPlayerFromClientState(client);
				if (p != NULL && p->getHasJoinBattle() && p->getCharacter() != NULL && p->getCharacter()->isPlayerReady() && !p->getCharacter()->isMoving())
				{
					tw::Match * m = tw::PlayerManager::getCurrentOrNextMatchForPlayer(p);
					if (m != NULL)
					{
						Battle * b = (Battle*)m->getBattlePayload();
						if (b != NULL && !b->isPreparationPhase())
						{
							// Si c'est le tour du joueur :
							if (b->getActivePlayer() == p)
							{
								std::string data = toParse.substr(2);
								std::vector<tw::Point2D> path = tw::Pathfinder::deserializePath(data);
								
								// Si le personnage a assez de PM :
								if (p->getCharacter()->hasEnoughPM(path.size()))
								{
									// Check si la cellule de début du chemin est adjacente à la celle où se trouve le personnage :
									tw::Point2D firstCell = path.back();
									int characterX = p->getCharacter()->getCurrentX();
									int characterY = p->getCharacter()->getCurrentY();
									if ((firstCell.getX() == characterX && firstCell.getY() == characterY + 1)
										||
										(firstCell.getX() == characterX && firstCell.getY() == characterY - 1)
										||
										(firstCell.getX() == characterX + 1 && firstCell.getY() == characterY)
										||
										(firstCell.getX() == characterX - 1 && firstCell.getY() == characterY)
										)
									{
										// Le déplacement demandé est valide :
										p->getCharacter()->serverSetPath(path);

										std::string str = "Cm" + std::to_string(b->getIdForPlayer(p)) + ";" + data + "\n";
										sendToMatch(m, str);

										// Si plus d'actions possibles, passage du tour automatique :
										if (p->getCharacter()->getCurrentPA() == 0 && p->getCharacter()->getCurrentPM() == 0)
										{
											b->changeTurn();
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else if (StringUtils::startsWith(toParse, "Ct"))	// Le client indique qu'il a terminé son tour
		{
			if (client->getPseudo().size() > 0)
			{
				tw::Player * p = getPlayerFromClientState(client);
				if (p != NULL && p->getHasJoinBattle() && p->getCharacter() != NULL && p->getCharacter()->isPlayerReady())
				{
					tw::Match * m = tw::PlayerManager::getCurrentOrNextMatchForPlayer(p);
					if (m != NULL)
					{
						Battle * b = (Battle*)m->getBattlePayload();
						if (b != NULL && !b->isPreparationPhase())
						{
							// Si c'est le tour du joueur :
							if (b->getActivePlayer() == p)
							{
								b->changeTurn();
							}
						}
					}
				}
			}
		}
		else if (StringUtils::startsWith(toParse, "CL"))	// Le client indique qu'il veut lancer un sort
		{
			if (client->getPseudo().size() > 0)
			{
				tw::Player * p = getPlayerFromClientState(client);
				if (p != NULL && p->getHasJoinBattle() && p->getCharacter() != NULL && p->getCharacter()->isPlayerReady() && !p->getCharacter()->isMoving())
				{
					tw::Match * m = tw::PlayerManager::getCurrentOrNextMatchForPlayer(p);
					if (m != NULL)
					{
						Battle * b = (Battle*)m->getBattlePayload();
						if (b != NULL && !b->isPreparationPhase())
						{
							// Si c'est le tour du joueur :
							if (b->getActivePlayer() == p)
							{
								std::string data = toParse.substr(2);
								std::vector<std::string> spellData = StringUtils::explode(data, ';');
								int spellId = std::atoi(spellData[0].c_str());
								int cellX = std::atoi(spellData[1].c_str());
								int cellY = std::atoi(spellData[2].c_str());

								if (p->getCharacter()->canDoAttack(spellId))
								{
									int attackPA = -1;
									attackPA = p->getCharacter()->getAttackPACost(spellId);

									// Si le personnage a assez de PA :
									if (attackPA != -1 && p->getCharacter()->hasEnoughPA(attackPA))
									{
										// Check si la cellule ciblée est dans les cellules ciblables :
										std::vector<tw::Point2D> targettable = calculateSpellZone(p->getCharacter(), spellId, m, m->getEnvironment());
										bool isTargettable = false;
										for (int i = 0; i < targettable.size(); i++)
										{
											tw::Point2D targettableCell = targettable[i];
											if (targettableCell.getX() == cellX && targettableCell.getY() == cellY)
											{
												isTargettable = true;
												break;
											}
										}

										// Si la cellule est bien ciblable, lance le sort :
										if (isTargettable)
										{
											std::vector<tw::AttackDamageResult> impactedEntities = p->getCharacter()->doAttack(spellId, cellX, cellY);
											for (int i = 0; i < impactedEntities.size(); i++)
											{
												impactedEntities[i].getCharacter()->modifyCurrentLife(-impactedEntities[i].getDamage());
											}

											std::string str = "CL" + std::to_string(b->getIdForPlayer(p)) + ";" + std::to_string(spellId) + ";" + std::to_string(cellX) + ";" + std::to_string(cellY) + "\n";
											sendToMatch(m, str);

											// Si le personnage est mort pendant son tour ou qu'il n'y a plus d'action possible :
											if (!p->getCharacter()->isAlive() || (p->getCharacter()->getCurrentPA() == 0 && p->getCharacter()->getCurrentPM() == 0))
											{
												// Passage automatique du tour ...
												b->changeTurn();
											}

											checkBattleEnd(m);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

std::vector<tw::Point2D> TWParser::calculateSpellZone(tw::BaseCharacterModel * character, int selectedSpell, tw::Match * match, tw::Environment * environment)
{
	int spellMinPO = -1;
	int spellMaxPO = -1;
	TypeZoneLaunch zoneType = TypeZoneLaunch::NORMAL;
	std::vector<tw::Point2D> targettable;

	if (character != NULL)
	{
		switch (selectedSpell)
		{
		case 1:
			spellMinPO = character->getSpell1MinPO();
			spellMaxPO = character->getSpell1MaxPO();
			zoneType = character->getSpell1LaunchZoneType();
			break;

		case 2:
			spellMinPO = character->getSpell2MinPO();
			spellMaxPO = character->getSpell2MaxPO();
			zoneType = character->getSpell2LaunchZoneType();
			break;

		case 3:
			spellMinPO = character->getSpell3MinPO();
			spellMaxPO = character->getSpell3MaxPO();
			zoneType = character->getSpell3LaunchZoneType();
			break;

		case 4:
			spellMinPO = character->getSpell4MinPO();
			spellMaxPO = character->getSpell4MaxPO();
			zoneType = character->getSpell4LaunchZoneType();
			break;
		}
	}

	if (character != NULL && selectedSpell != -1)
	{
		std::vector<tw::Point2D> targetZone = tw::ZoneAndSightCalculator::getInstance()->generateZone(
			character->getCurrentX(),
			character->getCurrentY(),
			spellMinPO,
			spellMaxPO,
			zoneType);

		std::vector<tw::Obstacle> obstacles;
		std::vector<tw::Obstacle> environmentObstacles = environment->getObstacles();
		std::vector<tw::Obstacle> dynamicsObstacles;
		std::vector<tw::Player*> players = match->getPlayers();
		for (int i = 0; i < players.size(); i++)
		{
			tw::BaseCharacterModel * target = players[i]->getCharacter();
			if (target != character && target->isAlive())
			{
				dynamicsObstacles.push_back(tw::Obstacle(target));
			}
		}

		obstacles.insert(obstacles.end(), environmentObstacles.begin(), environmentObstacles.end());
		obstacles.insert(obstacles.end(), dynamicsObstacles.begin(), dynamicsObstacles.end());

		targettable = tw::ZoneAndSightCalculator::getInstance()->processLineOfSight(
			character->getCurrentX(),
			character->getCurrentY(),
			targetZone,
			obstacles
		);
	}

	return targettable;
}

void TWParser::checkBattleEnd(tw::Match * m)
{
	std::string str = "";
	std::vector<tw::Player*> team1 = m->getTeam1();
	std::vector<tw::Player*> team2 = m->getTeam2();

	bool aliveInTeam1 = false;
	bool aliveInTeam2 = false;

	for (int i = 0; i < team1.size(); i++)
	{
		if (team1[i]->getCharacter()->isAlive())
		{
			aliveInTeam1 = true;
			break;
		}
	}

	for (int i = 0; i < team2.size(); i++)
	{
		if (team2[i]->getCharacter()->isAlive())
		{
			aliveInTeam2 = true;
			break;
		}
	}

	bool endOfBattle = false;
	if (!aliveInTeam1)
	{
		str = "BE" + std::to_string(/*team2[0]->getTeamNumber()*/2) + "\n";
		sendToMatch(m, str);
		m->setWinnerTeam(2);
		endOfBattle = true;
	}

	if (!aliveInTeam2)
	{
		str = "BE" + std::to_string(/*team1[0]->getTeamNumber()*/1) + "\n";
		sendToMatch(m, str);
		m->setWinnerTeam(1);
		endOfBattle = true;
	}

	if (endOfBattle)
	{
		std::vector<tw::Player*> players = m->getPlayers();
		for (int i = 0; i < players.size(); i++)
		{
			tw::BaseCharacterModel * character = players[i]->getCharacter();
			delete character;
			players[i]->setCharacter(NULL);
			players[i]->setHasJoinBattle(false);
		}
	}
}

void TWParser::notifyCharacterPositionChanged(ClientState * toNotify, int playerId, tw::Player * characterWhosePositionChanged)
{
	std::string str = "CP"	+ std::to_string(playerId) + ";" 
							+ std::to_string(characterWhosePositionChanged->getCharacter()->getCurrentX()) + ";"
							+ std::to_string(characterWhosePositionChanged->getCharacter()->getCurrentY())
							+ "\n";

	TcpServer<TWParser, ClientState>::Send(toNotify, (char*)str.c_str(), str.size());
}

bool TWParser::everybodyReadyForBattle(tw::Match * m)
{
	std::vector<tw::Player*> players = m->getPlayers();
	bool ready = true;

	for (int i = 0; i < players.size(); i++)
	{
		if (players[i]->getCharacter() == NULL)
		{
			ready = false;
			break;
		}
	}

	return ready;
}

void TWParser::switchParticipantToBattleState(Battle * b)
{
	std::vector<tw::Player *> players = b->getTimeline();
	for (int i = 0; i < players.size(); i++)
	{
		ClientState * c = getClientStateFromPlayer(players[i]);
		// Si le client est connecté :
		if (c != NULL)
		{
			enterBattleState(b->getMatch(), c);
			synchronizeBattleState(b->getMatch(), c);
		}
	}
}

void TWParser::synchronizeBattleState(tw::Match * m, ClientState * c)
{
	if (m != NULL && c != NULL)
	{
		tw::Player * p = getPlayerFromClientState(c);

		Battle * b = (Battle*)m->getBattlePayload();
		if (b != NULL)
		{
			std::vector<tw::Player *> players = b->getTimeline();
			for (int i = 0; i < players.size(); i++)
			{
				tw::Player * player = players[i];
				tw::BaseCharacterModel * model = player->getCharacter();
				
				std::string addPlayerStr = "CA" + std::to_string(i) + ";" 
												+ std::to_string(model->getClassId()) + ";" 
												+ std::to_string(model->getTeamId()) + ";"
												+ std::to_string(model->getCurrentX()) + ";"
												+ std::to_string(model->getCurrentY()) + ";"
												+ std::to_string(model->getCurrentLife()) + ";"
												+ std::to_string(model->getCurrentPA()) + ";"
												+ std::to_string(model->getCurrentPM()) + ";"
												+ std::to_string(model->getAttackCooldown(1)) + ";"
												+ std::to_string(model->getAttackCooldown(2)) + ";"
												+ std::to_string(model->getAttackCooldown(3)) + ";"
												+ std::to_string(model->getAttackCooldown(4)) + ";"
												+ (m->playerIsInTeam1(player) ? "1" : "2") + ";"	// Color in match
												+ model->getPseudo()
												+ "\n";
				TcpServer<TWParser, ClientState>::Send(c, (char*)addPlayerStr.c_str(), addPlayerStr.size());

				// Informe le client de son personnage actif (celui qu'il contrôle) :
				if (players[i] == p)
				{
					std::string activeCharacterStr = "CS" + std::to_string(i) + "\n";
					TcpServer<TWParser, ClientState>::Send(c, (char*)activeCharacterStr.c_str(), activeCharacterStr.size());
				}

				notifyReadyState(c, i, p);
			}

			notifyBattleState(c, b);

			if (b->getBattleState() == BattleState::BATTLE_PHASE)
			{
				notifyPlayerTurnToken(b, c);

				notifyActivePlayerPANumber(b, c);
				notifyActivePlayerPMNumber(b, c);
			}
		}
	}
}

void TWParser::notifyActivePlayerPANumber(Battle * b, ClientState * c)
{
	tw::Player * activePlayer = b->getActivePlayer();
	int playerId = b->getIdForPlayer(activePlayer);
	int currentPA = activePlayer->getCharacter()->getCurrentPA();

	std::string str = "Ca" + std::to_string(playerId) + ";" + std::to_string(currentPA) + "\n";
	TcpServer<TWParser, ClientState>::Send(c, (char*)str.c_str(), str.size());
}

void TWParser::notifyActivePlayerPMNumber(Battle * b, ClientState * c)
{
	tw::Player * activePlayer = b->getActivePlayer();
	int playerId = b->getIdForPlayer(activePlayer);
	int currentPM = activePlayer->getCharacter()->getCurrentPM();

	std::string str = "Cp" + std::to_string(playerId) + ";" + std::to_string(currentPM) +"\n";
	TcpServer<TWParser, ClientState>::Send(c, (char*)str.c_str(), str.size());
}

void TWParser::notifyPlayerTurnToken(Battle * b, ClientState * c = NULL)
{
	std::string str = "Ct" + std::to_string(b->getIdForPlayer(b->getActivePlayer())) + "\n";

	if (c != NULL)
	{
		TcpServer<TWParser, ClientState>::Send(c, (char*)str.c_str(), str.size());	
	}
	else
	{
		sendToMatch(b->getMatch(), str);
	}
}

void TWParser::notifyReadyState(ClientState * c, int playerId, tw::Player * p)
{
	std::string readyStateStr = (p->getCharacter()->isPlayerReady()) ? "1" : "0";
	std::string str = "Cs" + std::to_string(playerId) + ";" + readyStateStr + "\n";
	TcpServer<TWParser, ClientState>::Send(c, (char*)str.c_str(), str.size());
}

void TWParser::enterBattleState(tw::Match * m, ClientState * c)
{
	if (m != NULL && c != NULL)
	{
		Battle * b = (Battle*)m->getBattlePayload();
		if (b != NULL)
		{
			std::string sentence = "HG" + std::to_string(m->getEnvironment()->getId()) + "\n";
			TcpServer<TWParser, ClientState>::Send(c, (char*)sentence.c_str(), sentence.size());
		}
	}
}

void TWParser::notifyClassChoiceLocked(ClientState * c)
{
	if (c->getPseudo().size() > 0)
	{
		tw::Player * p = playersMap[c->getPseudo()];
		tw::BaseCharacterModel * character = p->getCharacter();
		if (p != NULL)
		{
			std::string sentence = "PO" + std::to_string(character->getClassId()) + "\n";
			TcpServer<TWParser, ClientState>::Send(c, (char*)sentence.c_str(), sentence.size());
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
			TcpServer<TWParser, ClientState>::Send(client, (char*)"HC\n", 3);
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
		TcpServer<TWParser, ClientState>::Send(c, (char*)matchData.c_str(), matchData.size());
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
		TcpServer<TWParser, ClientState>::Send(c, (char*)matchData.c_str(), matchData.size());
	}
}

void TWParser::notifyTeamList(ClientState * c)
{
	std::string data = "TL";
	int i = 0;
	for (std::map<int, std::vector<tw::Player*>>::iterator it = teamIdToPlayerList.begin(); it != teamIdToPlayerList.end(); it++)
	{
		if (i > 0)
		{
			data += ";";
		}

		int teamId = (*it).first;
		std::vector<tw::Player*> team = (*it).second;

		
	
		data += std::to_string(teamId) + ",";
		data += tw::Match::serializeTeam(team, '¨', '^');

		i++;
	}

	data += "\n";

	TcpServer<TWParser, ClientState>::Send(c, (char*)data.c_str(), data.length());
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
		TcpServer<TWParser, ClientState>::Send(c, (char*)matchData.c_str(), matchData.size());
	}
	// Envoi à tout le monde (mise à jour de la liste suite à une modif)
	else
	{
		for (int i = 0; i < spectatorModeClientDiffusionList.size(); i++)
		{
			TcpServer<TWParser, ClientState>::Send(spectatorModeClientDiffusionList[i], (char*)matchData.c_str(), matchData.size());
		}
	}
}

void TWParser::parse(SOCKET sock, unsigned char * buf, int length)
{
	Parser<ClientState>::parse(sock, buf, length);
}

void TWParser::kick(ClientState * client)
{
	// TODO : Notify disconnection to other clients (if in battle)
	notifyKick(client);
}

void TWParser::onClientConnected(ClientState * client)
{
	std::cout << "Client connecte" << std::endl;
	Parser<ClientState>::onClientConnected(client);
}

void TWParser::onClientDisconnected(SOCKET sock)
{
	Parser<ClientState>::onClientDisconnected(sock);
}

void TWParser::onClientDisconnected(ClientState * client)
{
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
	
	// Clear connected player map :
	if (client->getPseudo().length() > 0 && playersMap.find(client->getPseudo()) != playersMap.end())
	{
		tw::Player * p = playersMap[client->getPseudo()];
		
		p->setHasJoinBattle(false);

		if (playerToBattleMap.find(p) != playerToBattleMap.end())
		{
			std::cout << "Notify battle that connection is lost with " << p->getPseudo().c_str() << std::endl;
			
			// TODO : Notify battle that the connection with the player is lost
			//playerToBattleMap[p]->connectionLostWith(p);
		}

		connectedPlayerMap.erase(p);
		notifyMatchConnectedPlayerChanged(tw::PlayerManager::getCurrentOrNextMatchForPlayer(p));
		std::cout << "Client " << p->getPseudo().c_str() << " disconnected ..." << std::endl;
	}
	Parser<ClientState>::onClientDisconnected(client);	
}


void TWParser::onMatchStatusChanged(tw::Match * match, tw::MatchStatus oldStatus, tw::MatchStatus newStatus)
{
	// Notify spectator mode clients
	notifyPlayingMatchList();

	// Notify admin
	notifyPlanifiedAndPlayingMatch(admin);
	notifyFinishedMatch(admin);
}

void TWParser::onBattleStateChanged(tw::Match * m, BattleState state)
{
	std::vector<tw::Player*> players = m->getPlayers();
	for (int i = 0; i < players.size(); i++)
	{
		ClientState * c = getClientStateFromPlayer(players[i]);
		if (c != NULL)
		{
			notifyBattleState(c, (Battle*)m->getBattlePayload());
		}
	}
}

void TWParser::onPlayerTurnStart(tw::Match * match, tw::Player * player)
{
	Battle * b = (Battle *)match->getBattlePayload();
	if (b != NULL)
	{
		if (match->playerIsInThisMatch(player))
		{
			notifyPlayerTurnToken(b);
		}
	}
}

void TWParser::sendToMatch(tw::Match * match, std::string str)
{
	std::vector<tw::Player*> players = match->getPlayers();
	for (int i = 0; i < players.size(); i++)
	{
		ClientState * c = getClientStateFromPlayer(players[i]);
		if (c != NULL)
		{
			TcpServer<TWParser, ClientState>::Send(c, (char*)str.c_str(), str.size());
		}
	}
}


void TWParser::notifyBattleState(ClientState * c, Battle * battle)
{
	std::string str = "BS" + std::to_string((int)battle->getBattleState()) + "\n";
	TcpServer<TWParser, ClientState>::Send(c, (char*)str.c_str(), str.size());
}


void TWParser::notifyMatchConnectedPlayerChanged(tw::Match * match)
{
	if (match != NULL)
	{
		std::vector<tw::Player*> team1 = match->getTeam1();
		std::vector<tw::Player*> team2 = match->getTeam2();

		std::string playerStatus;
		std::vector<tw::Player*> diffusionList;

		for (int i = 0; i < team1.size(); i++)
		{
			if (i > 0)
				playerStatus += ";";
			tw::Player * p = team1[i];
			playerStatus += p->getPseudo() + "," + std::to_string((p->getHasJoinBattle() ? 1 : 0));

			if (p->getHasJoinBattle())
				diffusionList.push_back(p);
		}

		playerStatus += ";";

		for (int i = 0; i < team2.size(); i++)
		{
			if (i > 0)
				playerStatus += ";";
			tw::Player * p = team2[i];
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
				TcpServer<TWParser, ClientState>::Send(c, (char*)playerStatus.c_str(), playerStatus.size());
			}
		}
	}
}