#pragma once

#include <string>
#include <vector>

namespace tw
{
	// Apparences du joueur côté client : celles débloquées sur son compte (message PA du serveur,
	// gardées dans client.json pour l'entraînement hors ligne) et celles des énigmes réussies sur ce poste.
	std::vector<std::string> availableAppearances();
	// Apparence choisie (client.json) si elle est disponible, sinon la première (classique).
	std::string chosenAppearance();
	// Couleurs d'armure et de cheveux d'une apparence pour une équipe (1 ou 2). Apparence inconnue :
	// couleurs d'équipe.
	void appearanceColors(const std::string & appearance, int team, int armor[3], int hair[3]);
	// PA : apparences débloquées, choix retenu par le serveur, et celles débloquées à l'instant.
	void applyAppearanceMessage(const std::string & json);
	// Apparences débloquées depuis le dernier appel (à annoncer), vidées par l'appel.
	std::vector<std::string> takeFreshAppearances();
	// Nom d'une apparence (UTF-8), son identifiant si elle est inconnue.
	std::string appearanceName(const std::string & id);
}
