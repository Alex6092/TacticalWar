#include "TrainingScreen.h"

#include <algorithm>
#include <random>

#include <EnvironmentManager.h>

#include "ClientConfig.h"
#include "ClientGameData.h"
#include "MusicManager.h"
#include "ScreenManager.h"
#include "TrainingSetupScreen.h"

using namespace tw;

namespace
{
	// Part des décisions de l'IA prises au hasard en difficulté "Facile".
	const int EASY_MISTAKE_PERCENT = 35;
}

TrainingSettings & TrainingSettings::current()
{
	static TrainingSettings settings;
	return settings;
}

const std::vector<std::pair<int, std::string>> & TrainingScreen::maps()
{
	static std::vector<std::pair<int, std::string>> result;
	if (!result.empty())
		return result;

	std::vector<std::pair<int, std::string>> all;
	for (int id : EnvironmentManager::getInstance()->getAlreadyExistingIds())
	{
		Environment * environment = EnvironmentManager::getInstance()->loadEnvironment(id);
		if (environment == NULL)
			continue;
		std::string name = environment->getName().empty() ? "Carte " + std::to_string(id) : environment->getName();
		all.push_back({ id, name });
		if (environment->isInTournamentPool())
			result.push_back({ id, name });
		delete environment;
	}
	if (result.empty())
		result = all;
	return result;
}

int TrainingScreen::chooseMap(int requested)
{
	// Carte demandée (réglages, --training-map) : une carte du tournoi, ou une autre carte existante
	// (carte d'exercice des cases spéciales).
	std::vector<int> existing = EnvironmentManager::getInstance()->getAlreadyExistingIds();
	if (requested > 0 && std::find(existing.begin(), existing.end(), requested) != existing.end())
		return requested;
	const std::vector<std::pair<int, std::string>> & candidates = maps();
	if (candidates.empty())
		return requested;
	std::mt19937 rng(std::random_device{}());
	return candidates[rng() % candidates.size()].first;
}

TrainingScreen::TrainingScreen(tgui::Gui * gui, const TrainingSettings & settings)
	: LocalBattleScreen(gui, chooseMap(settings.mapId)), settings(settings), replay(false), replayRemaining(0),
	autoRng(std::random_device{}())
{
	timers = true;
	botOptions.mistakePercent = settings.difficulty == TrainingSettings::Difficulty::EASY ? EASY_MISTAKE_PERCENT : 0;
	botOptions.planner = settings.difficulty == TrainingSettings::Difficulty::HARD;

	const battle::GameData & data = ClientGameData::get().data();
	std::mt19937 rng(std::random_device{}());
	auto pick = [&](int classId) {
		if (data.findClass(classId) != nullptr || data.classes.empty())
			return classId;
		return data.classes[rng() % data.classes.size()].id;
	};

	// Sorts : ceux choisis par le joueur pour sa classe (à défaut, les premiers), au hasard pour l'IA.
	auto playerSpells = [&](int classId) {
		const battle::ClassDef * classDef = data.findClass(classId);
		if (classDef == nullptr)
			return std::vector<int>();
		return settings.autoplay ? battle::randomSpellChoice(*classDef, rng) : battle::validSpellChoice(*classDef, ClientConfig::get().spellChoice(classId));
	};
	// Talents : ceux du joueur (complétés au hasard), au hasard pour l'ordinateur.
	auto aiTalents = [&]() { return battle::randomTalentChoice(data, settings.talentCount, rng); };
	std::vector<std::string> playerTalents = settings.autoplay ? aiTalents()
		: battle::validTalentChoice(data, settings.talents, settings.talentCount);
	for (const std::string & id : battle::randomTalentChoice(data, (int)data.talents.size(), rng))
	{
		if ((int)playerTalents.size() >= settings.talentCount)
			break;
		if (std::find(playerTalents.begin(), playerTalents.end(), id) == playerTalents.end())
			playerTalents.push_back(id);
	}

	auto aiSpells = [&](int classId) {
		const battle::ClassDef * classDef = data.findClass(classId);
		return classDef != nullptr ? battle::randomSpellChoice(*classDef, rng) : std::vector<int>();
	};

	// Équipe 1 : le joueur (combattant 0) et son allié ; équipe 2 : les adversaires.
	std::unique_ptr<battle::BattleEngine> created(new battle::BattleEngine(data, map, rng()));
	int playerClass = pick(settings.playerClass);
	created->addFighter(1, playerClass, u8"Joueur", playerSpells(playerClass), playerTalents);
	bool allyControlled = settings.duo && settings.controlAlly;
	if (settings.duo)
	{
		int allyClass = pick(settings.allyClass);
		created->addFighter(1, allyClass, allyControlled ? u8"Allié (vous)" : u8"Allié (IA)",
			allyControlled ? playerSpells(allyClass) : aiSpells(allyClass), aiTalents());
	}
	int enemyClass = pick(settings.enemyClasses[0]);
	created->addFighter(2, enemyClass, settings.duo ? u8"Adversaire 1" : u8"Adversaire", aiSpells(enemyClass), aiTalents());
	if (settings.duo)
	{
		int secondClass = pick(settings.enemyClasses[1]);
		created->addFighter(2, secondClass, u8"Adversaire 2", aiSpells(secondClass), aiTalents());
	}
	if (settings.zone)
		created->enableZone(TrainingSettings::ZONE_POINTS);
	if (settings.bonuses)
		created->enableMapBonuses();
	created->startPlacement(nowMs);

	// Les combattants de l'IA se placent au hasard sur les cases de départ de leur équipe, puis sont prêts.
	for (const battle::Fighter & fighter : created->getState().fighters)
	{
		if (fighter.id != 0)
			bots.insert(fighter.id);
	}
	// Allié joué par le joueur : placé automatiquement, puis piloté comme le personnage d'un coéquipier absent.
	// Démonstration : le personnage du joueur est placé lui aussi.
	std::set<int> autoPlaced = bots;
	if (settings.autoplay)
		autoPlaced.insert(0);
	if (allyControlled)
	{
		bots.erase(1);
		created->setPiloted(1, true, nowMs);
	}
	for (int id : autoPlaced)
	{
		const battle::Fighter * fighter = created->getState().findFighter(id);
		std::vector<battle::Cell> cells = { fighter->position };
		for (const battle::Cell & cell : map.startCells[fighter->team])
		{
			if (map.isWalkable(cell) && created->getState().fighterAt(cell) == nullptr)
				cells.push_back(cell);
		}
		created->place(id, cells[rng() % cells.size()], nowMs);
		created->setReady(id, true, nowMs);
	}

	startLocal(std::move(created), 0);

	hud->onReplay = [this]() {
		replay = true;
		closeRequested = true;
	};
	hud->showLeaveButton(L"Quitter");
}

