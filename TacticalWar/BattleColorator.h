#pragma once

#include <map>
#include <set>
#include <vector>
#include <CellColorator.h>
#include <BattleState.h>

// Couleur des cases pendant le combat : placement, déplacement, sorts, glyphes.
class BattleColorator : public tw::CellColorator
{
public:
	virtual sf::Color getColorForCell(tw::CellData * cell);
	// Visée d'un sort (portée, cases ciblables, zone d'impact, case survolée) : par-dessus les cases.
	virtual sf::Color getOverlayForCell(tw::CellData * cell);

	void clearPreview();
	void setStartCells(const std::vector<tw::battle::Cell> & team1, const std::vector<tw::battle::Cell> & team2);
	void setReachable(const std::vector<tw::battle::Cell> & cells);
	void setPath(const std::vector<tw::battle::Cell> & cells, bool truncated);
	// Visée d'un sort : sa portée (forme et distance, sans la ligne de vue), les cases où il peut
	// être lancé, et la case survolée (cible valable ou non).
	void setRange(const std::vector<tw::battle::Cell> & cells);
	void setCastable(const std::vector<tw::battle::Cell> & cells);
	void setHovered(const tw::battle::Cell & cell, bool valid);
	// Cases qu'un combattant survolé pourra atteindre à son prochain tour (orange : ennemi, turquoise : allié).
	void setThreat(const std::vector<tw::battle::Cell> & cells, bool enemy);
	void setImpact(const std::vector<tw::battle::Cell> & cells);
	void setGlyphs(const std::vector<tw::battle::Glyph> & glyphs, int viewerTeam);
	// Zone à tenir : voile doré sous tout le reste.
	void setZone(const std::vector<tw::battle::Cell> & cells);

	bool isReachable(const tw::battle::Cell & cell) const { return reachable.count(cell) > 0; }
	bool isInRange(const tw::battle::Cell & cell) const { return range.count(cell) > 0; }
	bool isCastable(const tw::battle::Cell & cell) const { return castable.count(cell) > 0; }
	bool hasCastable() const { return !castable.empty(); }

private:
	std::set<tw::battle::Cell> startTeam1;
	std::set<tw::battle::Cell> startTeam2;
	std::set<tw::battle::Cell> reachable;
	std::set<tw::battle::Cell> path;
	bool pathTruncated = false;
	std::set<tw::battle::Cell> range;
	std::set<tw::battle::Cell> castable;
	tw::battle::Cell hovered = { -1, -1 };
	bool hoveredValid = false;
	std::set<tw::battle::Cell> threat;
	bool threatEnemy = true;
	std::set<tw::battle::Cell> impact;
	std::map<tw::battle::Cell, sf::Color> glyphColors;
	std::set<tw::battle::Cell> zone;
};
