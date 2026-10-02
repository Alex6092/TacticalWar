#include "EditorController.h"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <Environment.h>
#include <EnvironmentManager.h>
#include <TileRegistry.h>

using namespace tw;
using namespace tw::editor;

namespace
{
	const int NEIGHBOURS[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
}

EditorController::EditorController()
	: tool(Tool::PAINT), tile(TileRegistry::LEGACY_GROUND), active(false),
	rectX0(0), rectY0(0), rectX1(0), rectY1(0), nextRevision(1), savedRevision(0)
{
	newMap(15, 15, 1, TileRegistry::LEGACY_GROUND);
}

EditorController::~EditorController()
{
}

void EditorController::newMap(int width, int height, int id, const std::string & fillTile)
{
	width = std::max(MIN_SIZE, std::min(MAX_SIZE, width));
	height = std::max(MIN_SIZE, std::min(MAX_SIZE, height));
	setEnvironment(new Environment(width, height, id, fillTile));
}

void EditorController::setEnvironment(Environment * value)
{
	environment.reset(value);
	undoStack.clear();
	redoStack.clear();
	active = false;
	current = EditStep();
	savedRevision = currentRevision();
}

CellState EditorController::stateOf(int x, int y) const
{
	CellState state;
	CellData * cell = environment->getMapData(x, y);
	if (cell != nullptr)
	{
		state.tile = cell->getDisplayTile();
		state.start = cell->getTeamStartPointNumber();
	}
	return state;
}

void EditorController::applyState(int x, int y, const CellState & state)
{
	CellData * cell = environment->getMapData(x, y);
	if (cell == nullptr)
		return;
	environment->setTile(x, y, state.tile);
	cell->setTeamStartPoint(state.start);
}

void EditorController::change(int x, int y, const CellState & after)
{
	if (environment->getMapData(x, y) == nullptr)
		return;

	CellState before = stateOf(x, y);
	if (before == after)
		return;

	// Une case modifiée plusieurs fois dans la même action garde son état d'origine.
	auto it = std::find_if(current.changes.begin(), current.changes.end(),
		[x, y](const CellChange & c) { return c.x == x && c.y == y; });
	if (it != current.changes.end())
	{
		it->after = after;
	}
	else
	{
		CellChange cellChange;
		cellChange.x = x;
		cellChange.y = y;
		cellChange.before = before;
		cellChange.after = after;
		current.changes.push_back(cellChange);
	}
	applyState(x, y, after);
}

void EditorController::commitStep()
{
	bool empty = current.changes.empty() && current.mapBefore.empty();
	if (!empty)
	{
		current.revision = nextRevision++;
		undoStack.push_back(current);
		redoStack.clear();
	}
	current = EditStep();
}

int EditorController::currentRevision() const
{
	return undoStack.empty() ? 0 : undoStack.back().revision;
}

bool EditorController::isModified() const
{
	return currentRevision() != savedRevision;
}

void EditorController::markSaved()
{
	savedRevision = currentRevision();
}

void EditorController::applyTool(int x, int y)
{
	CellState state = stateOf(x, y);
	switch (tool)
	{
	case Tool::PAINT:
		state.tile = tile;
		change(x, y, state);
		break;
	case Tool::START_TEAM1:
		state.start = 1;
		change(x, y, state);
		break;
	case Tool::START_TEAM2:
		state.start = 2;
		change(x, y, state);
		break;
	case Tool::ERASE_START:
		state.start = 0;
		change(x, y, state);
		break;
	default:
		break;
	}
}

void EditorController::fill(int x, int y)
{
	// Remplissage des cases voisines (4 directions) ayant la même tuile.
	std::string target = stateOf(x, y).tile;
	if (target == tile)
		return;

	int width = environment->getWidth();
	int height = environment->getHeight();
	std::vector<bool> seen(width * height, false);
	std::deque<std::pair<int, int>> queue;
	queue.push_back({ x, y });
	seen[x * height + y] = true;

	while (!queue.empty())
	{
		std::pair<int, int> cell = queue.front();
		queue.pop_front();

		CellState state = stateOf(cell.first, cell.second);
		state.tile = tile;
		change(cell.first, cell.second, state);

		for (const int * offset : NEIGHBOURS)
		{
			int nx = cell.first + offset[0];
			int ny = cell.second + offset[1];
			if (nx < 0 || ny < 0 || nx >= width || ny >= height || seen[nx * height + ny])
				continue;
			seen[nx * height + ny] = true;
			if (stateOf(nx, ny).tile == target)
				queue.push_back({ nx, ny });
		}
	}
}

void EditorController::pointerDown(int x, int y)
{
	if (environment->getMapData(x, y) == nullptr)
		return;

	active = true;
	current = EditStep();
	rectX0 = rectX1 = x;
	rectY0 = rectY1 = y;

	if (tool == Tool::FILL)
	{
		fill(x, y);
		active = false;
		commitStep();
	}
	else if (tool != Tool::RECTANGLE)
	{
		applyTool(x, y);
	}
}

void EditorController::pointerMove(int x, int y)
{
	if (environment->getMapData(x, y) == nullptr)
		return;

	if (!active)
	{
		// Bouton enfoncé hors de la carte puis glissé dessus : le trait commence ici.
		if (tool != Tool::FILL)
			pointerDown(x, y);
		return;
	}

	rectX1 = x;
	rectY1 = y;
	if (tool != Tool::RECTANGLE && tool != Tool::FILL)
		applyTool(x, y);
}

void EditorController::pointerUp()
{
	if (!active)
		return;
	active = false;

	if (tool == Tool::RECTANGLE)
	{
		for (int x = std::min(rectX0, rectX1); x <= std::max(rectX0, rectX1); x++)
		{
			for (int y = std::min(rectY0, rectY1); y <= std::max(rectY0, rectY1); y++)
			{
				CellState state = stateOf(x, y);
				state.tile = tile;
				change(x, y, state);
			}
		}
	}
	commitStep();
}

bool EditorController::getRectangle(int & x0, int & y0, int & x1, int & y1) const
{
	if (!active || tool != Tool::RECTANGLE)
		return false;
	x0 = std::min(rectX0, rectX1);
	x1 = std::max(rectX0, rectX1);
	y0 = std::min(rectY0, rectY1);
	y1 = std::max(rectY0, rectY1);
	return true;
}

void EditorController::restoreMap(const std::string & json)
{
	Environment * restored = EnvironmentManager::fromJson(json);
	if (restored != nullptr)
		environment.reset(restored);
}

bool EditorController::undo()
{
	if (active)
		pointerUp();
	if (undoStack.empty())
		return false;

	EditStep step = undoStack.back();
	undoStack.pop_back();

	if (!step.mapBefore.empty())
	{
		restoreMap(step.mapBefore);
	}
	else
	{
		for (auto it = step.changes.rbegin(); it != step.changes.rend(); ++it)
			applyState(it->x, it->y, it->before);
	}
	redoStack.push_back(step);
	return true;
}

bool EditorController::redo()
{
	if (active)
		pointerUp();
	if (redoStack.empty())
		return false;

	EditStep step = redoStack.back();
	redoStack.pop_back();

	if (!step.mapAfter.empty())
	{
		restoreMap(step.mapAfter);
	}
	else
	{
		for (const CellChange & cellChange : step.changes)
			applyState(cellChange.x, cellChange.y, cellChange.after);
	}
	undoStack.push_back(step);
	return true;
}

void EditorController::resize(int width, int height, const std::string & fillTile)
{
	width = std::max(MIN_SIZE, std::min(MAX_SIZE, width));
	height = std::max(MIN_SIZE, std::min(MAX_SIZE, height));
	if (width == environment->getWidth() && height == environment->getHeight())
		return;

	if (active)
		pointerUp();

	Environment * resized = new Environment(width, height, environment->getId(), fillTile);
	resized->setName(environment->getName());
	resized->setInTournamentPool(environment->isInTournamentPool());
	for (int x = 0; x < std::min(width, environment->getWidth()); x++)
	{
		for (int y = 0; y < std::min(height, environment->getHeight()); y++)
		{
			CellData * cell = environment->getMapData(x, y);
			resized->setTile(x, y, cell->getDisplayTile());
			resized->getMapData(x, y)->setTeamStartPoint(cell->getTeamStartPointNumber());
		}
	}

	current = EditStep();
	current.mapBefore = EnvironmentManager::toJson(environment.get());
	current.mapAfter = EnvironmentManager::toJson(resized);
	environment.reset(resized);
	commitStep();
}

std::vector<ValidationMessage> EditorController::validate() const
{
	std::vector<ValidationMessage> messages;
	int width = environment->getWidth();
	int height = environment->getHeight();

	auto walkable = [this](int x, int y) {
		CellData * cell = environment->getMapData(x, y);
		return cell != nullptr && cell->getIsWalkable() && !cell->getIsObstacle();
	};

	std::vector<std::pair<int, int>> starts[3];
	for (int x = 0; x < width; x++)
	{
		for (int y = 0; y < height; y++)
		{
			int team = environment->getMapData(x, y)->getTeamStartPointNumber();
			if (team != 1 && team != 2)
				continue;

			if (!walkable(x, y))
			{
				ValidationMessage message;
				message.blocking = true;
				message.text = "Départ de l'équipe " + std::to_string(team) + " sur une case non praticable ("
					+ std::to_string(x) + ", " + std::to_string(y) + ").";
				message.x = x;
				message.y = y;
				messages.push_back(message);
				continue;
			}
			starts[team].push_back({ x, y });
		}
	}

	for (int team = 1; team <= 2; team++)
	{
		if (starts[team].size() < 2)
		{
			ValidationMessage message;
			message.blocking = true;
			message.text = "L'équipe " + std::to_string(team) + " a besoin d'au moins 2 cases de départ praticables ("
				+ std::to_string(starts[team].size()) + " actuellement).";
			messages.push_back(message);
		}
	}

	// Cases praticables accessibles depuis chaque zone de départ.
	auto reachableFrom = [&](const std::vector<std::pair<int, int>> & origins) {
		std::vector<bool> reached(width * height, false);
		std::deque<std::pair<int, int>> queue;
		for (const auto & origin : origins)
		{
			reached[origin.first * height + origin.second] = true;
			queue.push_back(origin);
		}
		while (!queue.empty())
		{
			std::pair<int, int> cell = queue.front();
			queue.pop_front();
			for (const int * offset : NEIGHBOURS)
			{
				int nx = cell.first + offset[0];
				int ny = cell.second + offset[1];
				if (nx < 0 || ny < 0 || nx >= width || ny >= height || reached[nx * height + ny] || !walkable(nx, ny))
					continue;
				reached[nx * height + ny] = true;
				queue.push_back({ nx, ny });
			}
		}
		return reached;
	};

	if (!starts[1].empty() && !starts[2].empty())
	{
		std::vector<bool> fromTeam1 = reachableFrom(starts[1]);
		bool connected = std::any_of(starts[2].begin(), starts[2].end(),
			[&](const std::pair<int, int> & cell) { return fromTeam1[cell.first * height + cell.second]; });
		if (!connected)
		{
			ValidationMessage message;
			message.blocking = true;
			message.text = "Aucun chemin ne relie les départs des deux équipes.";
			messages.push_back(message);
		}

		std::vector<std::pair<int, int>> allStarts = starts[1];
		allStarts.insert(allStarts.end(), starts[2].begin(), starts[2].end());
		std::vector<bool> reached = reachableFrom(allStarts);
		int islands = 0;
		int firstX = -1;
		int firstY = -1;
		for (int x = 0; x < width; x++)
		{
			for (int y = 0; y < height; y++)
			{
				if (walkable(x, y) && !reached[x * height + y])
				{
					if (islands == 0)
					{
						firstX = x;
						firstY = y;
					}
					islands++;
				}
			}
		}
		if (islands > 0)
		{
			ValidationMessage message;
			message.text = std::to_string(islands) + " case(s) praticable(s) inaccessible(s) depuis les départs (îlots), par exemple ("
				+ std::to_string(firstX) + ", " + std::to_string(firstY) + ").";
			message.x = firstX;
			message.y = firstY;
			messages.push_back(message);
		}
	}

	if (environment->getName().empty())
	{
		ValidationMessage message;
		message.text = "La carte n'a pas de nom.";
		messages.push_back(message);
	}

	return messages;
}

std::vector<std::string> EditorController::mapDirectories()
{
	namespace fs = std::filesystem;
	std::vector<std::string> directories;
	directories.push_back(EnvironmentManager::getInstance()->getMapDirectory());

	// Dépôt : premier dossier parent qui contient TacticalWar.sln et assets/map.
	std::error_code code;
	fs::path dir = fs::current_path(code);
	for (int depth = 0; depth < 6 && !code && !dir.empty(); depth++)
	{
		if (fs::exists(dir / "TacticalWar.sln", code) && fs::is_directory(dir / "assets" / "map", code))
		{
			fs::path repoMaps = fs::weakly_canonical(dir / "assets" / "map", code);
			fs::path gameMaps = fs::weakly_canonical(fs::path(directories[0]), code);
			if (repoMaps != gameMaps)
				directories.push_back(repoMaps.string() + "/");
			break;
		}
		if (dir == dir.parent_path())
			break;
		dir = dir.parent_path();
	}
	return directories;
}
