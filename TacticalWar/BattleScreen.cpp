#include "BattleScreen.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <BattleMirror.h>
#include <BattleRules.h>
#include <CharacterFactory.h>
#include <EnvironmentManager.h>
#include <EnvironmentMap.h>
#include <Message.h>

#include "AdminScreen.h"
#include "ClassSelectionScreen.h"
#include "ClientGameData.h"
#include "LinkToServer.h"
#include "LoginScreen.h"
#include "MusicManager.h"
#include "ScreenManager.h"
#include "SpectatorModeScreen.h"
#include "WaitMatchScreen.h"

using namespace tw;
using nlohmann::json;

namespace
{
	// Au-delà de ces tailles de file, les animations sont accélérées puis appliquées directement
	// (ex : reconnexion ou client qui a pris du retard).
	const std::size_t FAST_QUEUE = 25;
	const std::size_t INSTANT_QUEUE = 60;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	std::vector<Point2D> toLegacyPath(const json & path)
	{
		// Les vues attendent la destination en premier et le premier pas en dernier.
		std::vector<Point2D> legacy;
		for (const json & cell : path)
			legacy.insert(legacy.begin(), Point2D(cell.at(0).get<int>(), cell.at(1).get<int>()));
		return legacy;
	}

	sf::String reasonLabel(battle::EndReason reason)
	{
		switch (reason)
		{
		case battle::EndReason::KO: return L"Toute l'équipe adverse est hors combat.";
		case battle::EndReason::ROUND_LIMIT: return L"Limite de tours atteinte : décision aux points de vie.";
		case battle::EndReason::FORFEIT: return L"Victoire par forfait.";
		case battle::EndReason::ADMIN: return L"Combat arrêté par l'organisateur : décision aux points de vie.";
		default: return L"";
		}
	}
}

BattleScreen::BattleScreen(tgui::Gui * gui, int environmentId, Mode mode)
	: gui(gui), window(NULL), mode(mode), autoCloseRemaining(-1), you(-1), lastSeq(0), hasSnapshot(false), awaitingServer(false),
	stepRemaining(0), waitingMove(false), waitingMoveTime(0), deadline(0), selectedSpell(-1), hoveredFighter(-1),
	closeRequested(false), endShown(false)
{
	gui->removeAllWidgets();

	renderer = new IsometricRenderer(NULL);
	environment = EnvironmentManager::getInstance()->loadEnvironment(environmentId);
	map = battle::battleMapFromEnvironment(environment);
	hoveredCell = { -1, -1 };

	camera.reset(environment->getWidth(), environment->getHeight());
	// En mode réalisateur, la caméra suit le personnage actif.
	camera.setFollowing(mode == Mode::SPECTATOR && SpectatorModeScreen::isDirectorMode());

	colorator = new BattleColorator();
	renderer->setColorator(colorator);
	renderer->addEventListener(this);

	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");

	hud.reset(new BattleHud(gui, font));
	hud->onSpellClicked = [this](int slot) { selectSpell(selectedSpell == slot ? -1 : slot); };
	hud->onEndTurn = [this]() { sendAction("Ct", json::object()); };
	hud->onReady = [this](bool ready) { LinkToServer::getInstance()->SendRaw("Cs" + json({ { "ready", ready } }).dump()); };
	hud->onClose = [this]() { closeRequested = true; };

	if (MusicManager::getInstance()->isEnabled())
		sounds.resize(8);

	LinkToServer::getInstance()->addListener(this);
	MusicManager::getInstance()->setBattleMusic();
}

BattleScreen::~BattleScreen()
{
	LinkToServer::getInstance()->removeListener(this);
	for (auto & entry : views)
		delete entry.second;

	delete colorator;
	delete renderer;
	delete environment;
}

//----------------------------------------------------------
// Boucle principale
//----------------------------------------------------------

void BattleScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	this->window = window;
	this->gui = gui;
	hud->layout(window->getSize());
}