void TrainingScreen::playAsPlayer(float deltatime)
{
	if (!engine || !isInteractive() || !idle() || awaitingServer)
	{
		autoWait = 0;
		return;
	}
	const battle::BattleState & state = engine->getState();
	int turn = state.round * 100 + state.turnIndex;
	if (turn != autoTurn)
	{
		autoTurn = turn;
		autoActions = 0;
	}
	autoWait += deltatime;
	if (autoWait < botDelay)
		return;
	autoWait = 0;

	// Au plus 12 actions par tour, comme l'IA des adversaires.
	battle::BotAction action;
	if (autoActions++ < 12)
		action = battle::chooseBotAction(truth, map, engine->getData(), actor(), autoRng, botOptions);
	if (action.kind == battle::BotAction::Kind::CAST)
	{
		sendAction("CL", { { "slot", action.slot }, { "x", action.target.x }, { "y", action.target.y } });
	}
	else if (action.kind == battle::BotAction::Kind::MOVE)
	{
		nlohmann::json path = nlohmann::json::array();
		for (const battle::Cell & cell : action.path)
			path.push_back(nlohmann::json::array({ cell.x, cell.y }));
		sendAction("Cm", { { "path", path } });
	}
	else
	{
		sendAction("Ct", nlohmann::json::object());
	}
}

void TrainingScreen::update(float deltatime)
{
	if (settings.autoplay)
		playAsPlayer(deltatime);

	if (replayRemaining > 0)
	{
		replayRemaining -= deltatime;
		if (replayRemaining <= 0)
		{
			replay = true;
			closeRequested = true;
		}
	}

	LocalBattleScreen::update(deltatime);
}

void TrainingScreen::onLocalEnd()
{
	// Démonstration : le combat suivant commence quelques secondes après l'écran de fin.
	if (settings.autoplay)
		replayRemaining = 8;
}

void TrainingScreen::leave()
{
	gui->removeAllWidgets();
	if (window != NULL)
		window->setView(sf::View(sf::FloatRect(0.f, 0.f, (float)window->getSize().x, (float)window->getSize().y)));

	if (replay)
	{
		ScreenManager::getInstance()->setCurrentScreen(new TrainingScreen(gui, settings));
	}
	else
	{
		MusicManager::getInstance()->setMenuMusic();
		ScreenManager::getInstance()->setCurrentScreen(new TrainingSetupScreen(gui));
	}
	delete this;
}
