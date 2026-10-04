#include "TutorialScreen.h"

#include <algorithm>

#include <BattleRules.h>

#include "ClientGameData.h"
#include "LoginScreen.h"
#include "MusicManager.h"
#include "ScreenManager.h"
#include "TrainingSetupScreen.h"

using namespace tw;
using nlohmann::json;

namespace
{
	const int WARRIOR = 4;
	const int ARCHER = 2;
	// Mannequin abîmé : le combat dure quelques tours seulement.
	const int DUMMY_HP = 50;
	// Le mannequin passe son tour après ce délai (le joueur voit que c'est son tour).
	const float DUMMY_DELAY = 0.8f;
	// Démonstration : pause avant chaque action.
	const float DEMO_DELAY = 0.5f;
	// Case de la source : le Guerrier y va pendant la démonstration (--tutorial-step).
	const battle::Cell SPRING = { 4, 4 };
	const float PANEL_TOP = 52;
	const float FINAL_WIDTH = 860;
	const float FINAL_HEIGHT = 560;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}
}

TutorialScreen::TutorialScreen(tgui::Gui * gui, Origin origin, int startStep)
	: LocalBattleScreen(gui, MAP_ID), origin(origin), next(Next::BACK), dummy(1), fightStart({ -1, -1 }),
	turnEnded(false), pinged(false), continued(false), dummyWait(0), cameraPlaced(false), demoUntil(0), demoActed(-1),
	demoWait(0), finalRequested(false)
{
	timers = false;
	textFont.loadFromFile("./assets/font/OpenSans-Regular.ttf");
	const battle::GameData & data = ClientGameData::get().data();

	// Le joueur (combattant 0) : Guerrier, ses 4 premiers sorts, talent « Garde ». Le mannequin : un
	// Archer abîmé, protégé lui aussi par « Garde », déjà placé et prêt.
	std::unique_ptr<battle::BattleEngine> created(new battle::BattleEngine(data, map, 7));
	const battle::ClassDef * warrior = data.findClass(WARRIOR);
	created->addFighter(1, WARRIOR, u8"Joueur", warrior != nullptr ? battle::validSpellChoice(*warrior, {}) : std::vector<int>(), { "garde" });
	dummy = created->addFighter(2, ARCHER, u8"Mannequin", {}, { "garde" });
	created->startPlacement(nowMs);
	if (!map.startCells[2].empty())
		created->place(dummy, map.startCells[2][0], nowMs);
	created->setReady(dummy, true, nowMs);

	battle::BattleState state = created->getState();
	if (battle::Fighter * mannequin = state.findFighter(dummy))
		mannequin->hp = std::min(mannequin->hp, DUMMY_HP);
	startLocal(std::unique_ptr<battle::BattleEngine>(new battle::BattleEngine(data, map, state, 7)), 0);

	hud->showLeaveButton(L"Quitter");
	hud->showTimers(false);
	// Fermer (bilan de fin) : écran de présentation du tournoi ; Quitter en cours de route.
	hud->onClose = [this]() {
		if (shown.phase == battle::BattlePhase::ENDED)
			showFinal();
		else
			closeRequested = true;
	};

	// Panneau de consigne, en haut de l'écran.
	panel = tgui::Panel::create();
	panel->getRenderer()->setBackgroundColor(sf::Color(15, 15, 25, 230));
	panel->getRenderer()->setBorders(2);
	panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	stepLabel = tgui::Label::create();
	stepLabel->setInheritedFont(font);
	stepLabel->setTextSize(19);
	stepLabel->getRenderer()->setTextColor(sf::Color(255, 215, 0));
	stepLabel->setPosition(14, 10);
	panel->add(stepLabel);
	textLabel = tgui::Label::create();
	textLabel->setInheritedFont(textFont);
	textLabel->setTextSize(17);
	textLabel->getRenderer()->setTextColor(sf::Color::White);
	textLabel->setPosition(14, 40);
	panel->add(textLabel);
	continueButton = tgui::Button::create(L"Continuer");
	continueButton->setInheritedFont(font);
	continueButton->setTextSize(16);
	continueButton->setSize(130, 36);
	continueButton->connect("pressed", [this]() { continued = true; });
	panel->add(continueButton);
	gui->add(panel);

	// Démonstration des étapes précédant l'étape demandée ; au-delà de la dernière : écran final.
	demoUntil = std::max(0, std::min(startStep, script.count()));
	finalRequested = startStep > script.count();
	refreshPanel();
}