void BattleScreen::update(float deltatime)
{
	Screen::update(deltatime);
	hud->update(deltatime);

	for (auto & entry : views)
		entry.second->update(deltatime);

	processVisuals(deltatime);

	for (auto it = pendingDeaths.begin(); it != pendingDeaths.end();)
	{
		it->second -= deltatime;
		if (it->second <= 0)
		{
			BaseCharacterModel * view = viewOf(it->first);
			if (view != NULL)
			{
				view->resetAnimation();
				view->setCurrentLife(0);
			}
			it = pendingDeaths.erase(it);
		}
		else
		{
			it++;
		}
	}

	for (FloatingText & text : floatingTexts)
		text.age += deltatime;
	floatingTexts.erase(std::remove_if(floatingTexts.begin(), floatingTexts.end(),
		[](const FloatingText & text) { return text.age > 1.4f; }), floatingTexts.end());

	for (SpellEffect & effect : spellEffects)
	{
		effect.view->update(deltatime);
		effect.remaining -= deltatime;
	}
	spellEffects.erase(std::remove_if(spellEffects.begin(), spellEffects.end(),
		[](const SpellEffect & effect) { return effect.remaining <= 0; }), spellEffects.end());

	renderer->ellapseTime(deltatime);

	// Caméra : suivi du personnage actif (sa position interpolée pendant les déplacements).
	BaseCharacterModel * activeView = viewOf(shown.activeFighterId());
	if (activeView != NULL && shown.phase == battle::BattlePhase::FIGHT)
		camera.followCell(activeView->getInterpolatedX(), activeView->getInterpolatedY());
	camera.update(deltatime);

	// Mode réalisateur : retour automatique à la liste quelques secondes après la fin du combat.
	if (autoCloseRemaining > 0)
	{
		autoCloseRemaining -= deltatime;
		hud->setEndButtonText(L"Retour à la liste (" + num((int)std::ceil(std::max(0.f, autoCloseRemaining))) + L")");
		if (autoCloseRemaining <= 0)
			closeRequested = true;
	}

	if (hasSnapshot)
	{
		float remaining = std::max(0.f, deadline - clock.getElapsedTime().asSeconds());
		hud->refresh(shown, ClientGameData::get().data(), you, hoveredFighter, selectedSpell, isInteractive(), remaining);
		refreshPreview();
	}

	if (closeRequested)
	{
		leave();
		return;
	}

	LinkToServer::getInstance()->UpdateReceivedData();
}

void BattleScreen::render(sf::RenderWindow * window)
{
	renderer->modifyWindow(window);
	camera.apply(*renderer);

	std::vector<BaseCharacterModel*> characters;
	for (auto & entry : views)
		characters.push_back(entry.second);

	std::vector<AbstractSpellView<sf::Sprite*>*> effects;
	for (SpellEffect & effect : spellEffects)
		effects.push_back(effect.view.get());

	renderer->render(environment, characters, effects, getDeltatime());

	// Textes flottants (dégâts, soins, effets), dans le repère de la carte.
	for (const FloatingText & floating : floatingTexts)
	{
		sf::Text text(floating.text, font, 22);
		float alpha = std::max(0.f, 1.f - floating.age / 1.4f);
		sf::Color color = floating.color;
		color.a = (sf::Uint8)(255 * alpha);
		text.setFillColor(color);
		text.setOutlineColor(sf::Color(0, 0, 0, color.a));
		text.setOutlineThickness(2);

		float isoX = (floating.x * 120 - floating.y * 120) / 2 + 60;
		float isoY = (floating.x * 60 + floating.y * 60) / 2 + 30 - 110 - floating.age * 45;
		text.setPosition(isoX - text.getLocalBounds().width / 2, isoY);
		window->draw(text);
	}
}

//----------------------------------------------------------
// Messages du serveur
//----------------------------------------------------------

