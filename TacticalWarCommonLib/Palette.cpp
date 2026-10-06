#include "pch.h"
#include "Palette.h"

using namespace tw::palette;

namespace
{
	bool active = false;

	struct Entry
	{
		Rgba standard;
		Rgba colorblind;
		const char * standardName;
		const char * colorblindName;
	};

	// Dans l'ordre de Role. Jeu daltonien : couleurs d'Okabe et Ito (bleu 0,114,178 ; orange 230,159,0 ;
	// vermillon 213,94,0 ; bleu ciel 86,180,233 ; vert bleuté 0,158,115 ; jaune 240,228,66).
	const Entry ENTRIES[(int)Role::COUNT] = {
		{ { 0, 166, 214, 255 }, { 20, 130, 210, 255 }, "bleue", "bleue" },				// TEAM1_ARMOR
		{ { 120, 17, 17, 255 }, { 230, 159, 0, 255 }, "rouge", "orange" },				// TEAM2_ARMOR
		{ { 40, 80, 170, 210 }, { 0, 90, 160, 210 }, "bleue", "bleue" },				// TEAM1_PANEL
		{ { 160, 40, 40, 210 }, { 190, 105, 0, 210 }, "rouge", "orange" },				// TEAM2_PANEL
		{ { 150, 200, 255, 255 }, { 120, 190, 255, 255 }, "bleue", "bleue" },			// TEAM1_TEXT
		{ { 255, 160, 150, 255 }, { 255, 190, 90, 255 }, "rouge", "orange" },			// TEAM2_TEXT
		{ { 70, 140, 255, 255 }, { 86, 160, 233, 255 }, "bleue", "bleue" },				// TEAM1_START
		{ { 255, 90, 80, 255 }, { 240, 170, 40, 255 }, "rouge", "orange" },				// TEAM2_START
		{ { 150, 240, 150, 255 }, { 90, 200, 160, 255 }, "verte", "verte" },			// REACHABLE
		{ { 80, 230, 90, 255 }, { 0, 170, 130, 255 }, "verte", "verte" },				// PATH
		{ { 255, 160, 40, 255 }, { 240, 228, 66, 255 }, "orange", "jaune" },			// PATH_TRUNCATED
		{ { 255, 205, 40, 175 }, { 240, 228, 66, 175 }, "jaune", "jaune" },				// HOVER_VALID
		{ { 45, 45, 55, 150 }, { 45, 45, 55, 150 }, "grise", "grise" },					// HOVER_INVALID
		{ { 255, 60, 40, 140 }, { 213, 94, 0, 150 }, "rouge", "vermillon" },			// IMPACT
		{ { 40, 130, 255, 140 }, { 0, 114, 178, 150 }, "bleue", "bleue" },				// CASTABLE
		{ { 150, 205, 255, 75 }, { 86, 180, 233, 80 }, "bleu clair", "bleu clair" },	// RANGE
		{ { 255, 140, 40, 85 }, { 230, 159, 0, 95 }, "orange", "orange" },				// THREAT_ENEMY
		{ { 60, 200, 230, 85 }, { 86, 180, 233, 90 }, "turquoise", "bleu ciel" },		// THREAT_ALLY
		{ { 255, 200, 40, 95 }, { 240, 228, 66, 95 }, "jaune", "jaune" },				// ZONE
		{ { 120, 230, 230, 255 }, { 86, 180, 233, 255 }, "cyan", "bleu ciel" },			// GLYPH_ALLY
		{ { 200, 120, 255, 255 }, { 213, 94, 0, 255 }, "violette", "vermillon" },		// GLYPH_ENEMY
		{ { 255, 80, 70, 255 }, { 255, 130, 40, 255 }, "rouge", "orange" },				// DAMAGE_TEXT
		{ { 110, 255, 110, 255 }, { 90, 220, 170, 255 }, "verte", "vert bleuté" },		// HEAL_TEXT
	};

	const Entry & entry(Role role)
	{
		int index = (int)role;
		return ENTRIES[index >= 0 && index < (int)Role::COUNT ? index : 0];
	}
}

void tw::palette::setColorblind(bool enabled)
{
	active = enabled;
}

bool tw::palette::colorblind()
{
	return active;
}

Rgba tw::palette::color(Role role)
{
	return color(role, active);
}

Rgba tw::palette::color(Role role, bool colorblind)
{
	return colorblind ? entry(role).colorblind : entry(role).standard;
}

Role tw::palette::teamRole(int team, Role team1Role)
{
	// Les rôles d'équipe vont par paires : équipe 1, puis équipe 2.
	return team == 2 ? (Role)((int)team1Role + 1) : team1Role;
}

const char * tw::palette::name(Role role)
{
	return active ? entry(role).colorblindName : entry(role).standardName;
}

const char * tw::palette::teamPlayers(int team)
{
	if (team != 2)
		return "bleus";
	return active ? "orange" : "rouges";
}

void tw::palette::teamArmor(int team, int out[3])
{
	Rgba armor = team == 1 || team == 2 ? color(teamRole(team, Role::TEAM1_ARMOR)) : Rgba{ 255, 255, 255, 255 };
	out[0] = armor.r;
	out[1] = armor.g;
	out[2] = armor.b;
}

void tw::palette::defaultHair(int out[3])
{
	out[0] = 108;
	out[1] = 70;
	out[2] = 35;
}

void tw::palette::skin(int out[3])
{
	out[0] = 202;
	out[1] = 165;
	out[2] = 150;
}
