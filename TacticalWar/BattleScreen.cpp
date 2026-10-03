#include "BattleScreen.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <BattleMirror.h>
#include <BattleRules.h>
#include <CharacterFactory.h>
#include <Emotes.h>
#include <EnvironmentManager.h>
#include <EnvironmentMap.h>
#include <Message.h>

#include "AdminScreen.h"
#include "BattleEventView.h"
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

	// Cases touchées par un sort lancé sur "target" : sa zone d'effet, et celle des glyphes qu'il pose
	// (même calcul que le moteur).
	std::vector<battle::Cell> previewImpact(const battle::BattleMap & map, const battle::Cell & caster, const battle::Cell & target,
		const battle::SpellDef & spell)
	{
		std::vector<battle::Cell> cells = battle::impactCells(map, caster, target, spell.impact);
		for (const battle::EffectDef & effect : spell.effects)
		{
			if (effect.type != battle::EffectType::GLYPH)
				continue;
			for (const battle::Cell & cell : battle::impactCells(map, caster, target, { effect.glyphShape, effect.glyphSize }))
				cells.push_back(cell);
		}
		return cells;
	}

	// Fourchette « 12 » ou « 12 à 15 ».
	sf::String spanText(int a, int b)
	{
		int low = std::min(a, b);
		int high = std::max(a, b);
		return low == high ? num(low) : num(low) + L" à " + num(high);
	}

	sf::String reasonLabel(battle::EndReason reason)
	{
		switch (reason)
		{
		case battle::EndReason::KO: return L"Toute l'équipe adverse est hors combat.";
		case battle::EndReason::ROUND_LIMIT: return L"Limite de tours atteinte : décision aux points de vie.";
		case battle::EndReason::FORFEIT: return L"Victoire par forfait.";
		case battle::EndReason::ADMIN: return L"Combat arrêté par l'organisateur : décision aux points de vie.";
		case battle::EndReason::OBJECTIVE: return L"L'équipe a tenu la zone jusqu'au score demandé.";
		default: return L"";
		}
	}
}

BattleScreen::BattleScreen(tgui::Gui * gui, int environmentId, Mode mode)
	: gui(gui), window(NULL), mode(mode), autoCloseRemaining(-1), cameraFitted(false), you(-1), lastSeq(0), hasSnapshot(false), awaitingServer(false),
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
	eventView.reset(new BattleEventView(*this));

	std::string fxError;
	if (!fx.loadCatalog(BattleFx::CATALOG_PATH, &fxError))
		std::cout << fxError << std::endl;
	fx.positionOf = [this](int fighterId, sf::Vector2f & cell) {
		BaseCharacterModel * view = viewOf(fighterId);
		if (view == NULL)
			return false;
		cell = sf::Vector2f(view->getInterpolatedX(), view->getInterpolatedY());
		return true;
	};
	fx.playSound = [this](const std::string & path) { playSound(path); };
	hud->onSpellClicked = [this](int slot) { selectSpell(selectedSpell == slot ? -1 : slot); };
	hud->onEndTurn = [this]() { sendAction("Ct", json::object()); };
	hud->onReady = [this](bool ready) { sendToServer("Cs", { { "ready", ready } }); };
	hud->onEmote = [this](int id) { sendEmote(id); };
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

	// Première image : toute la carte tient dans la fenêtre.
	if (!cameraFitted)
	{
		camera.fit(environment->getWidth(), environment->getHeight(), window->getSize(), 1.0f);
		cameraFitted = true;
	}
}

void BattleScreen::update(float deltatime)
{
	Screen::update(deltatime);
	hud->update(deltatime);

	for (auto & entry : views)
		entry.second->update(deltatime);

	processVisuals(deltatime);

	// Fin des animations d'action : le personnage reprend l'animation de repos ou de course.
	for (auto it = actionAnimations.begin(); it != actionAnimations.end();)
	{
		it->second -= deltatime;
		if (it->second <= 0)
		{
			BaseCharacterModel * view = viewOf(it->first);
			if (view != NULL && pendingDeaths.find(it->first) == pendingDeaths.end())
				view->resetAnimation();
			it = actionAnimations.erase(it);
		}
		else
		{
			it++;
		}
	}

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
	for (SpeechBubble & bubble : bubbles)
		bubble.age += deltatime;
	bubbles.erase(std::remove_if(bubbles.begin(), bubbles.end(), [](const SpeechBubble & bubble) { return bubble.age > 2.6f; }), bubbles.end());
	emoteCooldown = std::max(0.f, emoteCooldown - deltatime);
	pingCooldown = std::max(0.f, pingCooldown - deltatime);
	floatingTexts.erase(std::remove_if(floatingTexts.begin(), floatingTexts.end(),
		[](const FloatingText & text) { return text.age > 1.4f; }), floatingTexts.end());

	fx.update(deltatime);

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
	fx.collectViews(effects);

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

	drawAimPreview(window);
	drawBubbles(window);
}