void BattleScreen::onMessageReceived(std::string msg)
{
	tw::protocol::Message message;
	if (!tw::protocol::Message::decode(msg, message))
		return;

	if (message.op == "BI")
	{
		json snapshot;
		if (message.parseJson(snapshot))
			applySnapshot(snapshot);
	}
	else if (message.op == "BV")
	{
		json batch;
		if (!hasSnapshot || !message.parseJson(batch))
			return;

		std::uint64_t seq = batch.value("seq", (std::uint64_t)0);
		if (seq != lastSeq + 1)
		{
			// Événements manquants : demande de l'état complet.
			LinkToServer::getInstance()->SendRaw("BR{}");
			return;
		}
		lastSeq = seq;
		awaitingServer = false;

		for (const json & event : batch["ev"])
		{
			battle::BattleMirror::applyEvent(truth, event);

			std::string type = event.value("t", std::string());
			if (type == "timer" || type == "placement")
				deadline = clock.getElapsedTime().asSeconds() + event.value("ms", 0) / 1000.f;

			visualQueue.push_back(event);
		}
	}
	else if (message.op == "ER")
	{
		json error;
		awaitingServer = false;
		if (message.parseJson(error))
			hud->showMessage(fromServerText(error.value("message", std::string())), sf::Color(255, 110, 90), 2.5f);
	}
	else if (message.op == "HW")
	{
		// Combat annulé par l'organisateur : retour à l'attente (ou à la liste des combats).
		closeRequested = true;
	}
	else if (message.op == "HC")
	{
		gui->removeAllWidgets();
		if (window != NULL)
			window->setView(window->getDefaultView());
		ScreenManager::getInstance()->setCurrentScreen(new ClassSelectionScreen(gui));
		MusicManager::getInstance()->setMenuMusic();
		delete this;
	}
}

void BattleScreen::onDisconnected()
{
	gui->removeAllWidgets();
	if (window != NULL)
		window->setView(window->getDefaultView());
	ScreenManager::getInstance()->setCurrentScreen(new LoginScreen(gui));
	delete this;
}

void BattleScreen::applySnapshot(const json & snapshot)
{
	battle::BattleMirror::applySnapshot(truth, map, snapshot);
	shown = truth;
	you = snapshot.value("you", -1);

	const json & teams = snapshot.value("teams", json::array());
	for (std::size_t i = 0; i < 2 && i < teams.size(); i++)
		teamNames[i] = fromServerText(teams[i].get<std::string>());
	if (mode != Mode::PLAYER)
	{
		sf::String title = fromServerText(snapshot.value("title", std::string()));
		hud->setSpectator((title.isEmpty() ? sf::String() : title + L" : ") + teamLabel(1) + L" contre " + teamLabel(2));
	}
	lastSeq = snapshot.value("seq", (std::uint64_t)0);
	deadline = clock.getElapsedTime().asSeconds() + snapshot.value("ms", 0) / 1000.f;
	hasSnapshot = true;
	awaitingServer = false;

	visualQueue.clear();
	waitingMove = false;
	stepRemaining = 0;
	pendingDeaths.clear();

	for (const battle::Fighter & fighter : truth.fighters)
		syncView(fighter);

	if (truth.phase == battle::BattlePhase::PLACEMENT)
		colorator->setStartCells(map.startCells[1], map.startCells[2]);
	else
		colorator->setStartCells({}, {});

	if (truth.phase == battle::BattlePhase::ENDED)
		showEnd();
}

BaseCharacterModel * BattleScreen::viewOf(int fighterId)
{
	auto it = views.find(fighterId);
	return it == views.end() ? NULL : it->second;
}

void BattleScreen::syncView(const battle::Fighter & fighter)
{
	BaseCharacterModel * view = viewOf(fighter.id);
	if (view == NULL)
	{
		view = CharacterFactory::getInstance()->constructCharacter(environment, fighter.classId, fighter.team, fighter.position.x, fighter.position.y, this);
		if (view == NULL)
			return;
		view->setColorNumber(fighter.team);
		view->setPseudo(fighter.name);
		views[fighter.id] = view;
	}

	view->setCurrentX(fighter.position.x);
	view->setCurrentY(fighter.position.y);
	view->setDisplayMaxLife(fighter.maxHp);
	view->setCurrentLife(fighter.alive ? fighter.hp : 0);
	view->setCurrentPA(fighter.ap);
	view->setCurrentPM(fighter.mp);
	view->setReadyStatus(fighter.ready);
	view->resetAnimation();
}

