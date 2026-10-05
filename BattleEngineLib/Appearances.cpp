#include "Appearances.h"
#include "Achievements.h"

#include <algorithm>
#include <cmath>

using namespace tw::battle;

bool tw::battle::isUnlocked(const AppearanceDef & appearance, const PlayerProgress & progress)
{
	if (!appearance.unlockAchievement.empty() && progress.achievements.count(appearance.unlockAchievement) == 0)
		return false;
	return progress.wins >= appearance.unlockWins && progress.mvp >= appearance.unlockMvp
		&& (int)progress.puzzles.size() >= appearance.unlockPuzzles;
}

std::vector<std::string> tw::battle::unlockedAppearances(const GameData & data, const PlayerProgress & progress)
{
	std::vector<std::string> unlocked;
	for (const AppearanceDef & appearance : data.appearances)
	{
		if (isUnlocked(appearance, progress))
			unlocked.push_back(appearance.id);
	}
	return unlocked;
}

std::string tw::battle::allowedAppearance(const GameData & data, const PlayerProgress & progress, const std::string & wanted)
{
	const AppearanceDef * appearance = data.findAppearance(wanted);
	return appearance != nullptr && isUnlocked(*appearance, progress) ? wanted : std::string();
}

std::string tw::battle::unlockCondition(const AppearanceDef & appearance)
{
	std::vector<std::string> parts;
	if (!appearance.unlockAchievement.empty())
	{
		const AchievementDef * achievement = findAchievement(appearance.unlockAchievement);
		parts.push_back(std::string(u8"haut fait « ") + (achievement != nullptr ? achievement->name : appearance.unlockAchievement.c_str()) + u8" »");
	}
	if (appearance.unlockWins > 0)
		parts.push_back(std::to_string(appearance.unlockWins) + (appearance.unlockWins > 1 ? " victoires" : " victoire"));
	if (appearance.unlockMvp > 0)
		parts.push_back(appearance.unlockMvp > 1 ? std::to_string(appearance.unlockMvp) + " titres de MVP" : std::string("un titre de MVP"));
	if (appearance.unlockPuzzles > 0)
		parts.push_back(std::to_string(appearance.unlockPuzzles) + (appearance.unlockPuzzles > 1 ? u8" énigmes réussies" : u8" énigme réussie"));
	if (parts.empty())
		return u8"disponible dès le départ";
	std::string text;
	for (std::size_t i = 0; i < parts.size(); i++)
		text += (i == 0 ? "" : " et ") + parts[i];
	return text;
}

void tw::battle::armorColor(const AppearanceDef * appearance, const int team[3], int out[3])
{
	for (int i = 0; i < 3; i++)
		out[i] = team[i];
	if (appearance == nullptr)
		return;

	// Teinte, saturation, valeur de la couleur d'équipe ; la teinte est gardée.
	double r = team[0] / 255.0;
	double g = team[1] / 255.0;
	double b = team[2] / 255.0;
	double max = std::max(r, std::max(g, b));
	double min = std::min(r, std::min(g, b));
	double delta = max - min;
	double hue = 0;
	if (delta > 0)
	{
		if (max == r)
			hue = std::fmod((g - b) / delta, 6.0);
		else if (max == g)
			hue = (b - r) / delta + 2;
		else
			hue = (r - g) / delta + 4;
		hue *= 60;
		if (hue < 0)
			hue += 360;
	}
	double saturation = max > 0 ? delta / max : 0;
	double value = max;

	saturation = std::max(0.0, std::min(1.0, saturation * appearance->armorSaturation));
	value = std::max(0.0, std::min(1.0, value * appearance->armorLight));

	double c = value * saturation;
	double x = c * (1 - std::fabs(std::fmod(hue / 60.0, 2.0) - 1));
	double m = value - c;
	double rgb[3] = { 0, 0, 0 };
	int sector = (int)(hue / 60.0) % 6;
	switch (sector)
	{
	case 0: rgb[0] = c; rgb[1] = x; break;
	case 1: rgb[0] = x; rgb[1] = c; break;
	case 2: rgb[1] = c; rgb[2] = x; break;
	case 3: rgb[1] = x; rgb[2] = c; break;
	case 4: rgb[0] = x; rgb[2] = c; break;
	default: rgb[0] = c; rgb[2] = x; break;
	}
	for (int i = 0; i < 3; i++)
		out[i] = std::max(0, std::min(255, (int)std::lround((rgb[i] + m) * 255)));
}
