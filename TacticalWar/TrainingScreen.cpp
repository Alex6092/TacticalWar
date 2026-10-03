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
	const std::vector<std::pair<int, std::string>> & candidates = maps();
	for (const auto & entry : candidates)
	{
		if (entry.first == requested)
			return requested;
	}
	if (candidates.empty())
		return requested;
	std::mt19937 rng(std::random_device{}());
	return candidates[rng() % candidates.size()].first;
}

TrainingScreen::TrainingScreen(tgui::Gui * gui, const TrainingSettings & settings)
	: LocalBattleScreen(gui, chooseMap(settings.mapId)), settings(settings), replay(false), replayRemaining(0)
{
	timers = true;
	botOptions.mistakePercent = settings.easy ? EASY_MISTAKE_PERCENT : 0;

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
	auto aiSpells = [&](int classId) {
		const battle::ClassDef * classDef = data.findClass(classId);
		return classDef != nullptr ? battle::randomSpellChoice(*classDef, rng) : std::vector<int>();
	};

	// Équipe 1 : le joueur (combattant 0) et son allié ; équipe 2 : les adversaires.
	std::unique_ptr<battle::BattleEngine> created(new battle::BattleEngine(data, map, rng()));
	int playerClass = pick(settings.playerClass);
	created->addFighter(1, playerClass, u8"Joueur", playerSpells(playerClass));
	if (settings.duo)
	{
		int allyClass = pick(settings.allyClass);
		created->addFighter(1, allyClass, u8"Allié (IA)", aiSpells(allyClass));
	}
	int enemyClass = pick(settings.enemyClasses[0]);
	created->addFighter(2, enemyClass, settings.duo ? u8"Adversaire 1" : u8"Adversaire", aiSpells(enemyClass));
	if (settings.duo)
	{
		int secondClass = pick(settings.enemyClasses[1]);
		created->addFighter(2, secondClass, u8"Adversaire 2", aiSpells(secondClass));
	}
	if (settings.zone)
		created->enableZone(TrainingSettings::ZONE_POINTS);
	created->startPlacement(nowMs);

	// Les combattants de l'IA se placent au hasard sur les cases de départ de leur équipe, puis sont prêts.
	for (const battle::Fighter & fighter : created->getState().fighters)
	{
		if (fighter.id != 0 || settings.autoplay)
			bots.insert(fighter.id);
	}
	for (int id : bots)
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

void TrainingScreen::update(float deltatime)
{
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