//----------------------------------------------------------
// Animation des événements
//----------------------------------------------------------

void BattleScreen::processVisuals(float deltatime)
{
	if (waitingMove)
	{
		// Sécurité : un déplacement qui ne se termine pas ne doit pas bloquer l'affichage.
		waitingMoveTime += deltatime;
		if (waitingMoveTime < 6.f)
			return;
		waitingMove = false;
	}
	waitingMoveTime = 0;

	bool instant = visualQueue.size() > INSTANT_QUEUE;
	float speed = visualQueue.size() > FAST_QUEUE ? 3.f : 1.f;
	stepRemaining -= deltatime * speed;

	while ((stepRemaining <= 0 || instant) && !visualQueue.empty() && !waitingMove)
	{
		json event = visualQueue.front();
		visualQueue.pop_front();
		stepRemaining = playVisual(event, speed > 1.f || instant);
		instant = visualQueue.size() > INSTANT_QUEUE;
	}
}

void BattleScreen::onMoveFinished()
{
	waitingMove = false;
	stepRemaining = 0;
}

float BattleScreen::playVisual(const json & event, bool fast)
{
	battle::BattleMirror::applyEvent(shown, event);

	const tw::battle::GameData & data = ClientGameData::get().data();
	std::string type = event.value("t", std::string());
	int fighterId = event.value("f", -1);
	BaseCharacterModel * view = viewOf(fighterId);
	const battle::Fighter * fighter = shown.findFighter(fighterId);

	if (type == "placement")
	{
		colorator->setStartCells(map.startCells[1], map.startCells[2]);
		return 0;
	}
	if (type == "place" && view != NULL)
	{
		view->setCurrentX(event["x"].get<int>());
		view->setCurrentY(event["y"].get<int>());
		return 0;
	}
	if (type == "fight")
	{
		colorator->setStartCells({}, {});
		hud->showMessage(L"Le combat commence !", sf::Color(255, 220, 80), 1.5f);
		hud->log(L"Le combat commence.", sf::Color(255, 220, 80));
		return fast ? 0 : 0.8f;
	}
	if (type == "turn" && fighter != NULL)
	{
		bool mine = fighterId == you;
		sf::String text = mine ? sf::String(L"À vous de jouer !") : L"Tour de " + fromServerText(fighter->name);
		hud->showMessage(text, mine ? sf::Color(120, 255, 120) : sf::Color(255, 220, 80), 1.2f);
		hud->log(L"--- Tour " + num(shown.round) + L" : " + fromServerText(fighter->name), sf::Color(255, 220, 80));
		selectSpell(-1);
		return fast ? 0 : 0.4f;
	}
	if (type == "move" && view != NULL)
	{
		for (const json & tackle : event.value("tackles", json::array()))
		{
			hud->log(fighterName(fighterId) + L" est taclé : -" + num(tackle.value("mp", 0)) + L" PM, -" + num(tackle.value("ap", 0)) + L" PA", sf::Color(255, 170, 90));
			addFloatingText(fighterId, L"Taclé !", sf::Color(255, 170, 90));
		}

		const json & path = event["path"];
		if (!path.empty())
		{
			if (fast)
			{
				view->setCurrentX(path.back().at(0).get<int>());
				view->setCurrentY(path.back().at(1).get<int>());
			}
			else
			{
				view->setPath(toLegacyPath(path), this);
				waitingMove = true;
			}
		}
		view->setCurrentPA(event.value("ap", 0));
		view->setCurrentPM(event.value("mp", 0));
		return 0;
	}
	if (type == "cast" && view != NULL && fighter != NULL)
	{
		int x = event["x"].get<int>();
		int y = event["y"].get<int>();
		const battle::SpellDef * spell = battle::spellOf(data, *fighter, event.value("slot", -1));
		view->setOrientationToLookAt(x, y);

		if (spell != NULL)
		{
			if (spell->casterAnimation == "physical")
				view->startAttack2Animation(1);
			else
				view->startAttack1Animation(1);

			if (!spell->fxSprite.empty() && !fast)
			{
				SpellEffect effect;
				effect.view.reset(new SpellView(x, y));
				effect.view->loadAnimation(spell->fxSprite);
				effect.remaining = 0.6f;
				spellEffects.push_back(std::move(effect));
			}
			playSound(spell->sound);
			hud->log(fighterName(fighterId) + L" lance " + fromServerText(spell->name), sf::Color(150, 200, 255));
		}
		return fast ? 0.05f : 0.7f;
	}
	if (type == "damage" && view != NULL && fighter != NULL)
	{
		int amount = event.value("amount", 0);
		int absorbed = event.value("absorbed", 0);
		std::string kind = event.value("kind", std::string());

		view->setDisplayMaxLife(fighter->maxHp);
		// Un mort reste affiché le temps de son animation.
		view->setCurrentLife(std::max(fighter->hp, fighter->alive ? 0 : 1));
		if (fighter->alive)
			view->startTakeDmg(1);

		sf::String text = L"-" + num(amount - absorbed);
		if (absorbed > 0)
			text += L" (bouclier -" + num(absorbed) + L")";
		addFloatingText(fighterId, text, sf::Color(255, 80, 70));

		sf::String source = kind == "dot" ? L" (effet)" : kind == "collision" ? L" (collision)" : kind == "sudden" ? L" (mort subite)" : L"";
		hud->log(fighterName(fighterId) + L" perd " + num(amount - absorbed) + L" PV" + source, sf::Color(255, 130, 120));
		MusicManager::getInstance()->playTakeDamageSound();
		return fast ? 0 : 0.35f;
	}
	if (type == "heal" && view != NULL && fighter != NULL)
	{
		view->setCurrentLife(fighter->hp);
		addFloatingText(fighterId, L"+" + num(event.value("amount", 0)), sf::Color(110, 255, 110));
		hud->log(fighterName(fighterId) + L" récupère " + num(event.value("amount", 0)) + L" PV", sf::Color(130, 255, 130));
		return fast ? 0 : 0.3f;
	}
	if (type == "effect+" && fighter != NULL)
	{
		battle::ActiveEffect effect = battle::BattleMirror::effectFromJson(event["effect"]);
		if (effect.spellId != "__passive")
		{
			sf::Color color = effect.positive ? sf::Color(120, 200, 255) : sf::Color(255, 170, 90);
			addFloatingText(fighterId, fromServerText(effect.name), color);
			hud->log(fighterName(fighterId) + L" : " + fromServerText(effect.name), color);
		}
		return fast ? 0 : 0.2f;
	}
	if (type == "stats" && view != NULL && fighter != NULL)
	{
		view->setCurrentPA(fighter->ap);
		view->setCurrentPM(fighter->mp);
		view->setDisplayMaxLife(fighter->maxHp);
		if (fighter->alive)
			view->setCurrentLife(fighter->hp);
		return 0;
	}
	if (type == "slide" && view != NULL)
	{
		view->setCurrentX(event["x"].get<int>());
		view->setCurrentY(event["y"].get<int>());
		std::string kind = event.value("kind", std::string());
		if (kind == "push" || kind == "pull")
			addFloatingText(fighterId, kind == "push" ? L"Repoussé" : L"Attiré", sf::Color(230, 230, 230));
		return fast ? 0 : 0.25f;
	}
	if (type == "swap" && view != NULL)
	{
		view->setCurrentX(event["x"].get<int>());
		view->setCurrentY(event["y"].get<int>());
		BaseCharacterModel * other = viewOf(event.value("other", -1));
		if (other != NULL)
		{
			other->setCurrentX(event["ox"].get<int>());
			other->setCurrentY(event["oy"].get<int>());
		}
		return fast ? 0 : 0.3f;
	}
	if (type == "glyph+")
	{
		hud->log(L"Un glyphe est posé : " + fromServerText(event["glyph"].value("name", std::string())), sf::Color(200, 150, 255));
		return fast ? 0 : 0.2f;
	}
	if (type == "glyph")
	{
		hud->log(fighterName(fighterId) + L" déclenche un glyphe", sf::Color(200, 150, 255));
		return fast ? 0 : 0.2f;
	}
	if (type == "death" && view != NULL)
	{
		view->startDieAction(1);
		pendingDeaths[fighterId] = fast ? 0.f : 0.9f;
		hud->log(fighterName(fighterId) + L" est hors combat !", sf::Color(255, 90, 90));
		return fast ? 0 : 0.9f;
	}
	if (type == "timeout")
	{
		hud->log(L"Temps écoulé pour " + fighterName(fighterId), sf::Color(200, 200, 200));
		return 0;
	}
	if (type == "connection")
	{
		bool connected = event.value("connected", true);
		hud->log(fighterName(fighterId) + (connected ? L" est revenu." : L" s'est déconnecté."), sf::Color(200, 200, 200));
		return 0;
	}
	if (type == "end")
	{
		showEnd();
		return 0;
	}

	return 0;
}

