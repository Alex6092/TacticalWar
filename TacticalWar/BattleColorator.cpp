#include "BattleColorator.h"

using tw::battle::Cell;

namespace
{
	std::set<Cell> toSet(const std::vector<Cell> & cells)
	{
		return std::set<Cell>(cells.begin(), cells.end());
	}
}

sf::Color BattleColorator::getColorForCell(tw::CellData * data)
{
	Cell cell = { data->getX(), data->getY() };

	// La visée d'un sort est dessinée par-dessus la case (getOverlayForCell) : la tuile reste claire.
	if (cell == hovered || impact.count(cell) > 0 || castable.count(cell) > 0 || range.count(cell) > 0)
		return sf::Color::White;
	if (path.count(cell) > 0)
		return pathTruncated ? sf::Color(255, 160, 40) : sf::Color(80, 230, 90);
	if (reachable.count(cell) > 0)
		return sf::Color(150, 240, 150);
	if (startTeam1.count(cell) > 0)
		return sf::Color(70, 140, 255);
	if (startTeam2.count(cell) > 0)
		return sf::Color(255, 90, 80);

	auto glyph = glyphColors.find(cell);
	if (glyph != glyphColors.end())
		return glyph->second;

	return sf::Color::White;
}

sf::Color BattleColorator::getOverlayForCell(tw::CellData * data)
{
	Cell cell = { data->getX(), data->getY() };

	if (cell == hovered)
		return hoveredValid ? sf::Color(255, 205, 40, 175) : sf::Color(45, 45, 55, 150);
	if (impact.count(cell) > 0)
		return sf::Color(255, 60, 40, 140);
	if (castable.count(cell) > 0)
		return sf::Color(40, 130, 255, 140);
	if (range.count(cell) > 0)
		return sf::Color(150, 205, 255, 75);
	if (threat.count(cell) > 0)
		return threatEnemy ? sf::Color(255, 140, 40, 85) : sf::Color(60, 200, 230, 85);
	if (zone.count(cell) > 0)
		return sf::Color(255, 200, 40, 95);
	return sf::Color::Transparent;
}

void BattleColorator::setZone(const std::vector<Cell> & cells)
{
	zone = toSet(cells);
}

void BattleColorator::clearPreview()
{
	reachable.clear();
	path.clear();
	range.clear();
	castable.clear();
	impact.clear();
	hovered = { -1, -1 };
	threat.clear();
}

void BattleColorator::setStartCells(const std::vector<Cell> & team1, const std::vector<Cell> & team2)
{
	startTeam1 = toSet(team1);
	startTeam2 = toSet(team2);
}

void BattleColorator::setReachable(const std::vector<Cell> & cells)
{
	reachable = toSet(cells);
}

void BattleColorator::setPath(const std::vector<Cell> & cells, bool truncated)
{
	path = toSet(cells);
	pathTruncated = truncated;
}

void BattleColorator::setRange(const std::vector<Cell> & cells)
{
	range = toSet(cells);
}

void BattleColorator::setCastable(const std::vector<Cell> & cells)
{
	castable = toSet(cells);
}

void BattleColorator::setHovered(const Cell & cell, bool valid)
{
	hovered = cell;
	hoveredValid = valid;
}

void BattleColorator::setThreat(const std::vector<Cell> & cells, bool enemy)
{
	threat = toSet(cells);
	threatEnemy = enemy;
}

void BattleColorator::setImpact(const std::vector<Cell> & cells)
{
	impact = toSet(cells);
}

void BattleColorator::setGlyphs(const std::vector<tw::battle::Glyph> & glyphs, int viewerTeam)
{
	glyphColors.clear();
	for (const tw::battle::Glyph & glyph : glyphs)
	{
		// Glyphe allié en cyan, ennemi en violet.
		sf::Color color = glyph.team == viewerTeam ? sf::Color(120, 230, 230) : sf::Color(200, 120, 255);
		for (const Cell & cell : glyph.cells)
			glyphColors[cell] = color;
	}
}
