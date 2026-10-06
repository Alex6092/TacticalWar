#include "PuzzleScreen.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "AppearanceChoice.h"
#include "ClientConfig.h"
#include "ClientGameData.h"
#include "LinkToServer.h"
#include "MusicManager.h"
#include "PuzzleSelectScreen.h"
#include "ScreenManager.h"

using namespace tw;
using nlohmann::json;

namespace
{
	const char * PUZZLE_DIR = "./assets/puzzles";
	const float PANEL_TOP = 52;
	// Démonstration : pause avant chaque action.
	const float DEMO_DELAY = 0.7f;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}
}

const std::vector<battle::Puzzle> & PuzzleScreen::puzzles()
{
	static std::vector<battle::Puzzle> list;
	static bool loaded = false;
	if (loaded)
		return list;
	loaded = true;

	std::error_code error;
	for (const auto & entry : std::filesystem::directory_iterator(PUZZLE_DIR, error))
	{
		if (entry.path().extension() != ".json")
			continue;
		std::ifstream file(entry.path(), std::ios::binary);
		std::stringstream content;
		content << file.rdbuf();
		battle::Puzzle puzzle;
		std::string message;
		if (battle::parsePuzzle(content.str(), puzzle, message))
			list.push_back(puzzle);
	}
	std::sort(list.begin(), list.end(), [](const battle::Puzzle & a, const battle::Puzzle & b) { return a.order < b.order; });
	return list;
}

int PuzzleScreen::mapOf(int index)
{
	const std::vector<battle::Puzzle> & list = puzzles();
	return index >= 0 && index < (int)list.size() ? list[index].mapId : 0;
}

PuzzleScreen::PuzzleScreen(tgui::Gui * gui, int index, bool demo)
	: LocalBattleScreen(gui, mapOf(index)), index(index), demo(demo), demoStep(0), demoWait(0), result(Result::PLAYING),
	next(Next::LIST), hintShown(false), cameraPlaced(false)
{
	timers = false;
	// Fin du combat (tous les adversaires hors combat) : le panneau de l'énigme remplace l'écran de fin.
	hideEnd = true;
	textFont.loadFromFile("./assets/font/OpenSans-Regular.ttf");
	if (index >= 0 && index < (int)puzzles().size())
		puzzle = puzzles()[index];

	const battle::GameData & data = ClientGameData::get().data();
	battle::BattleState state = battle::puzzleState(data, map, puzzle);
	std::unique_ptr<battle::BattleEngine> created(new battle::BattleEngine(data, map, state, 1));
	created->setRollMode(battle::BattleEngine::RollMode::MIN);
	int viewer = 0;
	for (const battle::Fighter & fighter : state.fighters)
	{
		if (fighter.team == 1 && !fighter.piloted)
		{
			viewer = fighter.id;
			break;
		}
	}
	startLocal(std::move(created), viewer);

	hud->showTimers(false);
	hud->showLeaveButton(L"Quitter");
	hud->onClose = [this]() {
		next = Next::LIST;
		closeRequested = true;
	};

	// Panneau de l'énigme, en haut de l'écran.
	panel = tgui::Panel::create();
	panel->getRenderer()->setBackgroundColor(sf::Color(15, 15, 25, 235));
	panel->getRenderer()->setBorders(2);
	panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	titleLabel = tgui::Label::create();
	titleLabel->setInheritedFont(font);
	titleLabel->setTextSize(19);
	titleLabel->setPosition(14, 10);
	panel->add(titleLabel);
	textLabel = tgui::Label::create();
	textLabel->setInheritedFont(textFont);
	textLabel->setTextSize(16);
	textLabel->getRenderer()->setTextColor(sf::Color::White);
	textLabel->setPosition(14, 40);
	panel->add(textLabel);

	auto makeButton = [this](const sf::String & text, float width) {
		tgui::Button::Ptr button = tgui::Button::create(text);
		button->setInheritedFont(font);
		button->setTextSize(15);
		button->setSize(width, 34);
		panel->add(button);
		return button;
	};
	hintButton = makeButton(L"Indice", 120);
	hintButton->connect("pressed", [this]() {
		hintShown = true;
		refreshPanel();
	});
	retryButton = makeButton(L"Recommencer", 175);
	retryButton->connect("pressed", [this]() {
		next = Next::RETRY;
		closeRequested = true;
	});
	nextButton = makeButton(L"Énigme suivante", 210);
	nextButton->getRenderer()->setBackgroundColor(sf::Color(255, 215, 0, 220));
	nextButton->connect("pressed", [this]() {
		next = Next::NEXT;
		closeRequested = true;
	});
	listButton = makeButton(L"Énigmes", 130);
	listButton->connect("pressed", [this]() {
		next = Next::LIST;
		closeRequested = true;
	});
	gui->add(panel);
	refreshPanel();
}