void BattleScreen::showEnd()
{
	if (endShown)
		return;
	endShown = true;
	selectSpell(-1);

	const battle::Fighter * me = shown.findFighter(you);
	sf::String title;
	bool victory = false;
	if (me != NULL)
	{
		victory = me->team == shown.winnerTeam;
		title = victory ? L"Victoire !" : L"Défaite...";
	}
	else
	{
		title = L"Victoire : " + teamLabel(shown.winnerTeam);
	}

	sf::String winners;
	for (const battle::Fighter & fighter : shown.fighters)
	{
		if (fighter.team == shown.winnerTeam)
			winners += (winners.isEmpty() ? sf::String() : sf::String(L" et ")) + fromServerText(fighter.name);
	}

	sf::String details = winners + L" remportent le combat.\n" + reasonLabel(shown.endReason)
		+ L"\nTours joués : " + num(shown.round);
	hud->showEnd(title, details, victory || me == NULL);
	hud->log(title, victory ? sf::Color(120, 255, 120) : sf::Color(255, 120, 120));

	if (mode == Mode::SPECTATOR && SpectatorModeScreen::isDirectorMode())
		autoCloseRemaining = 10.f;
}

sf::String BattleScreen::teamLabel(int team) const
{
	if (team >= 1 && team <= 2 && !teamNames[team - 1].isEmpty())
		return teamNames[team - 1];
	return L"équipe " + num(team);
}