void BattleScreen::drawBubbles(sf::RenderWindow * window)
{
	// Bulles des émotes, à gauche de la tête du personnage (l'aperçu des sorts est à droite, et
	// au-dessus des PV elles passeraient sous les panneaux du haut) ; elles s'effacent à la fin.
	for (const SpeechBubble & bubble : bubbles)
	{
		BaseCharacterModel * view = viewOf(bubble.fighterId);
		if (view == NULL)
			continue;

		float alpha = std::max(0.f, std::min(1.f, (2.6f - bubble.age) / 0.4f));
		sf::Text text(bubble.text, font, 18);
		text.setFillColor(sf::Color(30, 30, 45, (sf::Uint8)(255 * alpha)));
		sf::FloatRect bounds = text.getLocalBounds();

		float headX = (view->getInterpolatedX() - view->getInterpolatedY()) * 60.f + 60.f - 34.f;
		float headY = (view->getInterpolatedX() + view->getInterpolatedY()) * 30.f + 30.f - 100.f;
		float width = bounds.width + 22;
		float height = bounds.height + 16;
		float left = headX - 12 - width;
		float top = headY - height / 2;

		sf::RectangleShape box(sf::Vector2f(width, height));
		box.setPosition(std::round(left), std::round(top));
		box.setFillColor(sf::Color(255, 255, 255, (sf::Uint8)(235 * alpha)));
		box.setOutlineColor(sf::Color(40, 40, 60, (sf::Uint8)(255 * alpha)));
		box.setOutlineThickness(2);
		window->draw(box);

		sf::ConvexShape tail(3);
		tail.setPoint(0, sf::Vector2f(left + width - 1, headY - 8));
		tail.setPoint(1, sf::Vector2f(left + width - 1, headY + 8));
		tail.setPoint(2, sf::Vector2f(headX, headY));
		tail.setFillColor(box.getFillColor());
		window->draw(tail);

		text.setPosition(std::round(left + 11 - bounds.left), std::round(top + 8 - bounds.top));
		window->draw(text);
	}
}

