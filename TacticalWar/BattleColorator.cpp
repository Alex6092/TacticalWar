#include "BattleColorator.h"

#include <Palette.h>

using tw::battle::Cell;
using tw::palette::Role;

namespace
{
	std::set<Cell> toSet(const std::vector<Cell> & cells)
	{
		return std::set<Cell>(cells.begin(), cells.end());
	}

	sf::Color paletteColor(Role role)
	{
		tw::palette::Rgba color = tw::palette::color(role);
		return sf::Color(color.r, color.g, color.b, color.a);
	}
}

sf::Color BattleColorator::getColorForCell(tw::CellData * data)
{
	Cell cell = { data->getX(), data->getY() };

	// La visée d'un sort est dessinée par-dessus la case (getOverlayForCell) : la tuile reste claire.
	if (cell == hovered || impact.count(cell) > 0 || castable.count(cell) > 0 || range.count(cell) > 0)
		return sf::Color::White;
	if (path.count(cell) > 0)
		return paletteColor(pathTruncated ? Role::PATH_TRUNCATED : Role::PATH);
	if (reachable.count(cell) > 0)
		return paletteColor(Role::REACHABLE);
	if (startTeam1.count(cell) > 0)
		return paletteColor(Role::TEAM1_START);
	if (startTeam2.count(cell) > 0)
		return paletteColor(Role::TEAM2_START);

	auto glyph = glyphColors.find(cell);
	if (glyph != glyphColors.end())
		return glyph->second;

	return sf::Color::White;
}

sf::Color BattleColorator::getOverlayForCell(tw::CellData * data)
{
	Cell cell = { data->getX(), data->getY() };

	if (cell == hovered)
		return paletteColor(hoveredValid ? Role::HOVER_VALID : Role::HOVER_INVALID);
	if (impact.count(cell) > 0)
		return paletteColor(Role::IMPACT);
	if (castable.count(cell) > 0)
		return paletteColor(Role::CASTABLE);
	if (range.count(cell) > 0)
		return paletteColor(Role::RANGE);
	if (threat.count(cell) > 0)
		return paletteColor(threatEnemy ? Role::THREAT_ENEMY : Role::THREAT_ALLY);
	if (zone.count(cell) > 0)
		return paletteColor(Role::ZONE);
	return sf::Color::Transparent;
}

bool BattleColorator::isHatched(tw::CellData * data)
{
	return tw::palette::colorblind() && impact.count({ data->getX(), data->getY() }) > 0;
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
		// Glyphe allié en cyan, ennemi en violet (bleu ciel et vermillon en mode daltonien).
		sf::Color color = paletteColor(glyph.team == viewerTeam ? Role::GLYPH_ALLY : Role::GLYPH_ENEMY);
		for (const Cell & cell : glyph.cells)
			glyphColors[cell] = color;
	}
}