void BattleScreen::leave()
{
	gui->removeAllWidgets();
	if (window != NULL)
		window->setView(sf::View(sf::FloatRect(0.f, 0.f, (float)window->getSize().x, (float)window->getSize().y)));
	MusicManager::getInstance()->setMenuMusic();

	if (mode == Mode::PLAYER)
	{
		ScreenManager::getInstance()->setCurrentScreen(new WaitMatchScreen(gui));
	}
	else
	{
		LinkToServer::getInstance()->SendRaw("SU{}");
		if (mode == Mode::ADMIN)
			ScreenManager::getInstance()->setCurrentScreen(new AdminScreen(gui));
		else
			ScreenManager::getInstance()->setCurrentScreen(new SpectatorModeScreen(gui));
	}
	delete this;
}

//----------------------------------------------------------
// Interactions
//----------------------------------------------------------

bool BattleScreen::isInteractive() const
{
	return hasSnapshot && !awaitingServer && truth.phase == battle::BattlePhase::FIGHT
		&& truth.activeFighterId() == you && visualQueue.empty() && !waitingMove && stepRemaining <= 0;
}

bool BattleScreen::isMouseOverHud() const
{
	if (window == NULL)
		return false;

	sf::Vector2i mouse = sf::Mouse::getPosition(*window);
	for (const tgui::Widget::Ptr & widget : gui->getWidgets())
	{
		if (!widget->isVisible())
			continue;
		sf::Vector2f position = widget->getPosition();
		sf::Vector2f size = widget->getSize();
		if (mouse.x >= position.x && mouse.y >= position.y && mouse.x < position.x + size.x && mouse.y < position.y + size.y)
			return true;
	}
	return false;
}

