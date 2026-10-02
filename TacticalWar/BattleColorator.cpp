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

	if (impact.count(cell) > 0)
		return sf::Color(255, 70, 60);
	if (castable.count(cell) > 0)
		return sf::Color(70, 170, 255);
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

void BattleColorator::clearPreview()
{
	reachable.clear();
	path.clear();
	castable.clear();
	impact.clear();
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

void BattleColorator::setCastable(const std::vector<Cell> & cells)
{
	castable = toSet(cells);
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
