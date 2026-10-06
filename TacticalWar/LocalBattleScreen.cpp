#include "LocalBattleScreen.h"

using namespace tw;
using nlohmann::json;

LocalBattleScreen::LocalBattleScreen(tgui::Gui * gui, int environmentId)
	: BattleScreen(gui, environmentId, Mode::PLAYER), nowMs(0), hideEnd(false), timers(false), botDelay(0.7f),
	botRng(std::random_device()()), botWait(0), botTurn(-1), botActions(0)
{
}

void LocalBattleScreen::startLocal(std::unique_ptr<battle::BattleEngine> newEngine, int viewer)
{
	engine = std::move(newEngine);
	botTurn = -1;
	// Les événements produits jusqu'ici (placement…) sont déjà dans l'état complet.
	if (engine->hasPendingEvents())
		engine->flushEvents();
	onMessageReceived("BI" + engine->snapshot(viewer, nowMs).dump());
}

bool LocalBattleScreen::deliver()
{
	if (!engine || !engine->hasPendingEvents())
		return false;

	json batch = engine->flushEvents();
	json & events = batch["ev"];
	bool ended = false;
	for (auto it = events.begin(); it != events.end();)
	{
		if (it->value("t", std::string()) == "end")
		{
			ended = true;
			if (hideEnd)
			{
				it = events.erase(it);
				continue;
			}
		}
		++it;
	}

	onMessageReceived("BV" + batch.dump());
	if (ended)
		onLocalEnd();
	return ended;
}

bool LocalBattleScreen::idle() const
{
	return visualQueue.empty() && stepRemaining <= 0 && !waitingMove;
}

void LocalBattleScreen::sendToServer(const std::string & op, const json & body)
{
	if (!engine)
		return;

	// Signal d'équipe : les coéquipiers sont joués par l'IA, il n'est montré qu'au joueur.
	if (op == "CG")
	{
		onMessageReceived("BG" + json({ { "f", you }, { "x", body.value("x", -1) }, { "y", body.value("y", -1) }, { "kind", body.value("kind", 0) } }).dump());
		return;
	}
	// Resynchronisation : état complet du moteur local.
	if (op == "BR")
	{
		deliver();
		onMessageReceived("BI" + engine->snapshot(you, nowMs).dump());
		return;
	}

	battle::ActionResult result = battle::ActionResult::failure("Action inconnue.");
	if (op == "CE")
	{
		result = engine->emote(you, body.value("id", -1), nowMs);
	}
	else
	{
		onPlayerAction();
		if (op == "CL")
		{
			result = engine->cast(actor(), body.value("slot", -1), { body.value("x", 0), body.value("y", 0) }, nowMs);
		}
		else if (op == "Cm")
		{
			std::vector<battle::Cell> path;
			for (const json & cell : body.value("path", json::array()))
				path.push_back({ cell.at(0).get<int>(), cell.at(1).get<int>() });
			result = engine->move(actor(), path, nowMs);
		}
		else if (op == "Ct")
		{
			result = engine->endTurn(actor(), nowMs);
		}
		else if (op == "CP")
		{
			result = engine->place(you, { body.value("x", 0), body.value("y", 0) }, nowMs);
		}
		else if (op == "Cs")
		{
			result = engine->setReady(you, body.value("ready", true), nowMs);
		}
	}

	if (!result.ok)
		onMessageReceived("ER" + json({ { "op", op }, { "message", result.error } }).dump());
	deliver();
}

void LocalBattleScreen::update(float deltatime)
{
	nowMs += (std::int64_t)(deltatime * 1000);
	if (engine)
	{
		if (timers)
		{
			engine->tick(nowMs);
			deliver();
		}
		playBots(deltatime);
	}

	BattleScreen::update(deltatime);
}

void LocalBattleScreen::playBots(float deltatime)
{
	// L'IA joue une action à la fois, une fois les animations précédentes terminées. Sa décision est
	// calculée en tâche de fond dès que possible : pendant les animations et le délai entre deux
	// actions, qui masquent le calcul ; l'image n'attend jamais.
	const battle::BattleState & state = engine->getState();
	if (state.phase != battle::BattlePhase::FIGHT)
		return;
	int active = state.activeFighterId();
	if (bots.count(active) == 0)
		return;

	int turn = state.round * 100 + state.turnIndex;
	if (turn != botTurn)
	{
		botTurn = turn;
		botActions = 0;
		botWait = botDelay;
	}
	// Au plus 12 actions par tour (sécurité contre une IA qui tournerait en rond), puis fin du tour.
	AsyncBotDecision::Key key = { active, turn, botActions };
	if (botActions < 12 && !botDecision.requested(key))
		botDecision.request(key, state, engine->getMap(), engine->getData(), botRng(), botOptions);

	if (!idle())
		return;
	botWait -= deltatime;
	if (botWait > 0)
		return;
	battle::BotAction action;
	if (botActions < 12 && !botDecision.take(key, action))
		return;
	botActions++;
	botWait = botDelay;

	battle::ActionResult result = battle::ActionResult::failure("");
	if (action.kind == battle::BotAction::Kind::CAST)
		result = engine->cast(active, action.slot, action.target, nowMs);
	else if (action.kind == battle::BotAction::Kind::MOVE)
		result = engine->move(active, action.path, nowMs);
	if (!result.ok)
		engine->endTurn(active, nowMs);
	deliver();
}