void PuzzleScreen::refreshPanel()
{
	int count = (int)puzzles().size();
	sf::String text;
	if (result == Result::SOLVED)
	{
		titleLabel->setText(L"Réussi ! - " + fromServerText(puzzle.title));
		titleLabel->getRenderer()->setTextColor(sf::Color(130, 255, 130));
		text = index + 1 < count ? sf::String(L"Bravo ! Passez à l'énigme suivante, ou rejouez celle-ci.")
			: sf::String(L"Bravo, vous avez résolu la dernière énigme !");
	}
	else if (result == Result::FAILED)
	{
		titleLabel->setText(L"Raté - " + fromServerText(puzzle.title));
		titleLabel->getRenderer()->setTextColor(sf::Color(255, 140, 110));
		text = L"Vos tours sont finis et l'objectif n'est pas atteint. Réessayez !";
	}
	else
	{
		titleLabel->setText(L"Énigme " + num(index + 1) + L"/" + num(count) + L" - " + fromServerText(puzzle.title));
		titleLabel->getRenderer()->setTextColor(sf::Color(255, 215, 0));
		text = fromServerText(puzzle.goal) + L"\nDans les énigmes, les sorts font leurs dégâts minimum (premier chiffre de l'aperçu).";
	}
	if (hintShown && result != Result::SOLVED)
		text += L"\nIndice : " + fromServerText(puzzle.hint);
	textLabel->setText(text);

	hintButton->setVisible(result != Result::SOLVED && !hintShown);
	retryButton->setText(result == Result::FAILED ? L"Réessayer" : L"Recommencer");
	nextButton->setVisible(result == Result::SOLVED && index + 1 < count);
	viewSize = sf::Vector2f();
}

void PuzzleScreen::layoutPanel()
{
	sf::Vector2f size = gui->getView().getSize();
	if (size == viewSize)
		return;
	viewSize = size;

	// Entre le panneau de détails (à gauche) et la frise des tours (à droite).
	float width = std::max(520.f, std::min(780.f, size.x - 740.f));
	textLabel->setMaximumTextWidth(width - 28);
	float buttonsTop = 40 + textLabel->getSize().y + 8;
	float x = 14;
	for (const tgui::Button::Ptr & button : { hintButton, retryButton, nextButton, listButton })
	{
		if (!button->isVisible())
			continue;
		button->setPosition(x, buttonsTop);
		x += button->getSize().x + 10;
	}
	panel->setSize(width, buttonsTop + 34 + 12);
	panel->setPosition((size.x - width) / 2, PANEL_TOP);
}

void PuzzleScreen::playDemo(float deltatime)
{
	// La solution, jouée comme par un joueur (sélection du sort, puis les mêmes messages qu'un clic).
	if (!demo || result != Result::PLAYING || demoStep >= puzzle.solution.size() || !isInteractive() || !idle() || awaitingServer)
		return;
	demoWait += deltatime;
	if (demoWait < DEMO_DELAY)
		return;
	demoWait = 0;

	const battle::PuzzleAction & action = puzzle.solution[demoStep];
	if (action.fighter != actor())
		return;
	if (action.kind == battle::PuzzleAction::Kind::CAST && selectedSpell != action.slot)
	{
		selectSpell(action.slot);
		hoveredCell = action.target;
		return;
	}
	demoStep++;
	if (action.kind == battle::PuzzleAction::Kind::MOVE)
	{
		json path = json::array();
		for (const battle::Cell & cell : action.path)
			path.push_back(json::array({ cell.x, cell.y }));
		sendAction("Cm", { { "path", path } });
	}
	else if (action.kind == battle::PuzzleAction::Kind::CAST)
	{
		sendAction("CL", { { "slot", action.slot }, { "x", action.target.x }, { "y", action.target.y } });
		selectSpell(-1);
	}
	else
	{
		sendAction("Ct", json::object());
	}
}

void PuzzleScreen::update(float deltatime)
{
	// Carte un peu plus petite et plus bas : le panneau de l'énigme occupe le haut.
	if (cameraFitted && !cameraPlaced)
	{
		cameraPlaced = true;
		camera.setZoom(camera.getZoom() * 1.12f);
		camera.centerOn((environment->getWidth() - 1) / 2.f - 1.2f, (environment->getHeight() - 1) / 2.f - 1.2f);
	}

	playDemo(deltatime);

	// Résultat, une fois les animations terminées.
	if (result == Result::PLAYING && engine && idle())
	{
		const battle::BattleState & state = engine->getState();
		if (battle::puzzleSolved(state))
		{
			result = Result::SOLVED;
			ClientConfig & config = ClientConfig::get();
			std::vector<std::string> before = availableAppearances();
			if (config.solvedPuzzles.insert(puzzle.id).second)
				config.save();
			hud->showMessage(L"Énigme réussie !", sf::Color(120, 255, 120), 2.f);
			// Les énigmes débloquent des apparences (envoyées au serveur à la prochaine connexion).
			for (const std::string & id : availableAppearances())
			{
				if (std::find(before.begin(), before.end(), id) == before.end())
				{
					sf::String text = L"Énigme réussie ! Nouvelle apparence débloquée : " + fromServerText(appearanceName(id));
					hud->showMessage(text, sf::Color(255, 215, 70), 4.f);
					hud->log(text, sf::Color(255, 215, 70));
				}
			}
			refreshPanel();
		}
		else if (battle::puzzleFailed(state))
		{
			result = Result::FAILED;
			refreshPanel();
		}
	}
	layoutPanel();

	// En dernier : l'écran peut se fermer pendant cet appel.
	LocalBattleScreen::update(deltatime);
}

void PuzzleScreen::leave()
{
	gui->removeAllWidgets();
	if (window != NULL)
		window->setView(sf::View(sf::FloatRect(0.f, 0.f, (float)window->getSize().x, (float)window->getSize().y)));

	if (next == Next::RETRY)
		ScreenManager::getInstance()->setCurrentScreen(new PuzzleScreen(gui, index));
	else if (next == Next::NEXT && index + 1 < (int)puzzles().size())
		ScreenManager::getInstance()->setCurrentScreen(new PuzzleScreen(gui, index + 1));
	else
	{
		MusicManager::getInstance()->setMenuMusic();
		ScreenManager::getInstance()->setCurrentScreen(new PuzzleSelectScreen(gui));
	}
	delete this;
}