void TutorialScreen::playDemo(int step)
{
	// Chaque étape est jouée comme le ferait le joueur (mêmes messages qu'un clic) : sa condition de
	// réussite est vérifiée comme en jeu. Une action par étape, sauf pour finir le combat.
	const battle::Fighter * me = truth.findFighter(you);
	const battle::Fighter * target = truth.findFighter(dummy);
	if (me == NULL || target == NULL || (step == demoActed && step != 8))
		return;
	if (step >= 1 && !isInteractive())
		return;
	demoActed = step;

	switch (step)
	{
	case 0:
		sendToServer("CP", { { "x", map.startCells[1].front().x }, { "y", map.startCells[1].front().y } });
		sendToServer("Cs", { { "ready", true } });
		break;
	case 1:
	{
		json path = json::array();
		for (const battle::Cell & cell : battle::findPath(truth, map, *me, SPRING))
			path.push_back(json::array({ cell.x, cell.y }));
		sendAction("Cm", { { "path", path } });
		break;
	}
	case 2:
		continued = true;
		break;
	case 3:
		// Charge (emplacement 2), visée sur le mannequin.
		selectSpell(1);
		hoveredCell = target->position;
		break;
	case 4:
		sendAction("CL", { { "slot", 1 }, { "x", target->position.x }, { "y", target->position.y } });
		selectSpell(-1);
		break;
	case 5:
		sendAction("Ct", json::object());
		break;
	case 6:
		hoveredFighter = dummy;
		break;
	case 7:
		sendPing({ target->position.x - 1, target->position.y });
		break;
	default:
	{
		// Victoire : Taillade au contact, sinon Charge, sinon fin du tour.
		const battle::GameData & data = ClientGameData::get().data();
		for (int slot = 0; slot < 2; slot++)
		{
			const battle::SpellDef * spell = battle::spellOf(data, *me, slot);
			if (spell == NULL || !battle::checkSpellResources(*me, *spell).empty())
				continue;
			std::vector<battle::Cell> cells = battle::castableCells(truth, map, data, *me, *spell);
			if (std::find(cells.begin(), cells.end(), target->position) != cells.end())
			{
				sendAction("CL", { { "slot", slot }, { "x", target->position.x }, { "y", target->position.y } });
				return;
			}
		}
		sendAction("Ct", json::object());
		break;
	}
	}
}

void TutorialScreen::sendToServer(const std::string & op, const json & body)
{
	if (op == "Ct")
		turnEnded = true;
	else if (op == "CG")
		pinged = true;
	LocalBattleScreen::sendToServer(op, body);
}

void TutorialScreen::refreshPanel()
{
	if (script.finished())
	{
		stepLabel->setText(L"Tutoriel terminé !");
		textLabel->setText(L"Le bilan présente vos hauts faits. Cliquez sur Fermer pour la suite.");
		continueButton->setVisible(false);
	}
	else
	{
		const TutorialStep & step = script.step();
		stepLabel->setText(L"Étape " + num(script.current() + 1) + L"/" + num(script.count()) + L" - " + step.title);
		textLabel->setText(step.text);
		continueButton->setVisible(step.canContinue);
	}
	// Mise en page refaite à la prochaine image.
	viewSize = sf::Vector2f();
}

void TutorialScreen::layoutPanels()
{
	sf::Vector2f size = gui->getView().getSize();
	if (size == viewSize)
		return;
	viewSize = size;

	// Entre le panneau de détails (à gauche) et la frise des tours (à droite).
	float width = std::max(480.f, std::min(760.f, size.x - 760.f));
	bool button = continueButton->isVisible();
	textLabel->setMaximumTextWidth(width - 28 - (button ? 146 : 0));
	float height = std::max(84.f, 40 + textLabel->getSize().y + 14);
	panel->setSize(width, height);
	panel->setPosition((size.x - width) / 2, PANEL_TOP);
	continueButton->setPosition(width - 144, height - 48);

	if (finalPanel)
		finalPanel->setPosition((size.x - FINAL_WIDTH) / 2, std::max(10.f, (size.y - FINAL_HEIGHT) / 2));
}