void BattleScreen::selectSpell(int slot)
{
	selectedSpell = -1;
	if (slot < 0 || !isInteractive())
		return;

	const battle::Fighter * me = truth.findFighter(you);
	const battle::SpellDef * spell = me != NULL ? battle::spellOf(ClientGameData::get().data(), *me, slot) : NULL;
	if (spell == NULL)
		return;

	std::string error = battle::checkSpellResources(*me, *spell);
	if (!error.empty())
	{
		hud->showMessage(fromServerText(error), sf::Color(255, 110, 90), 1.5f);
		return;
	}
	selectedSpell = slot;
}

void BattleScreen::sendAction(const std::string & op, const json & body)
{
	if (awaitingServer)
		return;
	awaitingServer = true;
	LinkToServer::getInstance()->SendRaw(op + body.dump());
}

void BattleScreen::refreshPreview()
{
	colorator->clearPreview();
	const battle::Fighter * me = truth.findFighter(you);
	colorator->setGlyphs(shown.glyphs, me != NULL ? me->team : 0);
	hud->setHint("");

	if (!isInteractive() || me == NULL)
		return;

	const tw::battle::GameData & data = ClientGameData::get().data();

	if (selectedSpell >= 0)
	{
		const battle::SpellDef * spell = battle::spellOf(data, *me, selectedSpell);
		if (spell == NULL)
			return;

		std::vector<battle::Cell> castable = battle::castableCells(truth, map, data, *me, *spell);
		colorator->setCastable(castable);
		if (colorator->isCastable(hoveredCell))
			colorator->setImpact(battle::impactCells(map, me->position, hoveredCell, spell->impact));
		hud->setHint(fromServerText(spell->name) + L" : cliquez sur une case bleue (Échap pour annuler)");
		return;
	}

	colorator->setReachable(battle::reachableCells(truth, map, *me));
	if (colorator->isReachable(hoveredCell))
	{
		std::vector<battle::Cell> path = battle::findPath(truth, map, *me, hoveredCell);
		battle::MovePreview preview = battle::previewMove(truth, map, data, *me, path);
		colorator->setPath(preview.path, preview.path.size() < path.size() || !preview.tackles.empty());

		if (!preview.tackles.empty())
		{
			int lostMp = 0;
			int lostAp = 0;
			for (const battle::TackleLoss & loss : preview.tackles)
			{
				lostMp += loss.lostMp;
				lostAp += loss.lostAp;
			}
			hud->setHint(L"Tacle : -" + num(lostMp) + L" PM, -" + num(lostAp) + L" PA");
		}
	}
}

void BattleScreen::onCellClicked(int cellX, int cellY)
{
	if (!hasSnapshot || isMouseOverHud())
		return;

	battle::Cell cell = { cellX, cellY };
	const battle::Fighter * me = truth.findFighter(you);
	if (me == NULL)
		return;

	if (truth.phase == battle::BattlePhase::PLACEMENT)
	{
		const std::vector<battle::Cell> & starts = map.startCells[me->team];
		if (!me->ready && std::find(starts.begin(), starts.end(), cell) != starts.end())
			LinkToServer::getInstance()->SendRaw("CP" + json({ { "x", cellX }, { "y", cellY } }).dump());
		return;
	}

	if (!isInteractive())
		return;

	if (selectedSpell >= 0)
	{
		if (colorator->isCastable(cell))
			sendAction("CL", { { "slot", selectedSpell }, { "x", cellX }, { "y", cellY } });
		selectedSpell = -1;
		return;
	}

	if (colorator->isReachable(cell))
	{
		json path = json::array();
		for (const battle::Cell & step : battle::findPath(truth, map, *me, cell))
			path.push_back(json::array({ step.x, step.y }));
		sendAction("Cm", { { "path", path } });
	}
}

