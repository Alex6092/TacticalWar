#pragma once

#include <BaseCharacterModel.h>

// Protecteur : personnage affiché par le client. Les caractéristiques et les sorts de la classe sont
// définis dans assets/data/gamedata.json et appliqués par le serveur.
class Protecteur : public tw::BaseCharacterModel
{
public:
	Protecteur(tw::Environment * environment, int teamId, int currentX, int currentY)
		: BaseCharacterModel(environment, teamId, currentX, currentY)
	{
	}

	virtual int getClassId()
	{
		return 3;
	}

	virtual std::string getGraphicsPath()
	{
		return "./assets/Protecteur/";
	}
};