void TutorialScreen::showFinal()
{
	if (finalPanel)
		return;
	panel->setVisible(false);

	finalPanel = tgui::Panel::create();
	finalPanel->setSize(FINAL_WIDTH, FINAL_HEIGHT);
	finalPanel->getRenderer()->setBackgroundColor(sf::Color(15, 15, 25));
	finalPanel->getRenderer()->setBorders(2);
	finalPanel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));

	tgui::Label::Ptr title = tgui::Label::create(L"Prêt pour le tournoi !");
	title->setInheritedFont(font);
	title->setTextSize(30);
	title->getRenderer()->setTextColor(sf::Color(255, 215, 0));
	title->setPosition(26, 20);
	finalPanel->add(title);

	tgui::Label::Ptr body = tgui::Label::create(
		L"Le jour du tournoi, vous jouerez en équipe de deux contre une autre équipe.\n\n"
		L"-  Avant chaque match, choisissez votre classe et emportez 4 de ses 6 sorts.\n"
		L"-  Talents : chaque match joué vous en fait gagner un (3 au plus), à choisir avant chaque match. "
		L"Le talent Garde, qui vous a protégé ici, en est un.\n"
		L"-  Bannissement, si l'organisateur l'a prévu (souvent en phase finale) : chaque équipe interdit une "
		L"classe à l'autre avant le match.\n"
		L"-  Selon l'organisateur, on gagne en mettant l'équipe adverse hors combat, ou en tenant la zone "
		L"dorée au centre de la carte.\n"
		L"-  En 2 contre 2, parlez avec votre coéquipier : les signaux (Alt + clic) et les combinaisons de sorts "
		L"entre classes font la différence.\n\n"
		L"Entraînez-vous contre l'ordinateur pour découvrir les quatre classes !");
	body->setInheritedFont(textFont);
	body->setTextSize(18);
	body->setMaximumTextWidth(FINAL_WIDTH - 52);
	body->getRenderer()->setTextColor(sf::Color::White);
	body->setPosition(26, 78);
	finalPanel->add(body);

	tgui::Button::Ptr training = tgui::Button::create(L"Entraînement libre");
	training->setInheritedFont(font);
	training->setTextSize(18);
	training->setSize(260, 46);
	training->setPosition(FINAL_WIDTH / 2 - 280, FINAL_HEIGHT - 70);
	training->getRenderer()->setBackgroundColor(sf::Color(255, 215, 0, 220));
	training->connect("pressed", [this]() {
		next = Next::TRAINING;
		closeRequested = true;
	});
	finalPanel->add(training);

	tgui::Button::Ptr back = tgui::Button::create(L"Retour");
	back->setInheritedFont(font);
	back->setTextSize(18);
	back->setSize(260, 46);
	back->setPosition(FINAL_WIDTH / 2 + 20, FINAL_HEIGHT - 70);
	back->connect("pressed", [this]() {
		next = Next::BACK;
		closeRequested = true;
	});
	finalPanel->add(back);

	gui->add(finalPanel);
	viewSize = sf::Vector2f();
}

void TutorialScreen::update(float deltatime)
{
	// Le mannequin passe ses tours, une fois les animations terminées.
	const battle::BattleState & state = engine->getState();
	if (state.phase == battle::BattlePhase::FIGHT && state.activeFighterId() == dummy && idle())
	{
		dummyWait += deltatime;
		if (dummyWait >= DUMMY_DELAY)
		{
			dummyWait = 0;
			engine->endTurn(dummy, nowMs);
			deliver();
		}
	}
	else
	{
		dummyWait = 0;
	}

	// Carte un peu plus petite et plus bas que d'habitude : le panneau de consigne occupe le haut.
	if (cameraFitted && !cameraPlaced)
	{
		cameraPlaced = true;
		camera.setZoom(camera.getZoom() * 1.12f);
		camera.centerOn((environment->getWidth() - 1) / 2.f - 1.2f, (environment->getHeight() - 1) / 2.f - 1.2f);
	}

	if (finalRequested && endShown && !finalPanel)
		showFinal();

	// Démonstration (--tutorial-step) : une action à la fois, animations terminées.
	if (!script.finished() && script.current() < demoUntil && hasSnapshot && idle() && !awaitingServer)
	{
		demoWait += deltatime;
		if (demoWait >= DEMO_DELAY)
		{
			demoWait = 0;
			playDemo(script.current());
		}
	}

	if (fightStart.x < 0 && shown.phase == battle::BattlePhase::FIGHT)
		fightStart = shown.findFighter(0)->position;

	// Consigne de l'étape : remplie d'après l'état affiché et les actions du joueur.
	TutorialContext context;
	context.state = &shown;
	context.map = &map;
	context.player = 0;
	context.dummy = dummy;
	context.fightStart = fightStart;
	context.hoveredCell = hoveredCell;
	context.hoveredFighter = hoveredFighter;
	context.selectedSpell = selectedSpell;
	context.turnEnded = turnEnded;
	context.pinged = pinged;
	context.continued = continued;
	// Le bilan des combattants n'arrive dans l'état affiché qu'à la fin du combat : les dégâts subis
	// par le mannequin sont lus dans le moteur local, une fois les animations terminées.
	context.dummyHit = idle() && engine->getState().findFighter(dummy)->record.taken > 0;
	bool changed = script.update(context);
	// Mannequin mis hors combat avant la fin des étapes : le tutoriel est terminé.
	if (!script.finished() && shown.phase == battle::BattlePhase::ENDED)
	{
		script.skipTo(script.count());
		changed = true;
	}
	if (changed)
	{
		turnEnded = pinged = continued = false;
		hud->showMessage(script.finished() ? L"Tutoriel réussi !" : L"Bien joué !", sf::Color(120, 255, 120), 1.5f);
		refreshPanel();
	}
	layoutPanels();

	// En dernier : l'écran peut se fermer pendant cet appel.
	LocalBattleScreen::update(deltatime);
}

void TutorialScreen::leave()
{
	gui->removeAllWidgets();
	if (window != NULL)
		window->setView(sf::View(sf::FloatRect(0.f, 0.f, (float)window->getSize().x, (float)window->getSize().y)));
	MusicManager::getInstance()->setMenuMusic();

	if (next == Next::TRAINING || origin == Origin::TRAINING)
		ScreenManager::getInstance()->setCurrentScreen(new TrainingSetupScreen(gui));
	else
		ScreenManager::getInstance()->setCurrentScreen(new LoginScreen(gui));
	delete this;
}