void BattleScreen::drawAimPreview(sf::RenderWindow * window)
{
	// À droite de chaque combattant touché par le sort visé, à hauteur de tête : dégâts, soins,
	// bouclier et effets (au-dessus des PV, ils passeraient sous les panneaux du haut de l'écran).
	for (const battle::TargetPreview & preview : aimPreviews)
	{
		BaseCharacterModel * view = viewOf(preview.fighterId);
		if (view == NULL)
			continue;

		std::vector<std::pair<sf::String, sf::Color>> lines;
		if (preview.koCertain)
			lines.push_back({ L"KO !", sf::Color(255, 215, 60) });
		else if (preview.koPossible)
			lines.push_back({ L"KO possible", sf::Color(255, 175, 60) });
		if (preview.maxDamage > 0)
			lines.push_back({ L"-" + spanText(preview.minDamage, preview.maxDamage), sf::Color(255, 95, 80) });
		if (preview.maxHeal > 0)
			lines.push_back({ L"+" + spanText(preview.minHeal, preview.maxHeal), sf::Color(110, 255, 110) });
		if (preview.maxShield > 0)
			lines.push_back({ L"Bouclier +" + spanText(preview.minShield, preview.maxShield), sf::Color(150, 210, 255) });
		for (const std::string & note : preview.notes)
			lines.push_back({ fromServerText(note), sf::Color(235, 235, 235) });

		float x = (view->getInterpolatedX() - view->getInterpolatedY()) * 60.f + 60.f + 52.f;
		float y = (view->getInterpolatedX() + view->getInterpolatedY()) * 30.f + 30.f - 100.f;
		for (auto line = lines.begin(); line != lines.end(); ++line)
		{
			sf::Text text(line->first, font, line->second == sf::Color(235, 235, 235) ? 15 : 19);
			text.setFillColor(line->second);
			text.setOutlineColor(sf::Color::Black);
			text.setOutlineThickness(2);
			sf::FloatRect bounds = text.getLocalBounds();

			sf::RectangleShape back(sf::Vector2f(bounds.width + 14, bounds.height + 8));
			back.setPosition(std::round(x - 7), std::round(y - 4));
			back.setFillColor(sf::Color(15, 15, 25, 175));
			window->draw(back);

			text.setPosition(std::round(x - bounds.left), std::round(y - bounds.top));
			window->draw(text);
			y += bounds.height + 10;
		}
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
			sendToServer("BR", json::object());
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
	else if (message.op == "BG")
	{
		json ping;
		if (hasSnapshot && message.parseJson(ping))
			showPing(ping.value("f", -1), { ping.value("x", -1), ping.value("y", -1) });
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
	actionAnimations.clear();

	for (const battle::Fighter & fighter : truth.fighters)
		syncView(fighter);
	// Effets durables et glyphes déjà en place (spectateur arrivé en cours de combat, resynchronisation).
	fx.rebuild(truth);

	if (truth.phase == battle::BattlePhase::PLACEMENT)
		colorator->setStartCells(map.startCells[1], map.startCells[2]);
	else
		colorator->setStartCells({}, {});
	colorator->setZone(truth.zone.cells);

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
		view = CharacterFactory::getInstance()->constructCharacter(environment, fighter.classId, fighter.team, fighter.position.x, fighter.position.y);
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
	return eventView->play(event, fast);
}

void BattleScreen::startActionAnimation(int fighterId, BaseCharacterModel * view, tw::Animation animation)
{
	if (animation == tw::Animation::ATTACK1)
		view->startAttack1Animation(ACTION_ANIMATION_SECONDS);
	else if (animation == tw::Animation::ATTACK2)
		view->startAttack2Animation(ACTION_ANIMATION_SECONDS);
	else if (animation == tw::Animation::TAKE_DAMAGE)
		view->startTakeDmg(ACTION_ANIMATION_SECONDS);
	actionAnimations[fighterId] = ACTION_ANIMATION_SECONDS;
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
	int winnerCount = 0;
	for (const battle::Fighter & fighter : shown.fighters)
	{
		if (fighter.team == shown.winnerTeam)
		{
			winners += (winners.isEmpty() ? sf::String() : sf::String(L" et ")) + fromServerText(fighter.name);
			winnerCount++;
		}
	}

	sf::String reason = reasonLabel(shown.endReason);
	if (shown.zone.enabled && (shown.endReason == battle::EndReason::ROUND_LIMIT || shown.endReason == battle::EndReason::ADMIN))
		reason = L"Décision aux points de la zone, puis aux points de vie.";
	if (shown.zone.enabled && me != NULL)
		reason += L"\nZone : votre équipe " + num(shown.zone.scores[me->team]) + L" - " + num(shown.zone.scores[3 - me->team]) + L" adversaires";
	else if (shown.zone.enabled)
		reason += L"\nZone : " + teamLabel(1) + L" " + num(shown.zone.scores[1]) + L" - " + num(shown.zone.scores[2]) + L" " + teamLabel(2);
	sf::String details = winners + (winnerCount > 1 ? L" remportent" : L" remporte") + L" le combat.\n" + reason
		+ L"\nTours joués : " + num(shown.round);

	// Bilan de chaque combattant, l'équipe gagnante d'abord.
	std::vector<BattleHud::EndRow> rows;
	for (int pass = 0; pass < 2; pass++)
	{
		for (const battle::Fighter & fighter : shown.fighters)
		{
			if ((fighter.team == shown.winnerTeam) != (pass == 0))
				continue;
			const battle::ClassDef * classDef = ClientGameData::get().data().findClass(fighter.classId);
			BattleHud::EndRow row;
			row.name = fromServerText(fighter.name) + (classDef != nullptr ? L" (" + fromServerText(classDef->name) + L")" : sf::String());
			row.team = fighter.team;
			row.mvp = fighter.id == shown.mvpFighterId;
			row.dealt = fighter.record.dealt;
			row.healed = fighter.record.healed;
			row.shielded = fighter.record.shielded;
			row.kills = fighter.record.kills;
			rows.push_back(row);
			if (row.mvp)
				hud->log(L"MVP du combat : " + row.name, sf::Color(255, 215, 70));
		}
	}
	hud->showEnd(title, details, victory || me == NULL, rows);
	hud->log(title, victory ? sf::Color(120, 255, 120) : sf::Color(255, 120, 120));

	if (mode == Mode::SPECTATOR && SpectatorModeScreen::isDirectorMode())
		autoCloseRemaining = 10.f;
}

std::string BattleScreen::periodicSpell(const battle::Fighter & target, int sourceId, battle::EffectType type) const
{
	// Sort de l'effet périodique (poison, brûlure…) posé par ce lanceur sur la cible.
	for (const battle::ActiveEffect & effect : target.effects)
	{
		if (effect.type == type && effect.casterId == sourceId)
			return effect.spellId;
	}
	for (const battle::ActiveEffect & effect : target.effects)
	{
		if (effect.type == type)
			return effect.spellId;
	}
	return std::string();
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
	sendToServer(op, body);
}

void BattleScreen::sendToServer(const std::string & op, const json & body)
{
	LinkToServer::getInstance()->SendRaw(op + body.dump());
}

void BattleScreen::sendEmote(int emoteId)
{
	const battle::Fighter * me = truth.findFighter(you);
	if (mode != Mode::PLAYER || !hasSnapshot || me == NULL || truth.phase == battle::BattlePhase::ENDED
		|| emoteId < 0 || emoteId >= battle::EMOTE_COUNT)
		return;
	if (emoteCooldown > 0)
	{
		hud->showMessage(L"Attendez un peu avant la prochaine émote", sf::Color(200, 200, 200), 1.f);
		return;
	}
	emoteCooldown = battle::EMOTE_COOLDOWN_MS / 1000.f;
	sendToServer("CE", { { "id", emoteId } });
}

void BattleScreen::sendPing(const battle::Cell & cell)
{
	// Seuls les coéquipiers voient le signal (le serveur le relaie à l'équipe uniquement).
	if (mode != Mode::PLAYER || !hasSnapshot || !map.contains(cell) || truth.phase == battle::BattlePhase::ENDED || pingCooldown > 0)
		return;
	pingCooldown = 1.f;
	sendToServer("CG", { { "x", cell.x }, { "y", cell.y } });
}

void BattleScreen::showPing(int fighterId, const battle::Cell & cell)
{
	if (!map.contains(cell))
		return;
	sf::Vector2f position((float)cell.x, (float)cell.y);
	fx.playEffect("ping", position);
	fx.playEffect("ping_arrow", position);
	playSound("./assets/sound/ui/ping.ogg");

	const battle::Fighter * target = shown.fighterAt(cell);
	sf::String text = fighterName(fighterId) + (target != NULL ? L" désigne " + fromServerText(target->name) : sf::String(L" signale une case"));
	hud->log(text, sf::Color(255, 215, 70));
}

void BattleScreen::refreshPreview()
{
	colorator->clearPreview();
	const battle::Fighter * me = truth.findFighter(you);
	colorator->setGlyphs(shown.glyphs, me != NULL ? me->team : 0);
	hud->setHint("");
	const tw::battle::GameData & data = ClientGameData::get().data();

	bool aiming = isInteractive() && me != NULL && selectedSpell >= 0;
	if (!aiming)
	{
		aimPreviews.clear();

		// Combattant survolé : où il pourra aller à son prochain tour (orange pour un ennemi, turquoise
		// pour un allié). Pendant son propre tour, le joueur voit déjà ses déplacements possibles.
		const battle::Fighter * hovered = truth.findFighter(hoveredFighter);
		if (hovered != NULL && hovered->alive && truth.phase == battle::BattlePhase::FIGHT && (hovered->id != you || !isInteractive()))
		{
			bool enemy = me == NULL || hovered->team != me->team;
			colorator->setThreat(battle::nextTurnReach(truth, map, data, *hovered), enemy);
			hud->setHint(fromServerText(hovered->name) + (enemy ? L" : déplacement possible au prochain tour en orange" : L" : déplacement possible au prochain tour en turquoise"));
		}
	}

	if (!isInteractive() || me == NULL)
		return;

	if (selectedSpell >= 0)
	{
		const battle::SpellDef * spell = battle::spellOf(data, *me, selectedSpell);
		if (spell == NULL)
			return;

		// Portée du sort (forme et distance, sans la ligne de vue ni la cible) en bleu clair, et cases
		// où il peut être lancé en bleu : le joueur voit jusqu'où porte le sort, même quand aucune
		// case n'est ciblable d'ici.
		std::vector<battle::Cell> range;
		for (const battle::Cell & cell : battle::launchCells(truth, map, data, *me, *spell))
		{
			if (map.isWalkable(cell) || truth.fighterAt(cell) != NULL)
				range.push_back(cell);
		}
		colorator->setRange(range);
		std::vector<battle::Cell> castable = battle::castableCells(truth, map, data, *me, *spell);
		colorator->setCastable(castable);

		sf::String name = fromServerText(spell->name);
		if (spell->launch == battle::LaunchShape::SELF && !castable.empty())
		{
			// Sort lancé sur soi : sa zone est montrée tout de suite.
			colorator->setImpact(previewImpact(map, me->position, me->position, *spell));
			colorator->setHovered(me->position, true);
			updateAimPreview(*me, me->position);
			hud->setHint(name + L" : cliquez sur votre personnage (Échap pour annuler)");
		}
		else if (colorator->isCastable(hoveredCell))
		{
			colorator->setImpact(previewImpact(map, me->position, hoveredCell, *spell));
			colorator->setHovered(hoveredCell, true);
			updateAimPreview(*me, hoveredCell);
			hud->setHint(name + L" : cliquez pour lancer le sort (Échap pour annuler)");
		}
		else if (colorator->isInRange(hoveredCell))
		{
			aimPreviews.clear();
			// À portée mais pas ciblable : la raison est donnée (ligne de vue, type de cible…).
			colorator->setHovered(hoveredCell, false);
			hud->setHint(name + L" : " + fromServerText(battle::checkTarget(truth, map, data, *me, *spell, hoveredCell)));
		}
		else if (castable.empty())
		{
			aimPreviews.clear();
			hud->setHint(name + L" : aucune case ciblable d'ici, la portée du sort est en bleu clair (Échap pour annuler)");
		}
		else
		{
			aimPreviews.clear();
			hud->setHint(name + L" : cliquez sur une case bleue (Échap pour annuler)");
		}
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

void BattleScreen::updateAimPreview(const battle::Fighter & me, const battle::Cell & cell)
{
	// Le moteur rejoue le sort sur une copie de l'état : seulement quand la visée ou l'état changent.
	if (cell == aimPreviewCell && selectedSpell == aimPreviewSpell && lastSeq == aimPreviewSeq && !aimPreviews.empty())
		return;
	aimPreviews = battle::previewSpell(truth, map, ClientGameData::get().data(), me.id, selectedSpell, cell);
	aimPreviewCell = cell;
	aimPreviewSpell = selectedSpell;
	aimPreviewSeq = lastSeq;
}

void BattleScreen::onCellClicked(int cellX, int cellY)
{
	if (!hasSnapshot || isMouseOverHud())
		return;

	battle::Cell cell = { cellX, cellY };
	const battle::Fighter * me = truth.findFighter(you);
	if (me == NULL)
		return;

	// Alt+clic : signal pour son équipe.
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::LAlt) || sf::Keyboard::isKeyPressed(sf::Keyboard::RAlt))
	{
		sendPing(cell);
		return;
	}

	if (truth.phase == battle::BattlePhase::PLACEMENT)
	{
		const std::vector<battle::Cell> & starts = map.startCells[me->team];
		if (!me->ready && std::find(starts.begin(), starts.end(), cell) != starts.end())
			sendToServer("CP", { { "x", cellX }, { "y", cellY } });
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
		case sf::Keyboard::F1: sendEmote(0); break;
		case sf::Keyboard::F2: sendEmote(1); break;
		case sf::Keyboard::F3: sendEmote(2); break;
		case sf::Keyboard::F4: sendEmote(3); break;
		case sf::Keyboard::F5: sendEmote(4); break;
		case sf::Keyboard::F6: sendEmote(5); break;
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
			if (window != NULL)
				camera.fit(environment->getWidth(), environment->getHeight(), window->getSize(), 1.0f);
			break;
		default: break;
		}
	}

	// Clic molette : signal pour son équipe sur la case survolée.
	if (event->type == sf::Event::MouseButtonPressed && event->mouseButton.button == sf::Mouse::Middle && window != NULL && !isMouseOverHud())
		sendPing(hoveredCell);

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
