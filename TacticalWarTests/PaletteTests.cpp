#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <string>

#include <Palette.h>

using namespace tw::palette;

namespace
{
	// Simulation de la deutéranopie et de la protanopie (Machado, Oliveira et Fernandes, 2009, sévérité
	// maximale), sur les composantes linéaires.
	const double DEUTERANOPIA[3][3] = { { 0.367322, 0.860646, -0.227968 }, { 0.280085, 0.672501, 0.047413 }, { -0.011820, 0.042940, 0.968881 } };
	const double PROTANOPIA[3][3] = { { 0.152286, 1.052583, -0.204868 }, { 0.114503, 0.786281, 0.099216 }, { -0.003882, -0.048116, 1.051998 } };

	double linear(int component)
	{
		double c = component / 255.0;
		return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
	}

	struct Lab
	{
		double l;
		double a;
		double b;
	};

	Lab toLab(const double rgb[3])
	{
		double x = 0.4124 * rgb[0] + 0.3576 * rgb[1] + 0.1805 * rgb[2];
		double y = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
		double z = 0.0193 * rgb[0] + 0.1192 * rgb[1] + 0.9505 * rgb[2];
		auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116; };
		double fx = f(x / 0.95047);
		double fy = f(y);
		double fz = f(z / 1.08883);
		return { 116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz) };
	}

	Lab seenAs(const Rgba & color, const double (*matrix)[3])
	{
		double rgb[3] = { linear(color.r), linear(color.g), linear(color.b) };
		double seen[3];
		for (int i = 0; i < 3; i++)
		{
			seen[i] = matrix == nullptr ? rgb[i] : matrix[i][0] * rgb[0] + matrix[i][1] * rgb[1] + matrix[i][2] * rgb[2];
			seen[i] = std::max(0.0, std::min(1.0, seen[i]));
		}
		return toLab(seen);
	}

	// Différence de couleur CIE76 entre deux rôles, vue par une personne daltonienne (matrice) ou non.
	double difference(Role first, Role second, bool colorblindSet, const double (*matrix)[3])
	{
		Lab a = seenAs(color(first, colorblindSet), matrix);
		Lab b = seenAs(color(second, colorblindSet), matrix);
		return std::sqrt((a.l - b.l) * (a.l - b.l) + (a.a - b.a) * (a.a - b.a) + (a.b - b.b) * (a.b - b.b));
	}
}

TEST_CASE("The colorblind palette keeps key pairs distinct for deuteranopia and protanopia")
{
	struct Pair
	{
		Role first;
		Role second;
	};
	const Pair pairs[] = {
		{ Role::TEAM1_ARMOR, Role::TEAM2_ARMOR },
		{ Role::TEAM1_PANEL, Role::TEAM2_PANEL },
		{ Role::TEAM1_START, Role::TEAM2_START },
		{ Role::REACHABLE, Role::THREAT_ENEMY },
		{ Role::IMPACT, Role::CASTABLE },
		{ Role::GLYPH_ALLY, Role::GLYPH_ENEMY },
		{ Role::PATH, Role::PATH_TRUNCATED },
	};
	for (const Pair & pair : pairs)
	{
		CAPTURE((int)pair.first);
		CHECK(difference(pair.first, pair.second, true, nullptr) >= 50);
		CHECK(difference(pair.first, pair.second, true, DEUTERANOPIA) >= 40);
		CHECK(difference(pair.first, pair.second, true, PROTANOPIA) >= 40);
		// Jamais moins lisible que le jeu standard pour une personne daltonienne.
		CHECK(difference(pair.first, pair.second, true, DEUTERANOPIA) + 1 >= std::min(60.0, difference(pair.first, pair.second, false, DEUTERANOPIA)));
	}

	// Le jeu standard confond déplacement et menace ennemie : c'est ce que le mode daltonien corrige.
	CHECK(difference(Role::REACHABLE, Role::THREAT_ENEMY, false, DEUTERANOPIA) < 40);
	CHECK(difference(Role::REACHABLE, Role::THREAT_ENEMY, true, DEUTERANOPIA) > 60);
}

TEST_CASE("The palette switches sets, team colors and color names together")
{
	setColorblind(false);
	int armor[3];
	teamArmor(2, armor);
	CHECK(armor[0] == 120);
	CHECK(std::string(teamPlayers(2)) == "rouges");
	CHECK(std::string(name(Role::THREAT_ALLY)) == "turquoise");
	CHECK(teamRole(2, Role::TEAM1_PANEL) == Role::TEAM2_PANEL);
	CHECK(teamRole(1, Role::TEAM1_PANEL) == Role::TEAM1_PANEL);

	setColorblind(true);
	teamArmor(2, armor);
	CHECK(armor[0] == 230);
	CHECK(armor[1] == 159);
	CHECK(std::string(teamPlayers(2)) == "orange");
	CHECK(std::string(name(Role::THREAT_ALLY)) == "bleu ciel");
	CHECK(color(Role::IMPACT).r == 213);
	setColorblind(false);
}
