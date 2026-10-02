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

	void clearPreview();
	void setStartCells(const std::vector<tw::battle::Cell> & team1, const std::vector<tw::battle::Cell> & team2);
	void setReachable(const std::vector<tw::battle::Cell> & cells);
	void setPath(const std::vector<tw::battle::Cell> & cells, bool truncated);
	void setCastable(const std::vector<tw::battle::Cell> & cells);
	void setImpact(const std::vector<tw::battle::Cell> & cells);
	void setGlyphs(const std::vector<tw::battle::Glyph> & glyphs, int viewerTeam);

	bool isReachable(const tw::battle::Cell & cell) const { return reachable.count(cell) > 0; }
	bool isCastable(const tw::battle::Cell & cell) const { return castable.count(cell) > 0; }
	bool hasCastable() const { return !castable.empty(); }

private:
	std::set<tw::battle::Cell> startTeam1;
	std::set<tw::battle::Cell> startTeam2;
	std::set<tw::battle::Cell> reachable;
	std::set<tw::battle::Cell> path;
	bool pathTruncated = false;
	std::set<tw::battle::Cell> castable;
	std::set<tw::battle::Cell> impact;
	std::map<tw::battle::Cell, sf::Color> glyphColors;
};