void BattleScreen::onCellHover(int cellX, int cellY)
{
	hoveredCell = { cellX, cellY };
	const battle::Fighter * fighter = shown.fighterAt(hoveredCell);
	hoveredFighter = fighter != NULL ? fighter->id : -1;
}

void BattleScreen::onCellMouseDown(int cellX, int cellY)
{
}

void BattleScreen::onEvent(void * e)
{
	if (e == NULL)
		return;

	sf::Event * event = (sf::Event*)e;
	if (event->type == sf::Event::Resized && window != NULL)
	{
		sf::View view = window->getView();
		view.setSize((float)event->size.width, (float)event->size.height);
		window->setView(view);
	}
	else if (event->type == sf::Event::KeyPressed)
	{
		switch (event->key.code)
		{
		case sf::Keyboard::Num1: selectSpell(0); break;
		case sf::Keyboard::Num2: selectSpell(1); break;
		case sf::Keyboard::Num3: selectSpell(2); break;
		case sf::Keyboard::Num4: selectSpell(3); break;
		case sf::Keyboard::Escape: selectSpell(-1); break;
		case sf::Keyboard::F:
			camera.setFollowing(!camera.isFollowing());
			hud->showMessage(camera.isFollowing() ? L"Caméra : suivi du personnage actif" : L"Caméra libre", sf::Color(200, 220, 255), 1.2f);
			break;
		case sf::Keyboard::C:
			camera.reset(environment->getWidth(), environment->getHeight());
			break;
		default: break;
		}
	}

	// Caméra : la molette au-dessus de l'interface (journal) reste à l'interface.
	if (window != NULL)
	{
		bool overHud = isMouseOverHud();
		bool wheel = event->type == sf::Event::MouseWheelScrolled;
		bool press = event->type == sf::Event::MouseButtonPressed;
		if (!((wheel || press) && overHud))
			camera.handleEvent(*event, *window);
	}

	if (gui != NULL)
		gui->handleEvent(*event);
}

//----------------------------------------------------------
// Utilitaires
//----------------------------------------------------------

void BattleScreen::addFloatingText(int fighterId, const sf::String & text, const sf::Color & color)
{
	const battle::Fighter * fighter = shown.findFighter(fighterId);
	if (fighter == NULL)
		return;

	FloatingText floating;
	floating.text = text;
	floating.color = color;
	floating.x = (float)fighter->position.x;
	floating.y = (float)fighter->position.y;
	// Les textes simultanés sur un même combattant sont décalés.
	for (const FloatingText & other : floatingTexts)
	{
		if (other.x == floating.x && other.y == floating.y && other.age < 0.3f)
			floating.age -= 0.3f;
	}
	floatingTexts.push_back(floating);
}

void BattleScreen::playSound(const std::string & path)
{
	if (path.empty() || sounds.empty())
		return;

	auto it = soundBuffers.find(path);
	if (it == soundBuffers.end())
	{
		sf::SoundBuffer buffer;
		if (!buffer.loadFromFile(path))
			return;
		it = soundBuffers.insert(std::make_pair(path, buffer)).first;
	}

	for (sf::Sound & sound : sounds)
	{
		if (sound.getStatus() != sf::Sound::Playing)
		{
			sound.setBuffer(it->second);
			sound.play();
			return;
		}
	}
}

sf::String BattleScreen::fighterName(int fighterId) const
{
	const battle::Fighter * fighter = shown.findFighter(fighterId);
	return fighter != NULL ? fromServerText(fighter->name) : sf::String("?");
}

std::vector<tw::BaseCharacterModel*> BattleScreen::getAliveCharactersInZone(std::vector<tw::Point2D> zone)
{
	return std::vector<tw::BaseCharacterModel*>();
}
